/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * NNStreamer example for image classification of Tizen IoT.
 * Copyright (C) 2020 Gichan Jang <gichan2.jang@samsung.com>
 */
 /**
 * @file nnstreamer_tizen_iot_image_classification.c
 * @date 28 July 2020
 * @brief Image classification example of Tizen IoT without GUI
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author Gichan Jang <gichan2.jang@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/app/app.h>

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
  GstElement *valve; /**< gst valve to control data flow */
  GstBus *bus; /**< gst bus for data pipeline */

  gchar * target;
  gboolean running;
  guint buffering_timer; /**< buffering_timer */
  gint current_label_index; /**< current label index */
  gint new_label_index; /**< new label index */
  gint tcp_sc;

  tflite_info_s tflite_info; /**< tflite model info */
} app_data_s;

static app_data_s app;

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
 * @brief Function to load dictionary.
 */
static gboolean
load_model_files (tflite_info_s * tflite_info)
{
  const gchar path[] = "./tflite_model_img";
  const gchar tflite_model[] = "mobilenet_v1_1.0_224_quant.tflite";
  const gchar tflite_label[] = "labels.txt";

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
  gchar * label;
  g_return_val_if_fail (tflite_info != NULL, NULL);
  g_return_val_if_fail (tflite_info->labels != NULL, NULL);

  length = g_list_length (tflite_info->labels);
  g_return_val_if_fail (index >= 0 && index < length, NULL);

  label = (gchar *) g_list_nth_data (tflite_info->labels, index);
  return g_strndup (label, strlen (label) - 1);
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
  app.new_label_index = -1;

  g_return_if_fail (scores != NULL);
  g_return_if_fail (len == app.tflite_info.total_labels);

  for (i = 0; i < len; i++) {
    if (scores[i] > 0 && scores[i] > max_score) {
      index = i;
      max_score = scores[i];
    }
  }

  app.new_label_index = index;
}

/**
 * @brief Free resources in app data.
 */
static void
_free_app_data (void)
{
  if (app.loop) {
    g_main_loop_unref (app.loop);
    app.loop = NULL;
  }

  if (app.bus) {
    gst_bus_remove_signal_watch (app.bus);
    gst_object_unref (app.bus);
    app.bus = NULL;
  }

  if (app.pipeline) {
    gst_object_unref (app.pipeline);
    app.pipeline = NULL;
  }

  if (app.valve) {
    gst_object_unref (app.valve);
    app.valve = NULL;
  }

  if (app.target) {
    g_free (app.target);
    app.target = NULL;
  }
  _tflite_free_info (&app.tflite_info);
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
      g_main_loop_quit (app.loop);
      break;

    case GST_MESSAGE_ERROR:
      _print_log ("received error message");
      _parse_err_message (message);
      g_main_loop_quit (app.loop);
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
  app.buffering_timer++;
  _print_log ("New data is reveiced");

  if (app.running) {
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
 * @brief Timer callback to update label.
 * @return True to ensure the timer continues
 */
static gboolean
_timer_update_result_cb (gpointer user_data)
{
  if (app.running) {
    gchar *label = NULL;

    if ((app.current_label_index != app.new_label_index) &&
         app.buffering_timer > 30) {
      app.current_label_index = app.new_label_index;

      label = _tflite_get_label (&app.tflite_info, app.current_label_index);
      _print_log ("Detected label : %s", label);
      _print_log ("app target : %s", app.target);

      if (g_strcmp0 (app.target, label) == 0) {
        _print_log ("valve is opened");
        g_object_set (G_OBJECT (app.valve), "drop", FALSE, NULL);
        app.buffering_timer = 0;
      }
      else {
        g_object_set (G_OBJECT (app.valve), "drop", TRUE, NULL);
        _print_log ("valve is closed");
      }
      g_free (label);
    }
  }

  return TRUE;
}

/**
 * @brief main function
 */
int main(int argc, char *argv[]){
  gchar *pipeline = NULL, *host;
  GstElement *element;
  guint port;

  /* init app variable */
  app.running = FALSE;
  app.buffering_timer = 0;
  app.current_label_index = -1;
  app.new_label_index = -1;
  app.target = NULL;

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  app.loop = g_main_loop_new (NULL, FALSE);

  if (argc < 5) {
    g_print ("Please the input values for 'tcp server/client', 'target', 'host ip address' and 'port' exactly. \n");
    g_print ("e.g) $ ./nnstreamer_tien_iot_image_classification server \"coffee mug\" 192.168.1.1 5001 \n");
    return 0;
  }


  if (g_strcmp0 ("server", argv[1]) == 0) {
    app.tcp_sc = 1;
  }
  else if (g_strcmp0 ("client", argv[1]) == 0) {
    app.tcp_sc = 2;
  }
  else {
    return 0;
  }
  app.target = g_strdup_printf ("%s", argv[2]);
  host = g_strdup_printf ("%s", argv[3]);
  port = atoi (argv[4]);

  switch (app.tcp_sc) {
    case 1:
      /* Server pipeline */
      pipeline =
        g_strdup_printf
        ("tcpserversrc host=%s port=%d ! gdpdepay ! other/flatbuf-tensor ! tensor_converter ! "
          "tensor_decoder mode=direct_video silent=FALSE ! videoconvert !  videoscale ! "
          "video/x-raw,width=640,height=480,framerate=10/1 ! autovideosink sync=false", host, port);
      break;
    case 2:
      /* Client (Edge device) pipeline*/
      g_assert (load_model_files (&app.tflite_info));

      pipeline =
        g_strdup_printf
        ("v4l2src name=cam_src ! videoconvert ! videoscale ! "
          "video/x-raw,width=640,height=480,format=RGB,framerate=10/1 ! tee name=t_raw "
          "t_raw. ! queue ! valve name=valvex ! tensor_converter ! tensor_decoder mode=flatbuf ! "
            "gdppay ! tcpclientsink host=%s port=%d async=false "
          "t_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! tensor_converter ! "
            "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=tensor_sink",
          host, port, app.tflite_info.model_path);
      break;
    default:
      break;
  }

  app.pipeline = gst_parse_launch (pipeline, NULL);
  g_free (pipeline);
  g_assert (app.pipeline);

  /* bus and message callback */
  app.bus = gst_element_get_bus (app.pipeline);
  g_assert (app.bus);

  gst_bus_add_signal_watch (app.bus);
  g_signal_connect (app.bus, "message", G_CALLBACK (_message_cb), &app);

  switch (app.tcp_sc) {
    case 1:
      break;
    case 2:
      /* tensor sink signal : new data callback */
      element = gst_bin_get_by_name (GST_BIN (app.pipeline), "tensor_sink");
      g_signal_connect (element, "new-data", (GCallback) _new_data_cb, NULL);
      gst_object_unref (element);

      /* Get valve element */
      app.valve = gst_bin_get_by_name (GST_BIN (app.pipeline), "valvex");
      g_object_set (G_OBJECT (app.valve), "drop", TRUE, NULL);

      /* timer to update result */
      g_timeout_add (500, _timer_update_result_cb, NULL);
      break;
    default:
      break;
  }

  /* Start playing */
  gst_element_set_state (app.pipeline, GST_STATE_PLAYING);

  /* ready to get input sentence */
  app.running = TRUE;

  /* run main loop */
  g_main_loop_run (app.loop);

  /* quit when received eos or error message */
  app.running = FALSE;

  /* stop the pipeline */
  gst_element_set_state (app.pipeline, GST_STATE_NULL);

  g_free (host);
  _free_app_data ();

  return 0;
}
