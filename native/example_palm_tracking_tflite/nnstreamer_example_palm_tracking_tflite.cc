/**
 * @file  nnstreamer_example_palm_tracking_tflite.cc
 * @date  5 April 2022
 * @brief Tensor stream example with filter
 * @author  Jiho Chu <jiho.chu@samsung.com>
 * @bug   No known bugs.
 *
 * NNStreamer example for palm detection using tensorflow-lite.
 *
 * Pipeline :
 * v4l2src -- tee -- cairooverlay -- videoconvert -- ximagesink
 *             |
 *             --- videoscale -- tensor_filter(tensorflow-lite) -- tensor_filter(custom-easy) -- tensor_sink
 *
 * This app displays video sink.
 *
 * 'tensor_filter(tensorflow-lite)' for palm detection
 * Get model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model.sh palm-detection-lite
 *
 * 'tensor_filter(custom-easy)' updates detection result to display in cairooverlay.
 *
 * Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./nnstreamer_example_palm_tracking_tflite
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cairo-gobject.h>
#include <cairo.h>
#include <cmath>
#include <glib.h>
#include <gst/gst.h>
#include <limits>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#include <nnstreamer/tensor_filter_custom_easy.h>

/**
 * @brief Macro for debug mode.
 */
#ifndef DBG
#define DBG FALSE
#endif

/**
 * @brief Macro for debug message.
 */
#define _print_log(...) \
  if (DBG)              \
  g_message (__VA_ARGS__)

/**
 * @brief Macro to check error case.
 */
#define _check_cond_err(cond)                           \
  do {                                                  \
    if (!(cond)) {                                      \
      _print_log ("app failed! [line : %d]", __LINE__); \
      goto error;                                       \
    }                                                   \
  } while (0)

/**
 * @brief Data structure for tflite model info.
 */
struct TfliteInfo {
  gchar *model_path; /**< tflite model file path */
  gint model_input_width; /**< tflite model input image width */
  gint model_input_height; /**< tflite model input image height */
};


/**
 * @brief bounding box data
 */
typedef struct {
  float xmin;
  float ymin;
  float xmax;
  float ymax;
} bbox_s;

/**
 * @brief anchor data
 */
typedef struct {
  float x_center;
  float y_center;
  float w;
  float h;
} anchor_s;

/**
 * @brief Data structure for app.
 */
struct AppData {
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */

  gboolean running; /**< true when app is running */
  guint received; /**< received buffer count */
  TfliteInfo tflite_info; /**< tflite model info */

  std::vector<anchor_s> anchors;
  GMutex boxes_lock;
  std::queue<bbox_s> boxes;
};

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Free data in tflite info structure.
 */
static void
_tflite_free_info (TfliteInfo *tflite_info)
{
  g_return_if_fail (tflite_info != NULL);

  if (tflite_info->model_path) {
    g_free (tflite_info->model_path);
    tflite_info->model_path = NULL;
  }
}

/**
 * @brief Check tflite model and load labels.
 *
 * This example uses 'palm_detection_lite' for palm detection.
 */
static gboolean
_tflite_init_info (TfliteInfo *tflite_info, const gchar *path)
{
  const gchar tflite_model[] = "palm_detection_lite.tflite";

  g_return_val_if_fail (tflite_info != NULL, FALSE);

  tflite_info->model_path = NULL;

  /* check model file exists */
  tflite_info->model_path = g_strdup_printf ("%s/%s", path, tflite_model);

  if (access (tflite_info->model_path, F_OK) != 0) {
    g_critical ("cannot find tflite model [%s]", tflite_info->model_path);
    return FALSE;
  }

  tflite_info->model_input_width = 192;
  tflite_info->model_input_height = 192;

  return TRUE;
}

/**
 * @brief Free resources in app data.
 */
static void
_free_app_data (void)
{
  if (g_app.loop) {
    g_main_loop_unref (g_app.loop);
    g_app.loop = NULL;
  }

  if (g_app.bus) {
    gst_bus_remove_signal_watch (g_app.bus);
    gst_object_unref (g_app.bus);
    g_app.bus = NULL;
  }

  if (g_app.pipeline) {
    gst_object_unref (g_app.pipeline);
    g_app.pipeline = NULL;
  }

  _tflite_free_info (&g_app.tflite_info);
}

/**
 * @brief Function to print error message.
 */
static void
_parse_err_message (GstMessage *message)
{
  gchar *debug;
  GError *error;

  g_return_if_fail (message != NULL);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &error, &debug);
      break;

    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (message, &error, &debug);
      break;

    default:
      return;
  }

  gst_object_default_error (GST_MESSAGE_SRC (message), error, debug);
  g_error_free (error);
  g_free (debug);
}

/**
 * @brief Function to print qos message.
 */
static void
_parse_qos_message (GstMessage *message)
{
  GstFormat format;
  guint64 processed;
  guint64 dropped;

  gst_message_parse_qos_stats (message, &format, &processed, &dropped);
  _print_log ("format[%d] processed[%" G_GUINT64_FORMAT "] dropped[%" G_GUINT64_FORMAT "]",
      format, processed, dropped);
}

/**
 * @brief Callback for message.
 */
static void
_message_cb (GstBus *bus, GstMessage *message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      _print_log ("received eos message");
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_ERROR:
      _print_log ("received error message");
      _parse_err_message (message);
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_WARNING:
      _print_log ("received warning message");
      _parse_err_message (message);
      break;

    case GST_MESSAGE_STREAM_START:
      _print_log ("received start message");
      break;

    case GST_MESSAGE_QOS:
      _parse_qos_message (message);
      break;

    default:
      break;
  }
}

/**
 * @brief Calculate scale
 */
static float
_calculate_scale (float min_scale, float max_scale, int stride_index, int num_strides)
{
  if (num_strides == 1) {
    return (min_scale + max_scale) * 0.5f;
  } else {
    return min_scale + (max_scale - min_scale) * 1.0 * stride_index / (num_strides - 1.0f);
  }
}

/**
 * @brief Generate anchor information
 */
static void
_gnerate_anchors ()
{
  int layer_id = 0;
  int strides[4] = { 8, 16, 16, 16 };

  while (layer_id < 4) {
    std::vector<float> anchor_height;
    std::vector<float> anchor_width;
    std::vector<float> aspect_ratios;
    std::vector<float> scales;

    int last_same_stride_layer = layer_id;
    while (last_same_stride_layer < 4
           && strides[last_same_stride_layer] == strides[layer_id]) {

      const float scale = _calculate_scale (0.1484375, 0.75, last_same_stride_layer, 4);
      for (int aspect_ratio_id = 0; aspect_ratio_id < 1; ++aspect_ratio_id) {
        aspect_ratios.push_back (1.0f);
        aspect_ratios.push_back (1.0f);
        scales.push_back (scale);
      }
      last_same_stride_layer++;
    }

    for (gsize i = 0; i < aspect_ratios.size (); ++i) {
      const float ratio_sqrts = std::sqrt (aspect_ratios[i]);
      anchor_height.push_back (scales[i] / ratio_sqrts);
      anchor_width.push_back (scales[i] * ratio_sqrts);
    }

    int feature_map_height = 0;
    int feature_map_width = 0;

    const int stride = strides[layer_id];
    feature_map_height = std::ceil (1.0f * 192 / stride);
    feature_map_width = std::ceil (1.0f * 192 / stride);

    for (int y = 0; y < feature_map_height; ++y) {
      for (int x = 0; x < feature_map_width; ++x) {
        for (int anchor_id = 0; anchor_id < (int) anchor_height.size (); ++anchor_id) {
          const float x_center = (x + 0.5) * 1.0f / feature_map_width;
          const float y_center = (y + 0.5) * 1.0f / feature_map_height;

          g_app.anchors.push_back ({ x_center, y_center, 1.0f, 1.0f });
        }
      }
    }
    layer_id = last_same_stride_layer;
  }
}

/**
 * @brief Update box information
 */
static void
_update_box_info (float *mem, gsize box_len, std::vector<bbox_s> &boxes)
{
  float factor = 1.0f;

  for (gsize i = 0; i < box_len; ++i) {
    float y_center = mem[0] / factor;
    float x_center = mem[1] / factor;
    float h = mem[2] / factor;
    float w = mem[3] / factor;

    x_center = x_center / 192 * g_app.anchors[i].w + g_app.anchors[i].x_center;
    y_center = y_center / 192 * g_app.anchors[i].h + g_app.anchors[i].y_center;

    h = h / 192 * g_app.anchors[i].h;
    w = w / 192 * g_app.anchors[i].w;

    const float ymin = y_center - h / 2.f;
    const float xmin = x_center - w / 2.f;
    const float ymax = y_center + h / 2.f;
    const float xmax = x_center + w / 2.f;

    boxes.push_back ({ xmin, ymin, xmax, ymax });
    mem += 18;
  }
}

/**
 * @brief Update detection information
 */
static void
_update_detected (float *mem, gsize box_len, const std::vector<bbox_s> boxes)
{
  float max_score = 0.0f;
  int max_idx = -1;

  for (gsize i = 0; i < box_len; i++) {
    float score = *mem;
    score = score < -100.0f ? -100.0f : score;
    score = score > 100.0f ? 100.0f : score;
    score = 1.0f / (1.0f + std::exp (-score));

    if (score > 0.7f) {
      if (max_score < score) {
        max_score = score;
        max_idx = i;
      }
    }

    mem++;
  }

  if (max_idx != -1) {
    g_mutex_lock (&g_app.boxes_lock);
    if (g_app.boxes.size () < 30) {
      g_app.boxes.push (boxes[max_idx]);
      g_app.boxes.push (boxes[max_idx]);
      g_app.boxes.push (boxes[max_idx]);
    }
    g_mutex_unlock (&g_app.boxes_lock);
  }
}

/**
 * @brief Detect palm information.
 *        It detects a palm from input tensor. It consumes input tensor
 *        and does not transmit any information to the output
 */
static int
_detect_tensor (void *data, const GstTensorFilterProperties *prop,
    const GstTensorMemory *in, GstTensorMemory *out)
{

  g_app.received++;

  _print_log ("Received tensor count: %d", g_app.received);

  unsigned int in_num_tensors = prop->input_meta.num_tensors;
  unsigned int out_num_tensors = prop->output_meta.num_tensors;

  if (in_num_tensors != 2 || out_num_tensors != 2) {
    _print_log ("Invalid tensors: in(%d), out(%d)", in_num_tensors, out_num_tensors);
    return 1;
  }

  if (in[0].size != 2016 * 18 * 4) {
    _print_log ("Invalid input tensor: size(%ld)", in[0].size);
    return 1;
  }

  if (in[1].size != 2016 * 1 * 4) {
    _print_log ("Invalid input tensor: size(%ld)", in[1].size);
    return 1;
  }

  std::vector<bbox_s> boxes;

  _update_box_info ((float *) in[0].data, 2016, boxes);
  _update_detected ((float *) in[1].data, 2016, boxes);

  return 0;
}

/**
 * @brief Callback to draw the overlay.
 *        It draws rectangle from detection information.
 */
static void
draw_overlay_cb (GstElement *overlay, cairo_t *cr, guint64 timestamp,
    guint64 duration, gpointer user_data)
{
  g_return_if_fail (g_app.running);

  if (!g_app.boxes.empty ()) {
    g_mutex_lock (&g_app.boxes_lock);
    bbox_s b = g_app.boxes.front ();
    g_mutex_unlock (&g_app.boxes_lock);

    float x = b.xmin * 640;
    float y = b.ymin * 480;
    float w = (b.xmax - b.xmin) * 640;
    float h = (b.ymax - b.ymin) * 480;

    x = (x > 0) ? ((x > 640) ? 640 : x) : 0;
    y = (y > 0) ? ((y > 480) ? 480 : y) : 0;
    w = (x + w > 640) ? 640 - x : w;
    h = (y + h > 640) ? 480 - y : h;

    _print_log ("DRAW: [%f %f %f %f]", x, y, w, h);

    cairo_rectangle (cr, x, y, w, h);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_set_line_width (cr, 6);
    cairo_stroke (cr);
    cairo_fill_preserve (cr);

    g_mutex_lock (&g_app.boxes_lock);
    g_app.boxes.pop ();
    g_mutex_unlock (&g_app.boxes_lock);
  }
}


/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  const gchar tflite_model_path[] = "./tflite_model";

  gchar *str_pipeline;
  gulong handle_id;
  guint timer_id = 0;
  GstElement *element;
  int ret;

  /* setting tensor_filter custom-easy */
  const GstTensorsInfo info_in = { .num_tensors = 2U,
    .info = {
        { .name = NULL, .type = _NNS_FLOAT32, .dimension = { 18, 1, 2016, 1 } },
        { .name = NULL, .type = _NNS_FLOAT32, .dimension = { 1, 2016, 1, 1 } },
    } };

  const GstTensorsInfo info_out = { .num_tensors = 2U,
    .info = {
        { .name = NULL, .type = _NNS_FLOAT32, .dimension = { 4, 1, 2016, 1 } },
        { .name = NULL, .type = _NNS_FLOAT32, .dimension = { 1, 2016, 1, 1 } },
    } };

  _print_log ("start app..");

  /* init app variable */
  g_app.running = FALSE;
  g_app.received = 0;

  _check_cond_err (_tflite_init_info (&g_app.tflite_info, tflite_model_path));

  _gnerate_anchors ();

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  g_mutex_init (&g_app.boxes_lock);

  /* init pipeline */
  str_pipeline = g_strdup_printf (
      "v4l2src name=cam_src ! videoconvert ! videoscale ! video/x-raw,format=RGB,width=640,height=480,framerate=30/1 ! "
      "tee name=t "
      "t. ! "
      "queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw,format=RGB,width=%d,height=%d ! tensor_converter ! "
      "tensor_transform mode=arithmetic option=typecast:float32,add:0.0,div:255.0 ! "
      "tensor_filter framework=tensorflow-lite model=%s ! "
      "tensor_filter framework=custom-easy model=detect ! tensor_sink "
      "t. ! "
      "queue ! videoconvert ! cairooverlay name=tensor_res ! ximagesink ",
      g_app.tflite_info.model_input_width, g_app.tflite_info.model_input_height,
      g_app.tflite_info.model_path);
  _print_log ("%s\n", str_pipeline);


  ret = NNS_custom_easy_register ("detect", _detect_tensor, NULL, &info_in, &info_out);
  if (ret < 0) {
    _print_log ("detect couldn't be registered: %s", strerror (ret));
  } else {
    _print_log ("detect registered\n");
  }

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  handle_id = g_signal_connect (g_app.bus, "message", (GCallback) _message_cb, NULL);
  _check_cond_err (handle_id > 0);

  /* cairo overlay */
  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_res");
  g_signal_connect (element, "draw", G_CALLBACK (draw_overlay_cb), nullptr);
  gst_object_unref (element);

  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);

  g_app.running = TRUE;

  /* run main loop */
  g_main_loop_run (g_app.loop);

  /* quit when received eos or error message */
  g_app.running = FALSE;

  /* cam source element */
  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "cam_src");

  gst_element_set_state (element, GST_STATE_READY);
  gst_element_set_state (g_app.pipeline, GST_STATE_READY);

  g_usleep (200 * 1000);

  gst_element_set_state (element, GST_STATE_NULL);
  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

  g_usleep (200 * 1000);
  gst_object_unref (element);

error:
  _print_log ("close app..");

  if (timer_id > 0) {
    g_source_remove (timer_id);
  }

  _free_app_data ();
  return 0;
}
