/**
 * @file	nnstreamer_example_data_preprocessing.c
 * @date	9 June 2023
 * @brief	Data preprocessing example for generating training data
 * @author	Hyunil Park <hyunil46.park@samsung.com>
 * @bug		No known bugs.
 *
 * Sample code for data preprocessing
 *
 * Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./nnstreamer_example_data_preprocessing input_data_path new_file_name(for rename(optional))
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gst/gst.h>
#include <string.h>

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
 * @brief Data structure for app.
 */
typedef struct
{
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

#define MAX_OBJECT 10           /* maxium number of object in a image file */
#define ITEMS 5                 /* x, y, width, height and class */

/**
 * @brief Create an images list
 */
static GArray *create_images_list (const gchar * dir_path);
/**
 * @brief Create an annotations list
 */
static GArray *create_annotations_list (const GArray * images_list);
/**
 * @brief Read images filename
 */
static void rename_images_filename (const gchar * images_path,
    const gchar * new_name, const GArray * images_list);
/**
 * @brief Replace_word
 */
void replace_word (gchar * str, const char *target_word,
    const char *replacement_word);
/**
 * @brief Create label file
 */
static gchar *create_label_file (GArray * annotations_list);

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
    gst_object_unref (g_app.bus);
    g_app.bus = NULL;
  }

  if (g_app.pipeline) {
    gst_object_unref (g_app.pipeline);
    g_app.pipeline = NULL;
  }
}

/**
 * @brief Bus callback for message.
 */
static gboolean
bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      _print_log ("received eos message");
      g_main_loop_quit ((GMainLoop *) data);
      break;
    case GST_MESSAGE_ERROR:
      _print_log ("received error message");
      g_main_loop_quit ((GMainLoop *) data);
      break;
    default:
      break;
  }

  return TRUE;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  gchar *str_pipeline = NULL;
  gchar *data_path = NULL;
  gchar *new_name = NULL;
  GArray *images_list = NULL;
  GArray *annotations_list = NULL;
  gchar *images_path = NULL;
  gchar *label_file = NULL;

  _print_log ("start app..");

  data_path = argv[1];
  _check_cond_err (data_path != NULL);
  images_path = g_build_filename (data_path, "images", NULL);

  images_list = create_images_list (images_path);
  _check_cond_err (images_list != NULL);

  new_name = argv[2];
  if (new_name) {
    rename_images_filename (images_path, new_name, images_list);
  }

  /* Let's create an annotations_list in the order of images_list read with g_dir_open(). */
  annotations_list = create_annotations_list (images_list);
  _check_cond_err (annotations_list != NULL);

  /** The person using the model needs to know how to construct the model
     and what data formats are required. 
     The performance of the model is related to the construction of the model
     and the preprocessed data. */

  label_file = create_label_file (annotations_list);
  _check_cond_err (label_file != NULL);

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  GString *filename = g_string_new (NULL);
  g_string_append_printf (filename, "%s_", new_name);
  g_string_append (filename, "%03d.png");

  images_path = g_build_filename (images_path, filename->str, NULL);

  /* Generate yolov.data and yolov.json using datareposink */
  str_pipeline = g_strdup_printf
      ("multifilesrc location=%s ! pngdec ! videoconvert ! "
      "video/x-raw, format=RGB, width=416, height=416 ! "
      "tensor_converter input-dim=3:416:416:1 input-type=uint8 ! "
      "tensor_transform mode=arithmetic option=typecast:float32,div:255.0 ! mux.sink_0 "
      "filesrc location=%s blocksize=200 ! application/octet-stream ! "
      "tensor_converter input_dim=1:50:1:1 input-type=float32 ! mux.sink_1 "
      "tensor_mux name=mux sync-mode=nosync ! "
      "datareposink location=yolo.data json=yolo.json", images_path,
      label_file);
  g_string_free (filename, FALSE);
  _print_log ("%s\n", str_pipeline);

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);
  gst_bus_add_watch (g_app.bus, bus_callback, g_app.loop);
  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  /* run main loop */
  g_main_loop_run (g_app.loop);

  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

#ifdef READ_TEST
  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);
  /* Read yolo.data with yolo.json */
  str_pipeline = g_strdup_printf
      ("gst-launch-1.0 datareposrc location=yolo.data json=yolo.json ! tensor_sink");
  _print_log ("%s\n", str_pipeline);

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);
  gst_bus_add_watch (g_app.bus, bus_callback, g_app.loop);

  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  g_main_loop_run (g_app.loop);

  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);
#endif

error:
  _print_log ("close app..");
  _print_log
      ("command: ./nnstreamer_example_data_preprocessing input_data_path new_file_name(optional)\n");

  _free_app_data ();
  g_array_free (images_list, TRUE);
  g_array_free (annotations_list, TRUE);
  g_free (label_file);
  return 0;
}

/**
 * @brief Create an images list
 */
static GArray *
create_images_list (const gchar * dir_path)
{
  GDir *dir = NULL;
  GArray *images_list = NULL;
  const gchar *filename = NULL;

  g_return_val_if_fail (dir_path != NULL, NULL);

  images_list = g_array_new (FALSE, TRUE, sizeof (gchar *));

  dir = g_dir_open (dir_path, 0, NULL);
  if (!dir) {
    _print_log ("Failed to open directory");
    return NULL;
  }

  while ((filename = g_dir_read_name (dir)) != NULL) {
    gchar *file_path = g_build_filename (dir_path, filename, NULL);
    g_array_append_val (images_list, file_path);
  }
  g_dir_close (dir);

  return images_list;
}

/**
 * @brief Reanme images filename
 */
static void
rename_images_filename (const gchar * images_path, const gchar * new_name,
    const GArray * images_list)
{
  int i;
  gchar *old_path = NULL;
  gchar *new_path = NULL;

  g_return_if_fail (images_path != NULL);
  g_return_if_fail (new_name != NULL);
  g_return_if_fail (images_list != NULL);

  for (i = 0; i < images_list->len; i++) {
    GString *filename = g_string_new (NULL);
    g_string_append_printf (filename, "%s_%03d.png", new_name, i);

    old_path = g_array_index (images_list, gchar *, i);
    new_path = g_build_filename (images_path, filename->str, NULL);
    if (g_rename (old_path, new_path) == 0) {
      _print_log ("rename: %s -> %s", old_path, new_path);
    }

    g_string_free (filename, FALSE);
  }
}

/**
 * @brief Create an annotations list
 */
static GArray *
create_annotations_list (const GArray * images_list)
{
  int i;
  GArray *annotations_list = NULL;
  gchar *image_path = NULL;

  g_return_val_if_fail (images_list != NULL, NULL);

  annotations_list = g_array_new (FALSE, TRUE, sizeof (gchar *));

  for (i = 0; i < images_list->len; i++) {
    image_path = g_array_index (images_list, gchar *, i);
    gchar *annotation_path = g_strdup_printf ("%s", image_path);
    replace_word (annotation_path, "images", "annotations");
    replace_word (annotation_path, "png", "txt");
    g_array_append_val (annotations_list, annotation_path);
  }

  return annotations_list;
}

/**
 * @brief Replace_word
 */
void
replace_word (gchar * str, const char *target_word,
    const char *replacement_word)
{
  g_return_if_fail (str != NULL);
  g_return_if_fail (target_word != NULL);
  g_return_if_fail (replacement_word != NULL);

  int target_len = strlen (target_word);
  int replacement_len = strlen (replacement_word);

  char *ptr = strstr (str, target_word);

  memmove (ptr + replacement_len, ptr + target_len,
      strlen (ptr + target_len) + 1);
  memcpy (ptr, replacement_word, replacement_len);
}

/**
 * @brief Create label file
 */
static gchar *
create_label_file (GArray * annotations_list)
{
  FILE *read_fp = NULL;
  FILE *write_fp = NULL;
  gchar *line = NULL;
  gchar *annotations_path = NULL;
  gchar *label_filename = NULL;
  size_t len = 0;
  int i, row_idx;

  g_return_val_if_fail (annotations_list != NULL, NULL);

  label_filename = g_strdup ("yolo.label");
  write_fp = fopen (label_filename, "wb");
  if (write_fp == NULL) {
    _print_log ("Failed to open file");
    return NULL;
  }

  /** The maximum number of objects in an image file is MAX_OBJECT.
     All object information is stored in one label data,
     and insufficient information is padded with 0. */

  for (i = 0; i < annotations_list->len; i++) {
    float label[MAX_OBJECT * ITEMS] = { 0 };    /* padding with 0 */
    annotations_path = g_array_index (annotations_list, gchar *, i);
    read_fp = fopen (annotations_path, "r");
    if (read_fp == NULL) {
      _print_log ("Failed to open %s", annotations_path);
      return NULL;
    }

    int line_idx = 0;
    while (getline (&line, &len, read_fp) != -1) {
      gchar **strv = g_strsplit (line, " ", -1);
      for (row_idx = 0; strv[row_idx] != NULL; row_idx++) {
        if (row_idx == 0) {
          label[line_idx * 5 + 4] = atof (strv[row_idx]);       /* write class */
        } else {
          label[line_idx * 5 + row_idx - 1] = atof (strv[row_idx]) / 416;       /* box coordinate */
        }
      }
      g_strfreev (strv);
      line_idx++;
    }
    fclose (read_fp);
    fwrite ((char *) label, sizeof (float) * ITEMS * MAX_OBJECT, 1, write_fp);
  }
  fclose (write_fp);

  return label_filename;
}
