/**
 * @file yolo_model_training.c
 * @date 07 September 2023
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author Hyunil Park <hyunil46.park@samsung.com>
 * @bug No known bugs except for NYI items
 */
#include <glib.h>
#include <glib/gstdio.h>
#include <nnstreamer.h>
#include <ml-api-service.h>
#include <ml-api-common.h>
#include "yolo_model_training.h"

#define MAX_OBJECT 4            /* maxium number of object in a image file */
#define ITEMS 5

/**
 * @brief Appdata struct
 */
typedef struct appdata
{
  Evas_Object *win;
  Evas_Object *conform;
  Evas_Object *box;
  Evas_Object *title_label;
  Evas_Object *label1;
  Evas_Object *label2;
  Evas_Object *label3;

  Evas_Object *start_training_button;
  Evas_Object *end_training_button;
  Evas_Object *exit_button;
  Evas_Object *data_preprocess_button;
  Evas_Object *end_preprocess_button;

  Ecore_Pipe *ecore_pipe;       /* To use main loop */
  ml_pipeline_h data_preprocess_pipe;
  ml_pipeline_h model_training_pipe;
  ml_pipeline_sink_h sink_handle;
  double result_data[4];
  unsigned int epochs;
  gchar *model_name;
  gchar *data;
  gchar *json;
  gchar *model_config;
  gchar *model_save_path;
} appdata_s;

/**
 * @brief get output from the tensor_sink and update label
 */
static void
get_output_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  int ret = ML_ERROR_NONE;
  size_t data_size;
  double *output;
  appdata_s *ad = (appdata_s *) user_data;

  ret =
      ml_tensors_data_get_tensor_data (data, 0, (void **) &output, &data_size);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_tensors_data_get_tensor_data failed");
    return;
  }

  for (int i = 0; i < 4; i++) {
    ad->result_data[i] = output[i];
  }

  dlog_print (DLOG_INFO, LOG_TAG,
      "[training_loss: %f, training_accuracy: %f, validation_loss: %f, validation_accuracy: %f]",
      ad->result_data[0], ad->result_data[1], ad->result_data[2],
      ad->result_data[3]);

  dlog_print (DLOG_INFO, LOG_TAG, "start >> ecore_pipe_write");
  ecore_pipe_write (ad->ecore_pipe, ad, (unsigned int) data_size);
  dlog_print (DLOG_INFO, LOG_TAG, "end >> ecore_pipe_write");
}

/**
 * @brief Create an images list
 */
static GArray *
create_images_list (const gchar * dir_path, appdata_s * ad)
{
  GDir *dir = NULL;
  GArray *images_list = NULL;
  const gchar *filename = NULL;

  g_return_val_if_fail (dir_path != NULL, NULL);

  images_list = g_array_new (FALSE, TRUE, sizeof (gchar *));

  dir = g_dir_open (dir_path, 0, NULL);
  if (!dir) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to open directory");
    g_array_free (images_list, FALSE);
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
    gchar *filename = g_strdup_printf ("%s_%03d.jpg", new_name, i);

    old_path = g_array_index (images_list, gchar *, i);
    new_path = g_build_filename (images_path, filename, NULL);
    if (g_rename (old_path, new_path) == 0) {
      dlog_print (DLOG_INFO, LOG_TAG, "rename: %s -> %s", old_path, new_path);
    }

    g_free (new_path);
    g_free (filename);
  }
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

  size_t target_len = strlen (target_word);
  size_t replacement_len = strlen (replacement_word);

  char *ptr = strstr (str, target_word);

  memmove (ptr + replacement_len, ptr + target_len,
      strlen (ptr + target_len) + 1);
  memcpy (ptr, replacement_word, replacement_len);
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
    replace_word (annotation_path, "jpg", "txt");
    g_array_append_val (annotations_list, annotation_path);
    dlog_print (DLOG_INFO, LOG_TAG, "annotation_path: %s", annotation_path);
  }

  return annotations_list;
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
  gchar *shared_path = NULL;
  gchar *annotations_path = NULL;
  gchar *label_filename = NULL;
  size_t len = 0;
  int i, row_idx;

  g_return_val_if_fail (annotations_list != NULL, NULL);

  shared_path = app_get_shared_data_path ();
  label_filename = g_strdup_printf ("%s/%s", shared_path, "yolo.label");
  g_free (shared_path);

  write_fp = fopen (label_filename, "wb");
  if (write_fp == NULL) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to open file");
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
      dlog_print (DLOG_ERROR, LOG_TAG, "Failed to open %s", annotations_path);
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
      if (line_idx == MAX_OBJECT)
        break;
    }
    fclose (read_fp);
    fwrite ((char *) label, sizeof (float) * ITEMS * MAX_OBJECT, 1, write_fp);
  }
  fclose (write_fp);

  return label_filename;
}

/**
 * @brief show list to label
 */
static void
print_list_info (const GArray * list, Evas_Object * label)
{
  int i;
  gchar *get_path = NULL;
  GString *image_list_text = g_string_new (NULL);
  for (i = 0; i < list->len; i++) {
    get_path = g_array_index (list, gchar *, i);
    g_string_append_printf (image_list_text, "%s</br>", get_path);
  }
  g_autofree gchar *text =
      g_strdup_printf ("<align=center><font_size=17>%s</font></align>",
      image_list_text->str);
  elm_object_text_set (label, text);
  g_string_free (image_list_text, TRUE);
}

/**
 * @brief Create ml pipeline
 */
static void
create_data_preprocess_pipeline (appdata_s * ad)
{
  int ret = ML_ERROR_NONE;
  gchar *shared_path = NULL;
  gchar *images_path = NULL;
  GArray *images_list = NULL;
  GArray *annotations_list = NULL;
  gchar *label_file = NULL;
  gchar *pipeline_description = NULL;
  gchar *filename = NULL;
  gchar *res_path = NULL;

  res_path = app_get_resource_path ();
  shared_path = app_get_shared_data_path ();

  images_path = g_strdup_printf ("%s/coco_sample/images", shared_path);

  images_list = create_images_list (images_path, ad);
  if (images_list == NULL) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create image_list");
    goto error;
  }

  /* print images_list to UI screen */
  print_list_info (images_list, ad->label1);

  rename_images_filename (images_path, "image", images_list);

  /* Let's create an annotations_list in the order of images_list read with g_dir_open(). */
  annotations_list = create_annotations_list (images_list);
  if (annotations_list == NULL) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create annotations_list");
    goto error;
  }

  /* print annotations_list to UI screen */
  print_list_info (annotations_list, ad->label2);

  /** The person using the model needs to know how to construct the model
     and what data formats are required.
     The performance of the model is related to the construction of the model
     and the preprocessed data. */
  label_file = create_label_file (annotations_list);

  if (label_file == NULL) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create label_file");
    goto error;
  }
  filename = g_strdup_printf ("%s/%s", images_path, "image_%03d.jpg");

  ad->data = g_strdup_printf ("%s/%s", shared_path, "yolo.data");
  ad->json = g_strdup_printf ("%s/%s", shared_path, "yolo.json");

  dlog_print (DLOG_ERROR, LOG_TAG, "filename: %s", filename);
  dlog_print (DLOG_ERROR, LOG_TAG, "label_file: %s", label_file);


  gchar *text =
      g_strdup_printf ("<align=center>All images have been renamed to "
      "image_000.jpg, image_001.jpg, image_002.jpg, and image_003.jpg.</br>"
      "label data has been created.'%s'</br>"
      "Now, ml_pipeline use renamed images and label data to create yolo.data and yolo.json.</align>",
      label_file);
  elm_object_text_set (ad->label3, text);
  g_free (text);

  /* Generate yolov2.data and yolov2.json using datareposink */
  pipeline_description = g_strdup_printf
      ("multifilesrc location=%s ! jpegdec ! videoconvert ! "
      "video/x-raw, format=RGB, width=416, height=416 ! "
      "tensor_converter input-dim=3:416:416:1 input-type=uint8 ! "
      "tensor_transform mode=arithmetic option=typecast:float32,div:255.0 ! mux.sink_0 "
      "filesrc location=%s blocksize=80 ! application/octet-stream ! "
      "tensor_converter input_dim=1:20:1:1 input-type=float32 ! mux.sink_1 "
      "tensor_mux name=mux sync-mode=nosync ! "
      "datareposink location=%s json=%s", filename, label_file, ad->data,
      ad->json);

  ret =
      ml_pipeline_construct (pipeline_description, NULL, NULL,
      &ad->data_preprocess_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to construct ml pipeline");
    goto error;
  }

error:

  g_free (shared_path);
  g_free (images_path);
  g_array_free (images_list, TRUE);
  g_array_free (annotations_list, TRUE);
  g_free (label_file);
  g_free (pipeline_description);
  g_free (filename);

  return;
}

/**
 * @brief evas object smart callback
 */
static void
data_preprocess_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  int ret = ML_ERROR_NONE;
  appdata_s *ad = data;
  g_autofree gchar *text =
      g_strdup_printf
      ("<align=center> Data preprocessing for training yolo model, below imported images and annotaions are used.</align>");
  elm_object_text_set (ad->title_label, text);

  dlog_print (DLOG_INFO, LOG_TAG, "data_preprocess_button_cb");

  create_data_preprocess_pipeline (ad);

  ret = ml_pipeline_start (ad->data_preprocess_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to start ml pipeline ret(%d)", ret);
    return;
  }
}

/**
 * @brief evas object smart callback
 */
static void
end_data_preprocess_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  int ret = ML_ERROR_NONE;
  appdata_s *ad = data;
  g_autofree gchar *text;

  elm_object_text_set (ad->title_label, NULL);
  elm_object_text_set (ad->label1, NULL);
  elm_object_text_set (ad->label2, NULL);
  elm_object_text_set (ad->label3, NULL);

  elm_object_text_set (ad->title_label,
      "<align=center> Created YOLO data for yolo model training</align>");

  dlog_print (DLOG_INFO, LOG_TAG, "end_data_preprocess_button_cb");

  ret = ml_pipeline_stop (ad->data_preprocess_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to stop ml pipeline ret(%d)", ret);
    return;
  }

  ret = ml_pipeline_destroy (ad->data_preprocess_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->data_preprocess_pipe = NULL;

  text =
      g_strdup_printf ("<align=center>%s</br>%s<br></align>", ad->data,
      ad->json);
  elm_object_text_set (ad->label1, text);
  elm_object_text_set (ad->label2,
      "<align=center> complete data preprocessing for YOLO data</align>");
}

/**
 * @brief creat ml pipeline for YOLO model training
 */
static void
create_ml_pipeline (appdata_s * ad)
{
  int ret = ML_ERROR_NONE;
  gchar *pipeline_description = NULL;
  gchar *shared_path = NULL;

  shared_path = app_get_shared_data_path ();
  ad->model_save_path =
      g_strdup_printf ("%s/%s", shared_path, "yolov2_nntrainer_model.bin");

  dlog_print (DLOG_INFO, LOG_TAG, "data:%s", ad->data);
  dlog_print (DLOG_INFO, LOG_TAG, "json:%s", ad->json);
  dlog_print (DLOG_INFO, LOG_TAG, "model_config:%s", ad->model_config);
  dlog_print (DLOG_INFO, LOG_TAG, "model_save_path:%s", ad->model_save_path);

  pipeline_description =
      g_strdup_printf ("datareposrc location=%s json=%s epochs=4 ! queue ! "
      "tensor_trainer name=tensor_trainer0 framework=nntrainer model-config=%s "
      "model-save-path=%s num-inputs=1 num-labels=1 "
      "num-training-samples=4 num-validation-samples=4 epochs=2 ! "
      "tensor_sink name=tensor_sink0 sync=true", ad->data, ad->json,
      ad->model_config, ad->model_save_path);

  ret =
      ml_pipeline_construct (pipeline_description, NULL, NULL,
      &ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to construct ml pipeline");
    goto error;
  }

  ret =
      ml_pipeline_sink_register (ad->model_training_pipe, "tensor_sink0",
      get_output_from_tensor_sink, ad, &ad->sink_handle);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to register tensor_sink");
    ml_pipeline_destroy (ad->model_training_pipe);
    goto error;
  }

error:

  g_free (shared_path);
  g_free (pipeline_description);
}

/**
 * @brief evas object smart callback
 */
static void
start_training_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  int ret = ML_ERROR_NONE;
  appdata_s *ad = data;
  g_autofree gchar *res_path = app_get_resource_path ();
  g_autofree gchar *text, *text2;

  elm_object_text_set (ad->title_label, NULL);
  elm_object_text_set (ad->label1, NULL);
  elm_object_text_set (ad->label2, NULL);
  elm_object_text_set (ad->label3, NULL);

  ad->model_config = g_strdup_printf ("%s/%s", res_path, "yolov2.ini");

  text =
      g_strdup_printf
      ("<align=center> Start training YOLO model with preprocessing YOLO data</br> %s</align>",
      ad->data);
  text2 =
      g_strdup_printf ("<align=center> YOLO model is configured by %s</align>",
      ad->model_config);

  elm_object_text_set (ad->label1, text);
  elm_object_text_set (ad->label2, text2);

  create_ml_pipeline (ad);

  ret = ml_pipeline_start (ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to start ml pipeline ret(%d)", ret);
    return;
  }
}

/**
 * @brief evas object smart callback
 */
static void
end_training_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  int ret = ML_ERROR_NONE;
  appdata_s *ad = data;
  g_autofree gchar *text;

  elm_object_text_set (ad->title_label, NULL);
  elm_object_text_set (ad->label1, NULL);
  elm_object_text_set (ad->label2, NULL);
  elm_object_text_set (ad->label3, NULL);

  elm_object_text_set (ad->title_label,
      "<align=center> YOLO model training complete</align>");

  ret = ml_pipeline_stop (ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to stop ml pipeline ret(%d)", ret);
    return;
  }

  ret = ml_pipeline_destroy (ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->model_training_pipe = NULL;

  elm_object_text_set (ad->label1,
      "<align=center>YOLO model has been created.<br></align>");
  text = g_strdup_printf ("<align=center>%s<br></align>", ad->model_save_path);
  elm_object_text_set (ad->label2, text);

  g_free (ad->model_save_path);
  g_free (ad->data);
  g_free (ad->json);
  g_free (ad->model_config);

}

/**
 * @brief base code for basic UI app.
 */
static void
win_delete_request_cb (void *data, Evas_Object * obj, void *event_info)
{
  ui_app_exit ();
}

/**
 * @brief base code for basic UI app.
 */
static void
win_back_cb (void *data, Evas_Object * obj, void *event_info)
{
  appdata_s *ad = data;
  /* Let window go to hide state. */
  elm_win_lower (ad->win);
}

/**
 * @brief Called when data is written to the pipe
 */
static void
ecore_pipe_cb (void *data, void *buffer, unsigned int nbyte)
{
  appdata_s *ad = data;
  g_autofree gchar *text =
      g_strdup_printf
      ("<align=center>%d epochs = [training_loss: %f, training_accuracy: %f, validation_loss: %f, validation_accuracy: %f]</align>",
      ad->epochs++, ad->result_data[0], ad->result_data[1], ad->result_data[2],
      ad->result_data[3]);
  elm_object_text_set (ad->label1, text);
}

/**
 * @brief Creates essential UI objects.
 */
static void
create_base_gui (appdata_s * ad)
{
  /* Window */
  ad->win = elm_win_util_standard_add (PACKAGE, PACKAGE);
  elm_win_autodel_set (ad->win, EINA_TRUE);

  if (elm_win_wm_rotation_supported_get (ad->win)) {
    int rots[4] = { 0, 90, 180, 270 };
    elm_win_wm_rotation_available_rotations_set (ad->win, (const int *) (&rots),
        4);
  }

  evas_object_smart_callback_add (ad->win, "delete,request",
      win_delete_request_cb, NULL);
  eext_object_event_callback_add (ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

  /* Conformant */
  ad->conform = elm_conformant_add (ad->win);
  elm_win_indicator_mode_set (ad->win, ELM_WIN_INDICATOR_SHOW);
  elm_win_indicator_opacity_set (ad->win, ELM_WIN_INDICATOR_OPAQUE);
  evas_object_size_hint_weight_set (ad->conform, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_win_resize_object_add (ad->win, ad->conform);
  evas_object_show (ad->conform);

  ad->box = elm_box_add (ad->conform);
  evas_object_size_hint_weight_set (ad->box, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_show (ad->box);
  elm_object_content_set (ad->conform, ad->box);

  /* Label */
  ad->title_label = elm_label_add (ad->box);
  elm_object_text_set (ad->title_label,
      "<align=center><font_size=30>YOLO training</font></align>");
  elm_object_style_set (ad->title_label, "maker");
  evas_object_size_hint_weight_set (ad->title_label, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set (ad->title_label, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  evas_object_size_hint_min_set (ad->title_label, 100, 100);
  evas_object_show (ad->title_label);
  elm_box_pack_end (ad->box, ad->title_label);

  ad->label1 = elm_label_add (ad->box);
  evas_object_size_hint_weight_set (ad->label1, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set (ad->label1, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_min_set (ad->label1, 100, 100);
  evas_object_show (ad->label1);
  elm_box_pack_end (ad->box, ad->label1);

  ad->label2 = elm_label_add (ad->box);
  evas_object_size_hint_weight_set (ad->label2, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set (ad->label2, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_min_set (ad->label2, 100, 100);
  evas_object_show (ad->label2);
  elm_box_pack_end (ad->box, ad->label2);

  ad->label3 = elm_label_add (ad->box);
  evas_object_size_hint_weight_set (ad->label3, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set (ad->label3, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_min_set (ad->label3, 100, 100);
  evas_object_show (ad->label3);
  elm_box_pack_end (ad->box, ad->label3);

  /* Button */
  ad->data_preprocess_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->data_preprocess_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->data_preprocess_button,
      "1. Start data preporcessing for YOLO data");
  evas_object_show (ad->data_preprocess_button);
  evas_object_smart_callback_add (ad->data_preprocess_button, "clicked",
      data_preprocess_button_cb, ad);
  elm_box_pack_end (ad->box, ad->data_preprocess_button);

  /* Button */
  ad->end_preprocess_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->end_preprocess_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->end_preprocess_button,
      "2. End data preporcessing for YOLO data");
  evas_object_show (ad->end_preprocess_button);
  evas_object_smart_callback_add (ad->end_preprocess_button, "clicked",
      end_data_preprocess_button_cb, ad);
  elm_box_pack_end (ad->box, ad->end_preprocess_button);

  ad->start_training_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->start_training_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->start_training_button, "3. Start Model training");
  evas_object_show (ad->start_training_button);
  evas_object_smart_callback_add (ad->start_training_button, "clicked",
      start_training_button_cb, ad);
  elm_box_pack_end (ad->box, ad->start_training_button);

  ad->end_training_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->end_training_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->end_training_button, "3. End Model training");
  evas_object_show (ad->end_training_button);
  evas_object_smart_callback_add (ad->end_training_button, "clicked",
      end_training_button_cb, ad);
  elm_box_pack_end (ad->box, ad->end_training_button);

  ad->exit_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->exit_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->exit_button, "Exit");
  evas_object_show (ad->exit_button);
  evas_object_smart_callback_add (ad->exit_button, "clicked",
      win_delete_request_cb, ad);
  elm_box_pack_end (ad->box, ad->exit_button);
  /* Show window after base gui is set up */
  evas_object_show (ad->win);

  /* Add ecore_pipe */
  ad->ecore_pipe = ecore_pipe_add ((Ecore_Pipe_Cb) ecore_pipe_cb, ad);
  if (ad->ecore_pipe) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to register callback");
  }
}

/**
 * @brief base code for basic UI app.
 */
static bool
app_create (void *data)
{
  appdata_s *ad = data;
  gchar *shared_path = app_get_shared_data_path ();

  dlog_print (DLOG_INFO, LOG_TAG, "shared path:%s", shared_path);

  create_base_gui (ad);

  return true;
}

/**
 * @brief base code for basic UI app.
 */
static void
app_control (app_control_h app_control, void *data)
{
  /* Handle the launch request. */
}

/**
 * @brief base code for basic UI app.
 */
static void
app_pause (void *data)
{
  /* Take necessary actions when application becomes invisible. */
}

/**
 * @brief base code for basic UI app.
 */
static void
app_resume (void *data)
{
  /* Take necessary actions when application becomes visible. */
}

/**
 * @brief base code for basic UI app.
 */
static void
app_terminate (void *data)
{
  /* Release all resources. */
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_lang_changed (app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LANGUAGE_CHANGED */

  int ret;
  char *language;

  ret = app_event_get_language (event_info, &language);
  if (ret != APP_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG,
        "app_event_get_language() failed. Err = %d.", ret);
    return;
  }

  if (language != NULL) {
    elm_language_set (language);
    free (language);
  }
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_orient_changed (app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_DEVICE_ORIENTATION_CHANGED */
  return;
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_region_changed (app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_REGION_FORMAT_CHANGED */
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_low_battery (app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LOW_BATTERY */
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_low_memory (app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LOW_MEMORY */
}

/**
 * @brief base code for basic UI app.
 */
int
main (int argc, char *argv[])
{
  appdata_s ad = { 0, };
  int ret = 0;

  ui_app_lifecycle_callback_s event_callback = { 0, };
  app_event_handler_h handlers[5] = { NULL, };

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;

  ui_app_add_event_handler (&handlers[APP_EVENT_LOW_BATTERY],
      APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
  ui_app_add_event_handler (&handlers[APP_EVENT_LOW_MEMORY],
      APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);
  ui_app_add_event_handler (&handlers[APP_EVENT_DEVICE_ORIENTATION_CHANGED],
      APP_EVENT_DEVICE_ORIENTATION_CHANGED, ui_app_orient_changed, &ad);
  ui_app_add_event_handler (&handlers[APP_EVENT_LANGUAGE_CHANGED],
      APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, &ad);
  ui_app_add_event_handler (&handlers[APP_EVENT_REGION_FORMAT_CHANGED],
      APP_EVENT_REGION_FORMAT_CHANGED, ui_app_region_changed, &ad);

  ret = ui_app_main (argc, argv, &event_callback, &ad);
  if (ret != APP_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
  }

  return ret;
}
