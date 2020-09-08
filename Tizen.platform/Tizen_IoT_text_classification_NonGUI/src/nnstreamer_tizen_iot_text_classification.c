/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * NNStreamer example for text classification of Tizen IoT.
 * Copyright (C) 2020 Gichan Jang <gichan2.jang@samsung.com>
 */
 /**
 * @file nnstreamer_tizen_iot_text_classification.c
 * @date 17 July 2020
 * @brief Text classification example of Tizen IoT without GUI
 * @see  https://github.com/nnsuite/nnstreamer
 * @author Gichan Jang <gichan2.jang@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/app/app.h>

#define MAX_SENTENCE_LENGTH 256

/**
 * @brief Data structure for app.
 */
typedef struct
{
  gboolean running;
  gint tcp_sc;

  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
  GstElement *src; /**< appsrc element in pipeline */

  gchar *model_file; /**< tensorflow-lite model file */
  gchar **labels;
  GHashTable *words;
} app_data_s;


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
  g_warning ("format[%d] processed[%" G_GUINT64_FORMAT "] dropped[%"
      G_GUINT64_FORMAT "]", format, processed, dropped);
}

/**
 * @brief Callback for message.
 */
static void
bus_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  app_data_s *app;

  app = (app_data_s *) user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      app->running = FALSE;
      break;
    case GST_MESSAGE_ERROR:
      parse_err_message (message);
      app->running = FALSE;
      break;
    case GST_MESSAGE_WARNING:
      parse_err_message (message);
      break;
    case GST_MESSAGE_STREAM_START:
      break;
    case GST_MESSAGE_QOS:
      parse_qos_message (message);
      break;
    default:
      break;
  }
}

/**
 * @brief Function to load dictionary.
 */
static gboolean
load_model_files (app_data_s * app)
{
  const gchar path[] = "./res";

  gchar *model = g_build_filename (path, "text_classification.tflite", NULL);

  /* check the model files */
  if (!g_file_test (model, G_FILE_TEST_EXISTS)) {
    g_critical ("Failed to get model files.");
    g_free (model);
    return FALSE;
  }

  app->model_file = model;
  return TRUE;
}

/**
 * @brief Function to load dictionary.
 */
static gboolean
load_data (app_data_s * app)
{
  const gchar path[] = "./res";

  gboolean failed = FALSE;
  gchar *label = g_build_filename (path, "labels.txt", NULL);
  gchar *vocab = g_build_filename (path, "vocab.txt", NULL);
  gchar *meta = g_build_filename (path, "metadata", "MANIFEST", NULL);
  gchar *contents;

  /* check the model files */
  if (!g_file_test (label, G_FILE_TEST_EXISTS) ||
      !g_file_test (vocab, G_FILE_TEST_EXISTS) ||
      !g_file_test (meta, G_FILE_TEST_EXISTS)) {
    g_critical ("Failed to get model files.");
    failed = TRUE;
    goto error;
  }

  /* load label file */
  if (g_file_get_contents (label, &contents, NULL, NULL)) {
    app->labels = g_strsplit (contents, "\n", -1);

    g_free (contents);
  } else {
    g_critical ("Failed to load label file.");
    failed = TRUE;
    goto error;
  }

  /* load dictionary */
  if (g_file_get_contents (vocab, &contents, NULL, NULL)) {
    gchar **dics = g_strsplit (contents, "\n", -1);
    guint dics_len = g_strv_length (dics);
    guint i;

    /* init hash table */
    app->words = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    g_assert (app->words);

    for (i = 0; i < dics_len; i++) {
      if (dics[i] == NULL || dics[i][0] == '\0')
        continue;

      /* get word and index */
      gchar **word = g_strsplit (dics[i], " ", 2);

      if (g_strv_length (word) == 2) {
        gint word_index = (gint) g_ascii_strtoll (word[1], NULL, 10);

        gpointer key = g_strdup (word[0]);
        gpointer value = GINT_TO_POINTER (word_index);

        g_assert (g_hash_table_insert (app->words, key, value));
      } else {
        g_print ("Failed to parse voca.\n");
      }

      g_strfreev (word);
    }

    g_strfreev (dics);
    g_free (contents);
  } else {
    g_critical ("Failed to load dictionary file.");
    failed = TRUE;
    goto error;
  }

error:
  g_free (label);
  g_free (vocab);
  g_free (meta);

  return !failed;
}

/**
 * @brief Function to handle input string.
 */
static void
handle_input_string (app_data_s * app)
{
  GstBuffer *buf;
  gchar **tokens;
  gint tokens_len, i, value, start, unknown, pad;
  gfloat *float_array;

  float_array = g_malloc0 (sizeof (gfloat) * MAX_SENTENCE_LENGTH);
  g_assert (float_array);

  /* Get user input */
  gchar sentence[MAX_SENTENCE_LENGTH] = { 0, };
  g_print ("Please enter your movie review : ");

  if (fgets (sentence, sizeof (sentence), stdin)) {
    g_print ("Input : %s", sentence);
  }

  start = GPOINTER_TO_INT (g_hash_table_lookup (app->words, "<START>"));
  pad = GPOINTER_TO_INT (g_hash_table_lookup (app->words, "<PAD>"));
  unknown = GPOINTER_TO_INT (g_hash_table_lookup (app->words, "<UNKNOWN>"));

  float_array[0] = (gfloat) start;

  tokens = g_strsplit_set (sentence, " \n\t", MAX_SENTENCE_LENGTH);
  tokens_len = g_strv_length (tokens);

  for (i = 1; i < tokens_len; i++) {
    value = GPOINTER_TO_INT (g_hash_table_lookup (app->words, tokens[i - 1]));
    float_array[i] = (gfloat) (value > 0 ? value : unknown);
  }

  while (i < MAX_SENTENCE_LENGTH) {
    float_array[i++] = (gfloat) pad;
  }

  buf =
      gst_buffer_new_wrapped (float_array,
      MAX_SENTENCE_LENGTH * sizeof (gfloat));
  g_assert (buf);

  g_assert (gst_app_src_push_buffer (GST_APP_SRC (app->src),
          buf) == GST_FLOW_OK);
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  GstMemory *mem_res;
  GstMapInfo info_res;
  gfloat *output;

  mem_res = gst_buffer_get_memory (buffer, 0);
  g_assert (gst_memory_map (mem_res, &info_res, GST_MAP_READ));
  output = (gfloat *) info_res.data;

  g_print ("Output : \n");
  if (output[0] >= output[1]) {
    g_print ("  Negative : %.6f\n", output[0]);
    g_print ("  Positive : %.6f\n", output[1]);
  } else {
    g_print ("  Positive : %.6f\n", output[1]);
    g_print ("  Negative : %.6f\n", output[0]);
  }

  gst_memory_unmap (mem_res, &info_res);
  gst_memory_unref (mem_res);
}

/**
 * @brief main function
 */
int main(int argc, char *argv[]){
  app_data_s *app;
  gchar *pipeline = NULL, *host;
  GstElement *element;
  guint port;

  /* init gstreamer */
  gst_init (&argc, &argv);

  if (argc < 4) {
    g_print ("Please the input values for 'tcp server/client', 'host ip address' and 'port' exactly. \n");
    g_print ("e.g) $ ./nnstreamer_tizen_iot_text_classification server tensorflow-lite 192.168.1.1 5001 (optional)nnfw\n");
    return 0;
  }

  /* init app variable */
  app = g_new0 (app_data_s, 1);
  g_assert (app);
  
  if (g_strcmp0 ("server", argv[1]) == 0) {
    app->tcp_sc = 1;
  }
  else if (g_strcmp0 ("client", argv[1]) == 0) {
    app->tcp_sc = 2;
  }
  else {
    g_print ("Please Enter exactly one of the `server` and `client`. \n");
    g_free (app);
    return 0;
  }

  host = g_strdup_printf ("%s", argv[2]);
  port = atoi (argv[3]);
  
  switch (app->tcp_sc) {
    case 1:
      /* load model files */
      g_assert (load_data (app));

      /* init pipeline */
      pipeline =
        g_strdup_printf
        ("appsrc name=appsrc caps=other/tensor,dimension=(string)256:1:1:1,type=(string)float32,framerate=(fraction)0/1 ! "
         "tensor_decoder mode=protobuf ! tcpserversink host=%s port=%d", host, port);
      break;
    case 2:
      /* load model files */
      g_assert (load_model_files (app));

      /* init pipeline */
      if (g_strcmp0 ("nnfw", argv[4]) == 0) {
        pipeline =
          g_strdup_printf
          ("tcpclientsrc host=%s port=%d ! other/protobuf-tensor,framerate=0/1 ! tensor_converter silent=false ! "
          "tensor_filter name=tfilter framework=nnfw model=%s ! "
          "tensor_sink name=tensor_sink", host, port, app->model_file);
      }
      else {
        if (argc == 5 && g_strcmp0 ("tensorflow-lite", argv[4]) != 0) {
          g_print ("You want to designate the framework. But you entered the wrong framework name. \n");
          g_print ("We support only `tensorflow-lite` and `nnfw` in this example. \n");
          g_print ("The pipeline will operate as default option, tensorflow-lite.\n");
        }
        pipeline =
          g_strdup_printf
          ("tcpclientsrc host=%s port=%d ! other/protobuf-tensor,framerate=0/1 ! tensor_converter silent=false ! "
          "tensor_filter name=tfilter framework=tensorflow-lite model=%s ! "
          "tensor_sink name=tensor_sink", host, port, app->model_file);
      }      
      break;
    default:
      break;
  }
  

  app->pipeline = gst_parse_launch (pipeline, NULL);
  g_free (pipeline);
  g_assert (app->pipeline);

  /* bus and message callback */
  app->bus = gst_element_get_bus (app->pipeline);
  g_assert (app->bus);

  gst_bus_add_signal_watch (app->bus);
  g_signal_connect (app->bus, "message", G_CALLBACK (bus_message_cb), app);

  switch (app->tcp_sc) {
    case 1:
      /* appsrc element to push input buffer */
      app->src = gst_bin_get_by_name (GST_BIN (app->pipeline), "appsrc");
      g_assert (app->src);
      break;
    case 2:
      /* get tensor sink element using name */
      element = gst_bin_get_by_name (GST_BIN (app->pipeline), "tensor_sink");
      g_assert (element != NULL);

      /* tensor sink signal : new data callback */
      g_signal_connect (element, "new-data", (GCallback) new_data_cb, NULL);
      gst_object_unref (element);
      break;
    default:
      break;
  }

  /* Start playing */
  gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

  /* ready to get input sentence */
  app->running = TRUE;

  /* get user input */
  while (app->running) {
    g_usleep (200 * 1000);
    if (app->tcp_sc == 1) {
      handle_input_string (app);
    }
  }

  /* stop the pipeline */
  gst_element_set_state (app->pipeline, GST_STATE_NULL);

  /* close app */
  gst_bus_remove_signal_watch (app->bus);
  gst_object_unref (app->bus);
  gst_object_unref (app->pipeline);
  g_free (host);
  g_free (app->model_file);
  g_strfreev (app->labels);
  if (app->words != NULL)
    g_hash_table_destroy (app->words);

  g_free (app);
  return 0;
}
