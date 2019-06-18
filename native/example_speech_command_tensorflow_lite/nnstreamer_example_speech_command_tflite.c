/**
 * @file	nnstreamer_example_speech_command_tflite.c
 * @date	29 Jan 2019
 * @brief	Tensor stream example with filter (tf-lite model for speech command)
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @bug		No known bugs.
 *
 * NNStreamer example for speech command.
 *
 * Get model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model-speech-command.sh
 * 
 * Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./nnstreamer_example_speech_command_tflite
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
  if (!(cond)) { \
    _print_log ("app failed! [line : %d]", __LINE__); \
    goto error; \
  }

/**
 * @brief Data structure for tflite model info.
 */
typedef struct
{
  gchar *model_path; /**< tflite model file path */
  gchar *label_path; /**< label file path */
  GSList *labels; /**< list of loaded labels */
  guint total_labels; /**< count of labels */
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
  gint current_label_index; /**< current label index */
  gint new_label_index; /**< new label index */
  tflite_info_s tflite_info; /**< tflite model info */
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Free data in tflite info structure.
 */
static void
tflite_free_info (tflite_info_s * tflite_info)
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
    g_slist_free_full (tflite_info->labels, g_free);
    tflite_info->labels = NULL;
  }
}

/**
 * @brief Check tflite model and load labels.
 */
static gboolean
tflite_init_info (tflite_info_s * tflite_info, const gchar * path)
{
  const gchar tflite_model[] = "conv_actions_frozen.tflite";
  const gchar tflite_label[] = "conv_actions_labels.txt";

  FILE *fp;

  g_return_val_if_fail (tflite_info != NULL, FALSE);

  tflite_info->model_path = NULL;
  tflite_info->label_path = NULL;
  tflite_info->labels = NULL;

  /* check model file exists */
  tflite_info->model_path = g_strdup_printf ("%s/%s", path, tflite_model);

  if (access (tflite_info->model_path, F_OK) != 0) {
    _print_log ("cannot find tflite model [%s]", tflite_info->model_path);
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
      tflite_info->labels = g_slist_append (tflite_info->labels, label);
    }

    if (line) {
      free (line);
    }

    fclose (fp);
  } else {
    _print_log ("cannot find tflite label [%s]", tflite_info->label_path);
    return FALSE;
  }

  tflite_info->total_labels = g_slist_length (tflite_info->labels);
  _print_log ("finished to load labels, total %d", tflite_info->total_labels);
  return TRUE;
}

/**
 * @brief Get label string with given index.
 */
static gchar *
tflite_get_label (tflite_info_s * tflite_info, gint index)
{
  guint length;

  g_return_val_if_fail (tflite_info != NULL, NULL);
  g_return_val_if_fail (tflite_info->labels != NULL, NULL);

  length = g_slist_length (tflite_info->labels);
  g_return_val_if_fail (index >= 0 && index < length, NULL);

  return (gchar *) g_slist_nth_data (tflite_info->labels, index);
}

/**
 * @brief Update tflite label index with max score.
 * @param scores array of scores
 * @param len received data length
 * @return None
 */
static void
update_top_label_index (float *scores, guint len)
{
  gint i;
  gint index = -1;
  float max_score = .0;

  g_return_if_fail (scores != NULL);
  g_return_if_fail ((len / sizeof (float)) == g_app.tflite_info.total_labels);

  for (i = 0; i < g_app.tflite_info.total_labels; i++) {
    if (scores[i] > 0 && scores[i] > max_score) {
      index = i;
      max_score = scores[i];
    }
  }

  /* score threshold */
  _print_log ("max score %f", max_score);
  if (max_score > 0.50f) {
    g_app.new_label_index = index;
  }
}

/**
 * @brief Free resources in app data.
 */
static void
free_app_data (void)
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

  tflite_free_info (&g_app.tflite_info);
}

/**
 * @brief Function to print error message.
 */
static void
parse_err_message (GstMessage * message)
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
parse_qos_message (GstMessage * message)
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
bus_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      _print_log ("received eos message");
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_ERROR:
      _print_log ("received error message");
      parse_err_message (message);
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_WARNING:
      _print_log ("received warning message");
      parse_err_message (message);
      break;

    case GST_MESSAGE_STREAM_START:
      _print_log ("received start message");
      break;

    case GST_MESSAGE_QOS:
      parse_qos_message (message);
      break;

    default:
      break;
  }
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
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
        _print_log ("received %d", (guint) info.size);

        update_top_label_index ((float *) info.data, (guint) info.size);

        gst_memory_unmap (mem, &info);
      }
    }
  }
}

/**
 * @brief Timer callback to update result.
 * @return True to ensure the timer continues
 */
static gboolean
timer_update_result_cb (gpointer user_data)
{
  gchar *label = NULL;
  GstElement *overlay;

  if (g_app.running) {
    if (g_app.new_label_index >= 0 &&
        g_app.current_label_index != g_app.new_label_index) {
      label = tflite_get_label (&g_app.tflite_info, g_app.new_label_index);
      _print_log ("label %s", label);

      /* update label */
      g_app.current_label_index = g_app.new_label_index;

      overlay = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_res");
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
  /* check your device */
  const gchar alsa_device[] = "hw:2";
  const gchar tflite_model_path[] = "./speech_model";

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

  _check_cond_err (tflite_init_info (&g_app.tflite_info, tflite_model_path));

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("alsasrc name=audio_src device=%s ! audioconvert ! audio/x-raw,rate=16000,format=S16LE,channels=1 ! tee name=t_raw "
      "t_raw. ! queue ! goom ! textoverlay name=tensor_res font-desc=Sans,24 ! videoconvert ! ximagesink "
      "t_raw. ! queue ! tensor_converter frames-per-tensor=1600 ! "
      "tensor_aggregator frames-in=1600 frames-out=16000 frames-flush=3200 frames-dim=1 ! "
      "tensor_transform mode=arithmetic option=typecast:float32,div:32767.0 ! "
      "tensor_filter framework=custom model=./libnnscustom_speech_command_tflite.so ! "
      "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=tensor_sink",
      alsa_device, g_app.tflite_info.model_path);

  /**
   * tensor info (conv_actions_frozen.tflite)
   * input[0] (tensor-idx [14] rank [2]) >> type[7] dim[1:16000:1:1] audio stream
   * input[1] (tensor-idx [15] rank [1]) >> type[0] dim[1:1:1:1] samplerate list
   * output[0] (tensor-idx [16] rank [2]) >> type[7] dim[12:1:1:1] result (scores)
   */
  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  handle_id = g_signal_connect (g_app.bus, "message",
      (GCallback) bus_message_cb, NULL);
  _check_cond_err (handle_id > 0);

  /* tensor sink signal : new data callback */
  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_sink");
  handle_id =
      g_signal_connect (element, "new-data", (GCallback) new_data_cb, NULL);
  gst_object_unref (element);
  _check_cond_err (handle_id > 0);

  /* timer to update result */
  timer_id = g_timeout_add (500, timer_update_result_cb, NULL);
  _check_cond_err (timer_id > 0);

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

  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

error:
  _print_log ("close app..");

  if (timer_id > 0) {
    g_source_remove (timer_id);
  }

  free_app_data ();
  return 0;
}
