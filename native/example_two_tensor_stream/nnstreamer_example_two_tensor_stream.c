/**
 * @file  nnstreamer_example_two_tensor_stream.c
 * @date  13 June 2019
 * @brief	Two tensor stream example with filters.
 * @author	Hyunji Choi <hjbc0921@gmail.com>
 * @bug No known bugs.
 *
 * NNStreamer example for two tensor streams using tensorflow-lite.
 *
 * 'tensor_filter' for image classification and speech command classification.
 * Get model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model-image-classification-tflite.sh
 * $ bash get-model-speech-command.sh
 *
 * Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./nnstreamer_example_two_tensor_stream
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
 * @brief Data structure for tflite model info.
 */
typedef struct
{
  gchar *model_path; /**< tflite model file path */
  gchar *label_path; /**< label file path  */
  GList *labels; /**< list of loaded labels */
  guint total_labels; /**< count of labels */
} tflite_info_s;

/**
 * @brief Data structure for stream info.
 */
typedef struct
{
  guint received; /**< received buffer count */
  gint current_label_index; /**< current label index */
  gint new_label_index; /**< new label index */
} stream_info_s;

/**
 * @brief Data structure for app.
 */
typedef struct
{
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
  gboolean running; /**< true when app is running */

  stream_info_s stream_info_img; /**< stream info for img */
  stream_info_s stream_info_speech; /**< stream info for speech */

  tflite_info_s tflite_info_img; /**< tflite model info for img */
  tflite_info_s tflite_info_speech; /**< tflite model info for speech */
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

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
 * @brief Check tflite model and load labels.
 */
static gboolean
_tflite_init_info (tflite_info_s * tflite_info, const gchar * path,
    gboolean isImg)
{
  gchar *tflite_model;
  gchar *tflite_label;

  if (isImg) {
    tflite_model = "mobilenet_v1_1.0_224_quant.tflite";
    tflite_label = "labels.txt";
  } else {
    tflite_model = "conv_actions_frozen.tflite";
    tflite_label = "conv_actions_labels.txt";
  }

  FILE *fp;

  g_return_val_if_fail (tflite_info != NULL, FALSE);

  tflite_info->model_path = NULL;
  tflite_info->label_path = NULL;
  tflite_info->labels = NULL;

  /* check model file exists */
  tflite_info->model_path = g_strdup_printf ("%s/%s", path, tflite_model);

  if (access (tflite_info->model_path, F_OK) != 0) {
    g_critical ("cannot find tflite model [%s]", tflite_info->model_path);
    return FALSE;
  }

  /* load labels */
  tflite_info->label_path = g_strdup_printf ("%s/%s", path, tflite_label);

  if ((fp = fopen (tflite_info->label_path, "r")) != NULL) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    gchar *label;

    while ((read = getline (&line, &len, fp)) != -1) {
      label = g_strdup ((gchar *) line);
      tflite_info->labels = g_list_append (tflite_info->labels, label);
    }

    if (line) {
      free (line);
    }

    fclose (fp);
  } else {
    g_critical ("cannot find tflite label [%s]", tflite_info->label_path);
    return FALSE;
  }

  tflite_info->total_labels = g_list_length (tflite_info->labels);
  _print_log ("finished to load labels, total %d", tflite_info->total_labels);
  return TRUE;
}

/**
 * @brief Get label string with given index.
 */
static gchar *
_tflite_get_label (tflite_info_s * tflite_info, gint index)
{
  guint length;

  g_return_val_if_fail (tflite_info != NULL, NULL);
  g_return_val_if_fail (tflite_info->labels != NULL, NULL);

  length = g_list_length (tflite_info->labels);
  g_return_val_if_fail (index >= 0 && index < length, NULL);

  return (gchar *) g_list_nth_data (tflite_info->labels, index);
}

/**
 * @brief Update tflite label index with max score for speech command.
 * @param scores array of scores
 * @param len received data length
 * @return None
 */
static void
_update_top_label_index_speech (float *scores, guint len)
{
  gint i;
  gint index = -1;
  float max_score = .0;

  g_return_if_fail (scores != NULL);
  g_return_if_fail ((len / sizeof (float)) ==
      g_app.tflite_info_speech.total_labels);

  for (i = 0; i < g_app.tflite_info_speech.total_labels; i++) {
    if (scores[i] > 0 && scores[i] > max_score) {
      index = i;
      max_score = scores[i];
    }
  }

  /* score threshold */
  _print_log ("max score %f", max_score);
  if (max_score > 0.50f) {
    g_app.stream_info_speech.new_label_index = index;
  }
}

/**
 * @brief Update tflite label index with max score for img classification.
 * @param scores array of scores
 * @param len array length
 * @return None
 */
static void
_update_top_label_index_img (guint8 * scores, guint len)
{
  gint i;
  gint index = -1;
  guint8 max_score = 0;

  /* -1 if failed to get max score index */
  g_app.stream_info_img.new_label_index = -1;

  g_return_if_fail (scores != NULL);
  g_return_if_fail (len == g_app.tflite_info_img.total_labels);

  for (i = 0; i < len; i++) {
    if (scores[i] > 0 && scores[i] > max_score) {
      index = i;
      max_score = scores[i];
    }
  }

  g_app.stream_info_img.new_label_index = index;
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

  _tflite_free_info (&g_app.tflite_info_img);
  _tflite_free_info (&g_app.tflite_info_speech);
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
  gboolean isImg = GPOINTER_TO_INT (user_data);

  /* print progress */
  if (isImg) {
    g_app.stream_info_img.received++;
    _print_log ("receiving new data [%d]", g_app.stream_info_img.received);
  } else {
    g_app.stream_info_speech.received++;
    _print_log ("receiving new data [%d]", g_app.stream_info_speech.received);
  }
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
        _print_log ("received %d", (guint) info.size);
        if (isImg) {
          _update_top_label_index_img (info.data, (guint) info.size);
        } else {
          _update_top_label_index_speech ((float *) info.data,
              (guint) info.size);
        }
        gst_memory_unmap (mem, &info);
      }
    }
  }
}

/**
 * @brief Timer callback for textoverlay.
 * @return True to ensure the timer continues
 */
static gboolean
_timer_update_result_cb (gpointer user_data)
{
  gboolean isImg = GPOINTER_TO_INT (user_data);
  if (g_app.running) {
    GstElement *overlay;
    gchar *label = NULL;
    gchar *bin_name = NULL;
    stream_info_s stream_info;
    tflite_info_s tflite_info;

    if (isImg) {
      stream_info = g_app.stream_info_img;
      tflite_info = g_app.tflite_info_img;
      bin_name = "tensor_res";
    } else {
      stream_info = g_app.stream_info_speech;
      tflite_info = g_app.tflite_info_speech;
      bin_name = "overlay";
    }
    if (stream_info.new_label_index >= 0 &&
        stream_info.current_label_index != stream_info.new_label_index) {
      stream_info.current_label_index = stream_info.new_label_index;
      label = _tflite_get_label (&tflite_info, stream_info.current_label_index);
      overlay = gst_bin_get_by_name (GST_BIN (g_app.pipeline), bin_name);
      g_object_set (overlay, "text", (label != NULL) ? label : "", NULL);
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
  const gchar tflite_model_path[] = "./tflite_model_img";
  const gchar tflite_speech_model_path[] = "./speech_model";
  const gboolean IS_IMG = TRUE;

  gchar *str_pipeline;
  gulong handle_id;
  guint timer_id = 0;
  guint timer_id_speech = 0;
  GstElement *element;

  _print_log ("start app..");

  /* init app variable */
  g_app.running = FALSE;
  g_app.stream_info_img.received = 0;
  g_app.stream_info_img.current_label_index = -1;
  g_app.stream_info_img.new_label_index = -1;
  g_app.stream_info_speech.received = 0;
  g_app.stream_info_speech.current_label_index = -1;
  g_app.stream_info_speech.new_label_index = -1;

  _check_cond_err (_tflite_init_info (&g_app.tflite_info_img, tflite_model_path,
          IS_IMG));
  _check_cond_err (_tflite_init_info (&g_app.tflite_info_speech,
          tflite_speech_model_path, !IS_IMG));

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("v4l2src name=cam_src ! videoconvert ! videorate ! videoscale ! "
      "video/x-raw,width=600,height=450,format=RGB,framerate=25/1 ! tee name=t_raw "
      "t_raw. ! queue ! textoverlay name=tensor_res font-desc=Sans,24 ! "
      "compositor name=mix ! videoconvert ! videoscale ! ximagesink "
      "t_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! tensor_converter ! "
      "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=tensor_sink "
      "alsasrc name=audio_src ! audioconvert ! audio/x-raw,rate=16000,format=S16LE,channels=1 ! tee name=t_r "
      "t_r. ! queue ! goom ! textoverlay name=overlay font-desc=Sans,24 ! mix. "
      "t_r. ! queue ! tensor_converter frames-per-tensor=1600 ! "
      "tensor_aggregator frames-in=1600 frames-out=16000 frames-flush=3200 frames-dim=1 ! "
      "tensor_transform mode=arithmetic option=typecast:float32,div:32767.0 ! "
      "tensor_filter framework=custom model=./libnnscustom_speech_command_tflite.so ! "
      "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=tensor_sink_speech",
      g_app.tflite_info_img.model_path, g_app.tflite_info_speech.model_path);

  /**
   * speech recognition tensor info (conv_actions_frozen.tflite)
   * input[0] (tensor-idx [14] rank [2]) >> type[7] dim[1:16000:1:1] audio stream
   * input[1] (tensor-idx [15] rank [1]) >> type[0] dim[1:1:1:1] samplerate list
   * output[0] (tensor-idx [16] rank [2]) >> type[7] dim[12:1:1:1] result (scores)
   */

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
      (GCallback) _new_data_cb, GINT_TO_POINTER (IS_IMG));
  gst_object_unref (element);
  _check_cond_err (handle_id > 0);

  element =
      gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_sink_speech");
  handle_id =
      g_signal_connect (element, "new-data", (GCallback) _new_data_cb,
      GINT_TO_POINTER (!IS_IMG));
  gst_object_unref (element);
  _check_cond_err (handle_id > 0);

  /* timer to update result */
  timer_id =
      g_timeout_add (500, _timer_update_result_cb, GINT_TO_POINTER (IS_IMG));
  _check_cond_err (timer_id > 0);
  timer_id_speech =
      g_timeout_add (500, _timer_update_result_cb, GINT_TO_POINTER (!IS_IMG));
  _check_cond_err (timer_id_speech > 0);

  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);

  g_app.running = TRUE;

  /* audio src info */
  if (DBG) {
    gchar *audio_dev = NULL;
    gchar *dev_name = NULL;
    gchar *card_name = NULL;

    element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "audio_src");

    g_object_get (element, "device", &audio_dev, NULL);
    g_object_get (element, "device-name", &dev_name, NULL);
    g_object_get (element, "card-name", &card_name, NULL);

    _print_log ("device [%s]", GST_STR_NULL (audio_dev));
    _print_log ("device name [%s]", GST_STR_NULL (dev_name));
    _print_log ("card name [%s]", GST_STR_NULL (card_name));

    gst_object_unref (element);
  }

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
