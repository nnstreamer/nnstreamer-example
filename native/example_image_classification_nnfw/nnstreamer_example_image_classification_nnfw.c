/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * NNStreamer example for image classification using nnfw.
 * Copyright (C) 2020 Gichan Jang <gichan2.jang@samsung.com>
 */

/**
 * @file	nnstreamer_example_image_classification_nnfw.c
 * @date	15 July 2020
 * @brief	Image classfication example using nnfw
 * @author	Gichan Jang <gichan2.jang@samsung.com>
 * @bug		No known bugs.
 *
 * Pipeline :
 * v4l2src -- tee -- textoverlay -- videoconvert -- ximagesink
 *             |
 *             --- videoscale -- tensor_converter -- tensor_filter -- tensor_sink
 *
 * Get model by
 * $ cd $NNST_ROOT/bin
 * $ ./get-model.sh image-classification-tflite
 *
 * 'tensor_sink' updates classification result to display in textoverlay.
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
  gchar *label_path; /**< label file path */
  GList *labels; /**< list of loaded labels */
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
 *
 * This example uses 'Mobilenet_1.0_224_quant' for image classification.
 */
static gboolean
_tflite_init_info (tflite_info_s * tflite_info)
{
  const gchar path[] = "./tflite_model_img";
  const gchar tflite_model[] = "mobilenet_v1_1.0_224_quant.tflite";
  const gchar tflite_label[] = "labels.txt";
  gchar *meta_file;
  FILE *fp;

  g_return_val_if_fail (tflite_info != NULL, FALSE);

  tflite_info->model_path = NULL;
  tflite_info->label_path = NULL;
  tflite_info->labels = NULL;

  meta_file = g_build_filename (path, "metadata", "./MANIFEST", NULL);

  /* check model file exists */
  tflite_info->model_path = g_build_filename (path, tflite_model, NULL);

  if (!g_file_test (meta_file, G_FILE_TEST_EXISTS) ||
      !g_file_test (tflite_info->model_path, G_FILE_TEST_EXISTS)) {
    g_critical ("Failed to get meta and model files.");
    g_print ("Please enter as below to download and locate the model.\n");
    g_print ("$ cd $NNST_ROOT/bin/ \n");
    g_print ("$ ./get-model.sh image-classification-tflite \n");
    g_free (meta_file);
    return FALSE;
  }

  /* load labels */
  tflite_info->label_path = g_build_filename (path, tflite_label, NULL);

  if ((fp = fopen (tflite_info->label_path, "r")) != NULL) {
    gchar *line = NULL;
    size_t len = 0;
    ssize_t read;
    gchar *label;

    while ((read = getline (&line, &len, fp)) != -1) {
      label = g_strdup ((gchar *) line);
      tflite_info->labels = g_list_append (tflite_info->labels, label);
    }

    if (line) {
      g_free (line);
    }

    fclose (fp);
  } else {
    g_critical ("cannot find tflite label [%s]", tflite_info->label_path);
    return FALSE;
  }

  tflite_info->total_labels = g_list_length (tflite_info->labels);
  g_print ("finished to load labels, total %d", tflite_info->total_labels);

  g_free (meta_file);
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
 * @brief Update tflite label index with max score.
 * @param scores array of scores
 * @param len array length
 * @return None
 */
static void
_update_top_label_index (guint8 * scores, guint len)
{
  gint i;
  gint index = -1;
  guint8 max_score = 0;

  /* -1 if failed to get max score index */
  g_app.new_label_index = -1;

  g_return_if_fail (scores != NULL);
  g_return_if_fail (len == g_app.tflite_info.total_labels);

  for (i = 0; i < len; i++) {
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
        _update_top_label_index (info.data, (guint) info.size);

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
    gchar *label = NULL;

    if (g_app.current_label_index != g_app.new_label_index) {
      g_app.current_label_index = g_app.new_label_index;

      overlay = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_res");

      label = _tflite_get_label (&g_app.tflite_info, g_app.current_label_index);
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

  _check_cond_err (_tflite_init_info (&g_app.tflite_info));

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("v4l2src name=cam_src ! videoconvert ! videoscale ! "
      "video/x-raw,width=640,height=480,format=RGB ! tee name=t_raw "
      "t_raw. ! queue ! textoverlay name=tensor_res font-desc=Sans,24 ! "
      "videoconvert ! ximagesink name=img_tensor "
      "t_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! tensor_converter ! "
      "tensor_filter framework=nnfw model=%s ! "
      "tensor_sink name=tensor_sink", g_app.tflite_info.model_path);

  _print_log ("%s\n", str_pipeline);

  /**
   * tensor info (mobilenet_v1_1.0_224_quant.tflite)
   * input[0] >> type:5 (uint8), dim[3:224:224:1] video stream (RGB 224x224)
   * output[0] >> type:5 (uint8), dim[1001:1] LABEL_SIZE:1
   */
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
