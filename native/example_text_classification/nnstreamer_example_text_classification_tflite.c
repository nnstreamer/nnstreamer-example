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
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
    app_data_s *app;

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* init app variable */
  app = g_new0 (app_data_s, 1);
  g_assert (app);

  /* load model files */
  g_assert (load_model_files (app));

  g_free (app->model_file);
  g_strfreev (app->labels);
  g_hash_table_destroy (app->words);

  g_free (app);

  return 0;
}
