/**
 * @file	nnstreamer_example_text_classification_tflite.c
 * @date	26 November 2019
 * @brief	Example with tensorflolw-lite model for text classification.
 * @author	Gichan Jang <gichan2.jang@samsung.com>
 * @bug		No known bugs.
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/app.h>

#define MAX_SENTENCE_LENGTH 256

/**
 * @brief Data structure for app.
 */
typedef struct
{
  gboolean running;

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
 * @brief Callback for tensor sink signal.
 */
static void
new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  g_print("New data is arrived..\n");

  GstMemory *mem_res;
  GstMapInfo info_res;
  gchar *converted_val;

  mem_res = gst_buffer_get_memory (buffer, 0);
  g_assert (gst_memory_map (mem_res, &info_res, GST_MAP_READ));
  converted_val =  (gchar *)info_res.data;

  g_print("Sink pad data : %s\n",converted_val);

  gst_memory_unmap (mem_res, &info_res);
  gst_memory_unref (mem_res);


}

/**
 * @brief Function to load dictionary.
 */
static gboolean
load_model_files (app_data_s * app)
{
  const gchar path[] = "./tflite_text_classification";

  gboolean failed = FALSE;
  gchar *model = g_build_filename (path, "text_classification.tflite", NULL);
  gchar *label = g_build_filename (path, "labels.txt", NULL);
  gchar *vocab = g_build_filename (path, "vocab.txt", NULL);
  gchar *contents;

  /* check the model files */
  if (!g_file_test (model, G_FILE_TEST_EXISTS) ||
	  !g_file_test (label, G_FILE_TEST_EXISTS) ||
	  !g_file_test (vocab, G_FILE_TEST_EXISTS)) {
	g_critical ("Failed to get model files.");
	failed = TRUE;
	goto error;
  }

  /* load label file */
  if (g_file_get_contents (label, &contents, NULL, NULL)) {
	app->labels = g_strsplit (contents, "\n", -1);
    
	g_print("label 1 : %s \n", app->labels[0]);
	g_print("label 2 : %s \n", app->labels[1]);

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
      /* get word and index */
      gchar **word = g_strsplit (dics[i], " ", 2);

      if (g_strv_length (word) == 2) {
      gint word_index = (gint) g_ascii_strtoll (word[1], NULL, 10);

      gpointer key = g_strdup (word[0]);
      gpointer value = GINT_TO_POINTER (word_index);

      g_assert (g_hash_table_insert (app->words, key, value));
	  } else {
		  g_print("Failed to parse voca.\n");
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

  gint test_val = GPOINTER_TO_INT(g_hash_table_lookup(app->words, "radar"));
  g_print("radar : %d \n",test_val);
  g_assert(test_val == 9959);

  error:
	g_free (label);
	g_free (vocab);

  app->model_file = model;
  return !failed;
}

/**
 * @brief Function to handle input string.
 */
static void
handle_input_string (app_data_s * app)
{
  GstBuffer *buf;
  GstElement *appsrc;
  gchar **tokens;
  gint tokens_len, i, value, start, unknown, pad;
  static gfloat float_array[MAX_SENTENCE_LENGTH]={0.0, };

  /* Get user input */
  gchar sentence[MAX_SENTENCE_LENGTH] = { 0, };
  g_print("Please enter your movie review : ");

  if(fgets (sentence, sizeof (sentence), stdin)) {
	  g_print("Input : %s",sentence);
  }
  
  start = GPOINTER_TO_INT (g_hash_table_lookup (app->words, "<START>"));
  pad = GPOINTER_TO_INT (g_hash_table_lookup (app->words, "<PAD>"));
  unknown = GPOINTER_TO_INT (g_hash_table_lookup (app->words, "<UNKNOWN>"));
  
  float_array[0] = (gfloat) start;
  
  tokens = g_strsplit_set(sentence," \n\t",MAX_SENTENCE_LENGTH);
  tokens_len = g_strv_length(tokens) - 1;

  for(i=0;i<tokens_len;i++) {
    value = GPOINTER_TO_INT(g_hash_table_lookup(app->words, tokens[i]));
    float_array[i+1]= (gfloat) (value>0?value:unknown);
    g_print("Converted float array : %.1lf \n",float_array[i+1]);
  }
  while(i<MAX_SENTENCE_LENGTH) {
	  float_array[i++] = (gfloat) pad;
  }
  
  /* get appsrc element using name */
  appsrc = gst_bin_get_by_name (GST_BIN (app->pipeline), "appsrc");
  g_assert (appsrc != NULL);

  buf = gst_buffer_new_wrapped (sentence, MAX_SENTENCE_LENGTH);
  g_assert(buf);
  
  g_assert (gst_app_src_push_buffer (GST_APP_SRC (appsrc), buf) == GST_FLOW_OK);
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
	app_data_s *app;
  gchar *pipeline;
  GstElement *element;


  /* init gstreamer */
  gst_init (&argc, &argv);

  /* init app variable */
  app = g_new0 (app_data_s, 1);
  g_assert (app);

  /* load model files */
  g_assert (load_model_files (app));

  /* init pipeline, sample pipeline.. need to be changed */
   pipeline = 
       g_strdup_printf 
       ("appsrc name=appsrc caps=text/x-raw,format=utf8 ! "
        "tensor_converter input-dim = 256 ! tensor_sink name=tensor_sink");
  
  app->pipeline = gst_parse_launch (pipeline, NULL);
  g_free (pipeline);
  g_assert (app->pipeline);

  /* bus and message callback */
  app->bus = gst_element_get_bus (app->pipeline);
  g_assert (app->bus);


  gst_bus_add_signal_watch (app->bus);
  g_signal_connect (app->bus, "message", G_CALLBACK (bus_message_cb), app);

  /* get tensor sink element using name */
  element = gst_bin_get_by_name (GST_BIN (app->pipeline), "tensor_sink");
  g_assert (element != NULL);

  /* tensor sink signal : new data callback */
  g_signal_connect (element, "new-data",(GCallback) new_data_cb, NULL);
  gst_object_unref (element);



  /* Start playing */
  gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

  /* get user input */
  handle_input_string (app);
  
  g_usleep (200 * 1000);

  /* stop the pipeline */
  //gst_element_set_state (app->pipeline, GST_STATE_NULL);


 /* close app */
  gst_bus_remove_signal_watch (app->bus);
  gst_object_unref (app->bus);
  gst_object_unref (app->pipeline);

  g_free (app->model_file);
  g_strfreev (app->labels);
  g_hash_table_destroy (app->words);

  g_free (app);

  return 0;
}
