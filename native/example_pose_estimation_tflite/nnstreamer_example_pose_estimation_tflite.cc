/**
 * @file    nnstreamer_example_pose_estimation_tflite.cc
 * @date    19 February 2020
 * @brief   Tensor Stream example with TensorFlow Lite model for pose estimation
 * @author  Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 * @bug     No known bugs.
 * 
 * NNStreamer example for pose estimation on single person using
 * public TensorFlow Lite model in Ubuntu environment.
 * 
 * Get the required model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model.sh pose-estimation-tflite
 * 
 * It downloads the model from this link:
 * https://storage.googleapis.com/download.tensorflow.org/models/tflite/posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite
 * and make text file of key point labels for this model (total 17 key points
 * include nose, left ear, right ankle, etc.)
 * 
 * Run example:
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<path/to/nnstreamer/plugin/path>
 * $ ./nnstreamer_example_pose_estimation_tflite
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <math.h>
#include <cairo.h>

/**
 * @brief Macro for debug mode.
 */
#ifndef DBG
#define DBG FALSE
#endif

/**
 * @brief Macro for debug message.
 */
#define _print_log(...) if (DBG) g_message (__VA_ARGS__)

/**
 * @brief Macro to check error case.
 */
#define _check_cond_err(cond) \
  do { \
    if (!(cond)) { \
      _print_log ("app failed! [line : %d]", __LINE__); \
      goto error; \
    } \
  } while (0)

/**
 * @brief Macro for calculating sigmoid
 */
#define _sigmoid(x) \
    (1.f / (1.f + expf (-x)))

#define VIDEO_HEIGHT        480
#define VIDEO_WIDTH         640

#define MODEL_INPUT_HEIGHT  257
#define MODEL_INPUT_WIDTH   257

#define KEYPOINT_SIZE       17
#define OUTPUT_STRIDE       32
#define GRID_YSIZE          9
#define GRID_XSIZE          9

#define SCORE_THRESHOLD     0.7

typedef struct
{
  gchar *label;
  gfloat y;
  gfloat x;
  gfloat score;
} KeyPoint;

typedef struct
{
  gboolean valid;
  GstVideoInfo vinfo;
} CairoOverlayState;

/**
 * @brief Data structure for tflite model and label info.
 */
typedef struct
{
  gchar *model_path;
  gchar *label_path;
  GList *labels;
} tflite_info_s;

/**
 * @brief Data structure for app.
 */
typedef struct
{
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */

  gboolean running; /**< true when app is running */
  guint received; /**< received buffer count */
  GMutex mutex; /**< mutex for processing */
  tflite_info_s tflite_info; /**< tflite models info */
  KeyPoint kps[KEYPOINT_SIZE];
  CairoOverlayState overlay_state;
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Check and load tflite models.
 */
static gboolean
_tflite_init_info (tflite_info_s * tflite_info, const gchar * path)
{
  const gchar tflite_model[] =
      "posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite";
  const gchar tflite_label[] = "key_point_labels.txt";

  g_return_val_if_fail (tflite_info != NULL, FALSE);

  tflite_info->model_path = NULL;
  tflite_info->label_path = NULL;
  tflite_info->labels = NULL;

  /* check model file can be loaded */
  tflite_info->model_path = g_strdup_printf ("%s/%s", path, tflite_model);

  if (access (tflite_info->model_path, F_OK) != 0) {
    g_critical ("cannot find model file [%s]", tflite_info->model_path);
    return FALSE;
  }

  /* check label file can be loaded */
  tflite_info->label_path = g_strdup_printf ("%s/%s", path, tflite_label);

  if (access (tflite_info->label_path, F_OK) != 0) {
    g_critical ("cannot find label file [%s]", tflite_info->label_path);
    return FALSE;
  }

  /* load labels */
  FILE *fp = fopen (tflite_info->label_path, "r");
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  gchar *label;

  while ((read = getline (&line, &len, fp)) != -1) {
    line[read - 1] = '\0'; /**< trim the trailing newline chracter */
    label = g_strdup ((gchar *) line);
    tflite_info->labels = g_list_append (tflite_info->labels, label);
  }

  if (line) {
    free (line);
  }

  fclose (fp);

  _print_log ("Finished loading labels, total %d", g_list_length (tflite_info->labels));

  return TRUE;
}

/**
 * @brief Get label string with given index.
 */
static gchar *
_tflite_get_label (tflite_info_s * tflite_info, guint index)
{
  guint length;

  g_return_val_if_fail (tflite_info != NULL, NULL);
  g_return_val_if_fail (tflite_info->labels != NULL, NULL);

  length = g_list_length (tflite_info->labels);
  g_return_val_if_fail (index >= 0 && index < length, NULL);

  return (gchar *) g_list_nth_data (tflite_info->labels, index);
}

/**
 * @brief Free data in tflite info structure.
 */
static void
_tflite_free_info (tflite_info_s * tflite_info)
{
  g_return_if_fail (tflite_info != NULL);

  if (tflite_info->model_path) {
    g_free (tflite_info->model_path);
    tflite_info->model_path = NULL;
  }

  if (tflite_info->label_path) {
    g_free (tflite_info->label_path);
    tflite_info->label_path = NULL;
  }

  if (tflite_info->labels) {
    g_list_free_full (tflite_info->labels, g_free);
    tflite_info->labels = NULL;
  }
}

/**
 * @brief Free resource in app data.
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
_parse_err_message (GstMessage * message)
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
_parse_qos_message (GstMessage * message)
{
  GstFormat format;
  guint64 processed;
  guint64 dropped;

  gst_message_parse_qos_stats (message, &format, &processed, &dropped);
  _print_log ("format[%d] processed[%" G_GUINT64_FORMAT "] dropped[%"
      G_GUINT64_FORMAT "]", format, processed, dropped);
}

/**
 * @brief Callback for message.
 */
static void
_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
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
 * @brief Callback for tensor sink signal.
 */
static void
_new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  /* print progress */
  g_app.received++;
  _print_log ("receiving new data [%d]", g_app.received);

  GstMemory *mem_heatmap, *mem_offset;
  GstMapInfo info_heatmap, info_offset;
  gfloat *heatmap, *offset;

  g_return_if_fail (g_app.running);

  /**
   * tensor type is float32.
   * [0] dim of heatmap := KEY_POINT_NUMBER : height_after_stride : 
   *      width_after_stride: 1 (17:9:9:1)
   * [1] dim of offsets := 34 (concat of y-axis and x-axis of offset vector) :
   *      hegiht_after_stride : width_after_stride : 1 (34:9:9:1)
   * [2] dim of displacement forward (not used for single person pose estimation)
   * [3] dim of displacement backward (not used for single person pose estimation)
   */ 

  g_assert (gst_buffer_n_memory (buffer) == 4);

  mem_heatmap = gst_buffer_get_memory (buffer, 0);
  g_assert (gst_memory_map (mem_heatmap, &info_heatmap, GST_MAP_READ));
  g_assert (info_heatmap.size == 17 * 9 * 9 * 1 * 4); // float32 is 4 bytes
  heatmap = (gfloat *) info_heatmap.data; // can be viewed as float_arr[9][9][17]

  mem_offset = gst_buffer_get_memory (buffer, 1);
  g_assert (gst_memory_map (mem_offset, &info_offset, GST_MAP_READ));
  g_assert (info_offset.size == 34 * 9 * 9 * 1 * 4);
  offset = (gfloat *) info_offset.data; // can be viewd as float_arr[9][9][34]

  for (gint keyPointIdx = 0; keyPointIdx < KEYPOINT_SIZE; ++keyPointIdx) {
    gint yPosIdx = -1;
    gint xPosIdx = -1;
    gfloat highestScore = -G_MAXFLOAT;
    gfloat currentScore;

    gfloat yOffset, xOffset;
    gfloat yPosition, xPosition;

    /* find the index of key point with highestScore in 9 X 9 grid */
    for (gint yIdx = 0; yIdx < GRID_YSIZE; ++yIdx) {
      for (gint xIdx = 0; xIdx < GRID_XSIZE; ++xIdx) {
        currentScore = _sigmoid (
              heatmap[(yIdx * GRID_YSIZE + xIdx) * KEYPOINT_SIZE + keyPointIdx]);
        if (currentScore > highestScore) {
          yPosIdx = yIdx;
          xPosIdx = xIdx;
          highestScore = currentScore;
        }
      }
    }

    yOffset =
        offset[(yPosIdx * GRID_YSIZE + xPosIdx) * KEYPOINT_SIZE * 2 + keyPointIdx];
    xOffset =
        offset[(yPosIdx * GRID_YSIZE + xPosIdx) * KEYPOINT_SIZE * 2 + KEYPOINT_SIZE + keyPointIdx];
    
    yPosition = ((gfloat) yPosIdx / (GRID_YSIZE - 1)) * MODEL_INPUT_HEIGHT + yOffset;
    xPosition = ((gfloat) xPosIdx / (GRID_XSIZE - 1)) * MODEL_INPUT_WIDTH + xOffset;

    g_app.kps[keyPointIdx].y = yPosition;
    g_app.kps[keyPointIdx].x = xPosition;
    g_app.kps[keyPointIdx].label = _tflite_get_label (&g_app.tflite_info, keyPointIdx);
    g_app.kps[keyPointIdx].score = highestScore;   
  }

  gst_memory_unmap (mem_heatmap, &info_heatmap);
  gst_memory_unmap (mem_offset, &info_offset);

  gst_memory_unref (mem_heatmap);
  gst_memory_unref (mem_offset);
}

/**
 * @brief Set window title.
 * @param name GstXImageSink element name
 * @param title window title
 */
static void
_set_window_title (const gchar * name, const gchar * title)
{
  GstTagList *tags;
  GstPad *sink_pad;
  GstElement *element;

  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), name);

  g_return_if_fail (element != NULL);

  sink_pad = gst_element_get_static_pad (element, "sink");

  if (sink_pad) {
    tags = gst_tag_list_new (GST_TAG_TITLE, title, NULL);
    gst_pad_send_event (sink_pad, gst_event_new_tag (tags));
    gst_object_unref (sink_pad);
  }

  gst_object_unref (element);
}

/**
 * @brief Store the information from the caps that we are interested in.
 */
static void
prepare_overlay_cb (GstElement * overlay, GstCaps * caps, gpointer user_data)
{
  CairoOverlayState *state = &g_app.overlay_state;

  state->valid = gst_video_info_from_caps (&state->vinfo, caps);
}

/**
 * @brief Draw line between key points
 */
static void
_draw_line (KeyPoint * kpts, cairo_t * cr, gint from, gint to)
{
  if (kpts[from].score < SCORE_THRESHOLD || kpts[to].score < SCORE_THRESHOLD)
    return;

  cairo_move_to (cr, kpts[from].x * VIDEO_WIDTH / MODEL_INPUT_WIDTH,
      kpts[from].y * VIDEO_HEIGHT / MODEL_INPUT_HEIGHT);
  cairo_line_to (cr, kpts[to].x * VIDEO_WIDTH / MODEL_INPUT_WIDTH,
      kpts[to].y * VIDEO_HEIGHT / MODEL_INPUT_HEIGHT);
}

/**
 * @brief Callback to draw the overlay.
 */
static void
draw_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp,
    guint64 duration, gpointer user_data)
{
  CairoOverlayState *state = &g_app.overlay_state;
  gfloat x, y, score;
  // gchar *label;

  g_return_if_fail (state->valid);
  g_return_if_fail (g_app.running);
  KeyPoint *kpts;
  g_mutex_lock (&g_app.mutex);
  kpts = g_app.kps;
  g_assert (kpts);
  g_mutex_unlock (&g_app.mutex);

  /* set font props */
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, 8.0);
  
  for (gint keyPointIdx = 0; keyPointIdx < KEYPOINT_SIZE; ++keyPointIdx) {
    score = kpts[keyPointIdx].score;
    if (score < SCORE_THRESHOLD)
      continue;
    
    y = kpts[keyPointIdx].y * VIDEO_HEIGHT / MODEL_INPUT_HEIGHT;
    x = kpts[keyPointIdx].x * VIDEO_WIDTH / MODEL_INPUT_WIDTH;

    /* draw key point */
    cairo_set_source_rgb (cr, 0.85, 0.2, 0.2);
    cairo_arc (cr, x, y, 3, 0, 2 * M_PI);
    cairo_stroke_preserve (cr);
    cairo_fill (cr);

    /* draw the text of each key point */
    cairo_move_to (cr, x + 5, y + 17);
    cairo_text_path (cr, kpts[keyPointIdx].label);
    cairo_set_source_rgb (cr, 0.7, 0.3, 0.5);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, 0.7, 0.3, 0.5);
    cairo_set_line_width (cr, .2);
    cairo_stroke (cr);
    cairo_fill_preserve (cr);
  }

  /* draw body lines */
  cairo_set_source_rgb (cr, 0.85, 0.2, 0.2);
  cairo_set_line_width (cr, 3);

  _draw_line (kpts, cr, 5, 6);
  _draw_line (kpts, cr, 5, 7);
  _draw_line (kpts, cr, 7, 9);
  _draw_line (kpts, cr, 6, 8);
  _draw_line (kpts, cr, 8, 10);
  _draw_line (kpts, cr, 6, 12);
  _draw_line (kpts, cr, 5, 11);
  _draw_line (kpts, cr, 11, 13);
  _draw_line (kpts, cr, 13, 15);
  _draw_line (kpts, cr, 12, 11);
  _draw_line (kpts, cr, 12, 14);
  _draw_line (kpts, cr, 14, 16);

  cairo_stroke (cr);
}

/**
 * @brief Main function.
 */
int
main (int argc, char ** argv)
{
  const gchar tflite_model_path[] = "./tflite_pose_estimation";

  gchar *str_pipeline;
  gulong handle_id;
  GstElement *element;

  _print_log ("start app..");

  /* init app variable */
  g_app.running = FALSE;
  g_app.received = 0;

  _check_cond_err (_tflite_init_info (&g_app.tflite_info, tflite_model_path));

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("v4l2src name=src ! videoconvert ! videoscale ! "
      "video/x-raw,width=%d,height=%d,format=RGB ! tee name=t_raw "
      "t_raw. ! queue ! videoconvert ! cairooverlay name=tensor_res ! "
      "ximagesink name=img_tensor "
      "t_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! "
      "video/x-raw,width=%d,height=%d,format=RGB ! tensor_converter ! "
      "tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
      "tensor_filter framework=tensorflow-lite model=%s ! "
      "tensor_sink name=tensor_sink",
      VIDEO_WIDTH, VIDEO_HEIGHT, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT,
      g_app.tflite_info.model_path);

  _print_log ("%s\n", str_pipeline);

  /**
   * tensor info (posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite)
   * 
   * input[0] >> type:float32, dim[3:257:257:1], video stream (RGB 257x257)
   * 
   * output[0] >> type:float32, dim[17:9:9:1], heat map of 17 key points
   * output[1] >> type:float32, dim[34:9:9:1], offset vectors
   * output[2] >> type:float32, dim[32:9:9:1], not used
   * output[3] >> type:float32, dim[32:9:9:1], not used
   */

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  handle_id = g_signal_connect (g_app.bus, "message", G_CALLBACK (_message_cb), NULL);
  _check_cond_err (handle_id > 0);

  /* tensor sink signal : new data callback */
  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_sink");
  handle_id = g_signal_connect (element, "new-data", G_CALLBACK (_new_data_cb), NULL);
  gst_object_unref (element);
  _check_cond_err (handle_id > 0);

  /* cairo overlay */
  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_res");
  g_signal_connect (element, "draw", G_CALLBACK (draw_overlay_cb), NULL);
  g_signal_connect (element, "caps-changed", G_CALLBACK (prepare_overlay_cb), NULL);
  gst_object_unref (element);

  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  g_app.running = TRUE;

  /* set window title */
  _set_window_title ("img_tensor",
      "An NNStreamer Example - Single Person Pose Estimation");

  /* run main loop */
  g_main_loop_run (g_app.loop);

  /* quit when received eos or error message */
  g_app.running = FALSE;

  /* video test source element */
  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "src");

  gst_element_set_state (element, GST_STATE_READY);
  gst_element_set_state (g_app.pipeline, GST_STATE_READY);

  g_usleep (200 * 1000);

  gst_element_set_state (element, GST_STATE_NULL);
  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

  g_usleep (200 * 1000);
  gst_object_unref (element);

error:
  _print_log ("close app..");

  _free_app_data ();
  return 0;
}
