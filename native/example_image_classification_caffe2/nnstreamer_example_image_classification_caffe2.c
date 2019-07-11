/**
 * @file	nnstreamer_example_image_classification_caffe2.c
 * @date	8 July 2019
 * @brief	Tensor stream example with filter
 * @author	Hyoung Joo Ahn <hello.ahn@samsung.com>
 * @bug		No known bugs.
 *
 ** Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./nnstreamer_example_image_classification_caffe2
 *
 * Required model and resources are stored at below link
 * https://github.com/caffe2/models/tree/master/mobilenet_v2
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>

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
  do { \
   if (DBG) g_message (__VA_ARGS__); \
  } while (0)

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


#define VIDEO_WIDTH     640
#define VIDEO_HEIGHT    480

/**
 * @brief Data structure for caffe2 model info.
 */
typedef struct
{
  gchar *init_model_path; /**< caffe2 initialize model file path */
  gchar *pred_model_path; /**< caffe2 predict model file path */
  gchar *label_path; /**< label file path */
  GList *labels; /**< list of loaded labels */
  guint total_labels; /**< count of labels */
} caffe2_info_s;

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
  gint current_label_index; /**< current label index */
  gint new_label_index; /**< new label index */
  caffe2_info_s caffe2_info; /**< caffe2 model info */
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Free data in caffe2 info structure.
 */
static void
_caffe2_free_info (caffe2_info_s * caffe2_info)
{
  g_return_if_fail (caffe2_info != NULL);

  if (caffe2_info->init_model_path) {
    g_free (caffe2_info->init_model_path);
    caffe2_info->init_model_path = NULL;
  }

  if (caffe2_info->pred_model_path) {
    g_free (caffe2_info->pred_model_path);
    caffe2_info->pred_model_path = NULL;
  }

  if (caffe2_info->label_path) {
    g_free (caffe2_info->label_path);
    caffe2_info->label_path = NULL;
  }

  if (caffe2_info->labels) {
    g_list_free_full (caffe2_info->labels, g_free);
    caffe2_info->labels = NULL;
  }
}

/**
 * @brief Check caffe2 model and load labels.
 *
 * This example uses 'Mobilenet_1.0_224_quant' for image classification.
 */
static gboolean
_caffe2_init_info (caffe2_info_s * caffe2_info, const gchar * path)
{
  const gchar caffe2_init_model[] = "init_net.pb";
  const gchar caffe2_pred_model[] = "predict_net.pb";
  const gchar caffe2_label[] = "labels.txt";

  FILE *fp;

  g_return_val_if_fail (caffe2_info != NULL, FALSE);

  caffe2_info->init_model_path = NULL;
  caffe2_info->pred_model_path = NULL;
  caffe2_info->label_path = NULL;
  caffe2_info->labels = NULL;

  /* check model file exists */
  caffe2_info->init_model_path =
      g_strdup_printf ("%s/%s", path, caffe2_init_model);
  caffe2_info->pred_model_path =
      g_strdup_printf ("%s/%s", path, caffe2_pred_model);

  if (access (caffe2_info->init_model_path, F_OK) != 0) {
    _print_log ("cannot find caffe2 init model [%s]",
        caffe2_info->init_model_path);
    return FALSE;
  }

  if (access (caffe2_info->pred_model_path, F_OK) != 0) {
    _print_log ("cannot find caffe2 predict model [%s]",
        caffe2_info->pred_model_path);
    return FALSE;
  }

  /* load labels */
  caffe2_info->label_path = g_strdup_printf ("%s/%s", path, caffe2_label);

  if ((fp = fopen (caffe2_info->label_path, "r")) != NULL) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    gchar *label;

    while ((read = getline (&line, &len, fp)) != -1) {
      label = g_strdup ((gchar *) line);
      caffe2_info->labels = g_list_append (caffe2_info->labels, label);
    }

    if (line) {
      free (line);
    }

    fclose (fp);
  } else {
    _print_log ("cannot find caffe2 label [%s]", caffe2_info->label_path);
    return FALSE;
  }

  caffe2_info->total_labels = g_list_length (caffe2_info->labels);
  _print_log ("finished to load labels, total %d", caffe2_info->total_labels);
  return TRUE;
}

/**
 * @brief Get label string with given index.
 */
static gchar *
_caffe2_get_label (caffe2_info_s * caffe2_info, gint index)
{
  guint length;

  g_return_val_if_fail (caffe2_info != NULL, NULL);
  g_return_val_if_fail (caffe2_info->labels != NULL, NULL);

  length = g_list_length (caffe2_info->labels);
  g_return_val_if_fail (index >= 0 && index < length, NULL);

  return (gchar *) g_list_nth_data (caffe2_info->labels, index);
}

/**
 * @brief Update caffe2 label index with max score.
 * @param scores array of scores
 * @param len array length
 * @return None
 */
static void
_update_top_label_index (float *scores, guint len)
{
  gint i;
  gint index = -1;
  float max_score = 0;

  /* -1 if failed to get max score index */
  g_app.new_label_index = -1;

  g_return_if_fail (scores != NULL);
  g_return_if_fail (len / 4 == g_app.caffe2_info.total_labels);

  for (i = 0; i < len / 4; i++) {
    if (scores[i] > 0 && scores[i] > max_score) {
      index = i;
      max_score = scores[i];
    }
  }
  g_app.new_label_index = index;
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

  _caffe2_free_info (&g_app.caffe2_info);
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

  if (g_app.running) {
    GstMemory *mem;
    GstMapInfo info;
    guint i;
    guint num_mems;

    num_mems = gst_buffer_n_memory (buffer);
    for (i = 0; i < num_mems; i++) {
      mem = gst_buffer_peek_memory (buffer, i);

      if (gst_memory_map (mem, &info, GST_MAP_READ)) {
        /* update label index with max score */
        _update_top_label_index ((float *) info.data, (guint) info.size);

        gst_memory_unmap (mem, &info);
      }
    }
  }
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
 * @brief Timer callback for textoverlay.
 * @return True to ensure the timer continues
 */
static gboolean
_timer_update_result_cb (gpointer user_data)
{
  if (g_app.running) {
    GstElement *overlay;
    gchar *labelWithCode = NULL;
    gchar **labels = NULL;
    gchar **label = NULL;

    if (g_app.current_label_index != g_app.new_label_index) {
      g_app.current_label_index = g_app.new_label_index;

      overlay = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_res");

      labelWithCode =
          _caffe2_get_label (&g_app.caffe2_info, g_app.current_label_index);
      labels = g_strsplit (labelWithCode, " ", 2);
      label = g_strsplit (labels[1], ",", -1);
      g_object_set (overlay, "text", (label != NULL) ? label[0] : "", NULL);

      gst_object_unref (overlay);
    }
  }

  return TRUE;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  const gchar caffe2_model_path[] = "./caffe2_model";

  gchar *str_pipeline;
  gulong handle_id;
  guint timer_id = 0;
  GstElement *element;

  _print_log ("start app..");

  /* init app variable */
  g_app.running = FALSE;
  g_app.received = 0;
  g_app.current_label_index = -1;
  g_app.new_label_index = -1;

  _check_cond_err (_caffe2_init_info (&g_app.caffe2_info, caffe2_model_path));

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("v4l2src name=cam_src ! videoconvert ! videoscale ! "
      "video/x-raw,width=%d,height=%d,format=RGB ! tee name=t_raw "
      "t_raw. ! queue ! textoverlay name=tensor_res font-desc=Sans,24 ! "
      "videoconvert ! ximagesink name=img_tensor "
      "t_raw. ! queue leaky=2 max-size-buffers=2 ! "
      "videoscale ! video/x-raw,width=224,height=224,format=RGB ! tensor_converter ! "
      "tensor_transform mode=transpose option=1:2:0:3 ! "
      "tensor_transform mode=arithmetic option=typecast:float32,add:-123,div:63 ! "
      "tensor_filter framework=caffe2 model=\"%s,%s\" "
      "inputname=data input=224:224:3:1 inputtype=float32 "
      "output=1000:1:1:1 outputtype=float32 outputname=softmax ! "
      "tensor_sink name=tensor_sink",
      VIDEO_WIDTH, VIDEO_HEIGHT,
      g_app.caffe2_info.init_model_path, g_app.caffe2_info.pred_model_path);

  _print_log ("%s\n", str_pipeline);

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  handle_id = g_signal_connect (g_app.bus, "message",
      (GCallback) _message_cb, NULL);
  _check_cond_err (handle_id > 0);

  /* tensor sink signal : new data callback */
  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_sink");
  handle_id = g_signal_connect (element, "new-data",
      (GCallback) _new_data_cb, NULL);
  gst_object_unref (element);
  _check_cond_err (handle_id > 0);

  /* timer to update result */
  timer_id = g_timeout_add (500, _timer_update_result_cb, NULL);
  _check_cond_err (timer_id > 0);

  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);

  g_app.running = TRUE;

  /* set window title */
  _set_window_title ("img_tensor", "NNStreamer Example");

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
