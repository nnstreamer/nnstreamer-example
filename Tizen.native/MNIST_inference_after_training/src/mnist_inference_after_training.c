/**
 * @file mnist_inference_after_training.c
 * @date 21 September 2023
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author Hyunil Park <hyunil46.park.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */
#include <glib.h>
#include <glib/gstdio.h>
#include <nnstreamer.h>
#include <ml-api-service.h>
#include <ml-api-common.h>
#include "mnist_inference_after_training.h"

/**
 * @brief Appdata struct
 */
typedef struct appdata
{
  Evas_Object *win;
  Evas_Object *conform;

  /* training */
  Evas_Object *box;
  Evas_Object *training_label1;
  Evas_Object *training_label2;
  Evas_Object *start_training_button;
  Evas_Object *stop_training_button;
  Evas_Object *end_training_button;
  Evas_Object *inference_button;
  Evas_Object *training_exit;
  Evas_Object *inference_exit;

  /* inference */
  Evas_Object *inference_label1;
  Evas_Object *inference_box;
  Evas_Object *table;
  Evas_Object *start_inference_button;
  Evas_Object *next_image_button;
  Evas_Object *image;

  /* ml pipe */
  gchar *data;
  gchar *json;
  gchar *model_config;
  gchar *model_save_path;
  gchar *inference_file;
  Ecore_Pipe *training_pipe;    /* To use main loop */
  Ecore_Pipe *inference_pipe;   /* To use main loop */
  ml_pipeline_h model_training_pipe;
  ml_pipeline_h model_inference_pipe;
  ml_pipeline_sink_h sink_handle;
  ml_pipeline_element_h elem_handle;
  unsigned int epochs;
  unsigned int inference_idx;
  double result_data[4];
  float inference[10];
  unsigned int inference_result;

  guint64 start_time, end_time;
  gdouble elapsed_time;
} appdata_s;

/**
 * @brief Called when data is written to the pipe
 */
static void
ecore_pipe_inference_cb (void *data, void *buffer, unsigned int nbyte)
{
  appdata_s *ad = data;
  g_autofree gchar *text1;

  text1 = g_strdup_printf
      ("<align=center><font_size=21>0: %f</br>1: %f</br>2: %f</br>3: %f</br>"
      "4: %f</br>5: %f</br>6: %f</br>7: %f</br>8: %f</br>9: %f</br></font></align>"
      "<align=center><font_size=30>Inference : %d</font></align>",
      ad->inference[0], ad->inference[1], ad->inference[2], ad->inference[3],
      ad->inference[4], ad->inference[5], ad->inference[6], ad->inference[7],
      ad->inference[8], ad->inference[9], ad->inference_result);
  elm_object_text_set (ad->inference_label1, text1);
}

/**
 * @brief get output from the tensor_sink
 */
static void
get_inference_output_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  int ret = ML_ERROR_NONE;
  size_t data_size;
  float *output;
  float max_value = 0;
  appdata_s *ad = (appdata_s *) user_data;

  ret =
      ml_tensors_data_get_tensor_data (data, 0, (void **) &output, &data_size);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_tensors_data_get_tensor_data failed");
    return;
  }

  ad->inference_result = 0;
  for (int i = 0; i < 10; i++) {
    ad->inference[i] = output[i];
    dlog_print (DLOG_INFO, LOG_TAG, "%d: %f", i, ad->inference[i]);
    if (ad->inference[i] > max_value) {
      max_value = ad->inference[i];
      ad->inference_result = i;
    }
  }
  dlog_print (DLOG_INFO, LOG_TAG, "start >> ecore_pipe_write");
  ecore_pipe_write (ad->inference_pipe, ad, (unsigned int) data_size);
  dlog_print (DLOG_INFO, LOG_TAG, "end >> ecore_pipe_write");
}

/**
 * @brief create pipeline for MNIST model inference
 */
static void
create_inference_pipeline (appdata_s * ad)
{
  int ret = ML_ERROR_NONE;
  g_autofree gchar *pipeline_description = NULL;
  g_autofree gchar *shared_path = app_get_shared_data_path ();
  g_autofree gchar *res_path = app_get_resource_path ();

  ad->model_save_path =
      g_strdup_printf ("%s/%s", shared_path, "mnist_nntrainer_model.bin");
  ad->model_config = g_strdup_printf ("%s/%s", res_path, "data/inference.ini");

  /* Save_Path in inference.ini must be set to trained model path for inference */
  dlog_print (DLOG_INFO, LOG_TAG, "model_config:%s", ad->model_config);
  dlog_print (DLOG_INFO, LOG_TAG, "model_save_path:%s", ad->model_save_path);

  pipeline_description =
      g_strdup_printf ("filesrc location=%s ! jpegdec ! videoconvert ! "
      "video/x-raw,format=GRAY8,framerate=0/1 ! tensor_converter ! "
      "tensor_transform mode=transpose option=1:2:0:3 ! "
      "tensor_transform mode=typecast option=float32 ! "
      "tensor_filter framework=nntrainer model=%s input=28:28:1:1 "
      "inputtype=float32 output=10:1:1:1 outputtype=float32 ! tensor_sink name=tensor_sink0",
      ad->inference_file, ad->model_config);

  ret =
      ml_pipeline_construct (pipeline_description, NULL, NULL,
      &ad->model_inference_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to construct ml pipeline");
    return;
  }

  ret =
      ml_pipeline_sink_register (ad->model_inference_pipe, "tensor_sink0",
      get_inference_output_from_tensor_sink, ad, &ad->sink_handle);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to register tensor_sink");
    ml_pipeline_destroy (ad->model_training_pipe);
    return;
  }
}

/**
 * @brief create pipeline for MNIST model inference
 */
static void
destroy_inference_pipeline (appdata_s * ad)
{
  int ret = ML_ERROR_NONE;

  if (!ad->model_inference_pipe)
    return;

  ret = ml_pipeline_stop (ad->model_inference_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to stop ml pipeline ret(%d)", ret);
    return;
  }

  ret = ml_pipeline_sink_unregister (ad->sink_handle);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->sink_handle = NULL;

  ret = ml_pipeline_destroy (ad->model_inference_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->model_inference_pipe = NULL;

  ecore_pipe_del (ad->inference_pipe);
  ad->inference_pipe = NULL;
  g_free (ad->model_save_path);
  g_free (ad->model_config);
  g_free (ad->inference_file);
}

/**
 * @brief evas object smart callback
 */
static void
load_next_image_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  appdata_s *ad = data;
  g_autofree gchar *res_path = app_get_resource_path ();
  destroy_inference_pipeline (ad);
  elm_object_text_set (ad->inference_label1, NULL);
  if (ad->inference_idx > 9)
    return;
  ad->inference_file =
      g_strdup_printf ("%s/data/%d.jpg", res_path, ad->inference_idx++);
  elm_image_file_set (ad->image, ad->inference_file, NULL);
}

/**
 * @brief evas object smart callback
 */
static void
start_inference_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  int ret = ML_ERROR_NONE;
  appdata_s *ad = data;

  elm_object_text_set (ad->inference_label1, NULL);

  /* Add ecore_pipe */
  ad->inference_pipe =
      ecore_pipe_add ((Ecore_Pipe_Cb) ecore_pipe_inference_cb, ad);
  if (!ad->inference_pipe) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to register callback");
    return;
  }

  create_inference_pipeline (ad);

  ret = ml_pipeline_start (ad->model_inference_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to start ml pipeline ret(%d)", ret);
    return;
  }
}

/**
 * @brief evas object smart callback
 */
static void
win_delete_request_cb (void *data, Evas_Object * obj, void *event_info)
{
  appdata_s *ad = data;
  destroy_inference_pipeline (ad);
  ui_app_exit ();
}

/**
 * @brief evas object smart callback
 */
static void
win_back_cb (void *data, Evas_Object * obj, void *event_info)
{
  appdata_s *ad = data;
  /* Let window go to hide state. */
  elm_win_lower (ad->win);
}

/**
 * @brief Creates essential UI objects.
 */
static void
create_inference_gui (appdata_s * ad)
{
  evas_object_size_hint_weight_set (ad->inference_box, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_object_content_set (ad->conform, ad->inference_box);

  ad->table = elm_table_add (ad->inference_box);
  elm_table_homogeneous_set (ad->table, EINA_TRUE);
  evas_object_size_hint_weight_set (ad->table, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set (ad->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
  elm_box_pack_end (ad->inference_box, ad->table);
  evas_object_show (ad->table);

  ad->image = elm_image_add (ad->table);
  evas_object_size_hint_align_set (ad->image, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set (ad->image, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_table_pack (ad->table, ad->image, 0, 0, 1, 1);
  evas_object_show (ad->image);

  /* Button */
  ad->start_inference_button = elm_button_add (ad->inference_box);
  evas_object_size_hint_align_set (ad->start_inference_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->start_inference_button, "start inference");
  evas_object_show (ad->start_inference_button);
  evas_object_smart_callback_add (ad->start_inference_button, "clicked",
      start_inference_button_cb, ad);
  elm_box_pack_end (ad->inference_box, ad->start_inference_button);

  ad->inference_label1 = elm_label_add (ad->inference_box);
  elm_object_text_set (ad->inference_label1, "<align=center>Inference</align>");
  evas_object_size_hint_weight_set (ad->inference_label1, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end (ad->inference_box, ad->inference_label1);
  evas_object_show (ad->inference_label1);

  ad->next_image_button = elm_button_add (ad->inference_box);
  evas_object_size_hint_align_set (ad->next_image_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->next_image_button, "Load next image");
  evas_object_show (ad->next_image_button);
  evas_object_smart_callback_add (ad->next_image_button, "clicked",
      load_next_image_button_cb, ad);
  elm_box_pack_end (ad->inference_box, ad->next_image_button);

  ad->inference_exit = elm_button_add (ad->inference_box);
  evas_object_size_hint_align_set (ad->inference_exit, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->inference_exit, "Exit");
  evas_object_show (ad->inference_exit);
  evas_object_smart_callback_add (ad->inference_exit, "clicked",
      win_delete_request_cb, ad);
  elm_box_pack_end (ad->inference_box, ad->inference_exit);

}

/**
 * @brief Called when data is written to the pipe
 */
static void
ecore_pipe_training_cb (void *data, void *buffer, unsigned int nbyte)
{
  appdata_s *ad = data;
  ad->end_time = g_get_monotonic_time ();
  ad->elapsed_time = (ad->end_time - ad->start_time) / (double) G_USEC_PER_SEC;

  g_autofree gchar *text =
      g_strdup_printf
      ("<align=center><font_size=21>%d/300 epochs = [training_loss: %f, training_accuracy: %f, validation_loss: %f, validation_accuracy: %f]</br></font></align>"
      "<align=right><font_size=21>Elapsed time: %.6f second</font></align>",
      ad->epochs++, ad->result_data[0], ad->result_data[1], ad->result_data[2],
      ad->result_data[3], ad->elapsed_time);
  elm_object_text_set (ad->training_label2, text);
}

/**
 * @brief get output from the tensor_sink
 */
static void
get_training_output_from_tensor_sink (const ml_tensors_data_h data,
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

  for (int i = 0; i < 4; i++)
    ad->result_data[i] = output[i];

  dlog_print (DLOG_INFO, LOG_TAG,
      "[training_loss: %f, training_accuracy: %f, validation_loss: %f, validation_accuracy: %f]",
      ad->result_data[0], ad->result_data[1], ad->result_data[2],
      ad->result_data[3]);
  dlog_print (DLOG_INFO, LOG_TAG, "start >> ecore_pipe_write");
  ecore_pipe_write (ad->training_pipe, ad, (unsigned int) data_size);
  dlog_print (DLOG_INFO, LOG_TAG, "end >> ecore_pipe_write");
}

/**
 * @brief create pipeline for MNIST model training
 */
static void
create_training_pipeline (appdata_s * ad)
{
  int ret = ML_ERROR_NONE;
  g_autofree gchar *pipeline_description = NULL;
  g_autofree gchar *shared_path = NULL;

  shared_path = app_get_shared_data_path ();
  ad->model_save_path =
      g_strdup_printf ("%s/%s", shared_path, "mnist_nntrainer_model.bin");

  dlog_print (DLOG_INFO, LOG_TAG, "data:%s", ad->data);
  dlog_print (DLOG_INFO, LOG_TAG, "json:%s", ad->json);
  dlog_print (DLOG_INFO, LOG_TAG, "model_config:%s", ad->model_config);
  dlog_print (DLOG_INFO, LOG_TAG, "model_save_path:%s", ad->model_save_path);

  pipeline_description =
      g_strdup_printf ("datareposrc location=%s json=%s epochs=300 ! queue ! "
      "tensor_trainer name=tensor_trainer0 framework=nntrainer model-config=%s "
      "model-save-path=%s num-inputs=1 num-labels=1 epochs=300 "
      "num-training-samples=500 num-validation-samples=500 ! "
      "tensor_sink name=tensor_sink0 sync=true", ad->data, ad->json,
      ad->model_config, ad->model_save_path);

  ret =
      ml_pipeline_construct (pipeline_description, NULL, NULL,
      &ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to construct ml pipeline");
    return;
  }

  ret =
      ml_pipeline_sink_register (ad->model_training_pipe, "tensor_sink0",
      get_training_output_from_tensor_sink, ad, &ad->sink_handle);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to register tensor_sink");
    ml_pipeline_destroy (ad->model_training_pipe);
    return;
  }
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
  gchar *text;

  elm_object_text_set (ad->training_label1, NULL);
  elm_object_text_set (ad->training_label2, NULL);

  ad->model_config = g_strdup_printf ("%s/%s", res_path, "data/training.ini");
  ad->data = g_strdup_printf ("%s/%s", res_path, "data/mnist.data");
  ad->json = g_strdup_printf ("%s/%s", res_path, "data/mnist.json");

  text =
      g_strdup_printf
      ("<align=center><font_size=21>Start training MNIST model with MNIST data</br> %s </br>"
      "MNIST model is configured by %s</font></align>", ad->data,
      ad->model_config);

  elm_object_text_set (ad->training_label1, text);
  g_free (text);

  /* Add ecore_pipe */
  ad->training_pipe =
      ecore_pipe_add ((Ecore_Pipe_Cb) ecore_pipe_training_cb, ad);
  if (!ad->training_pipe) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to register callback");
  }

  create_training_pipeline (ad);

  ret = ml_pipeline_start (ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to start ml pipeline ret(%d)", ret);
    return;
  }

  ad->start_time = g_get_monotonic_time ();
}

/**
 * @brief evas object smart callback
 */
static void
stop_training_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  int ret = ML_ERROR_NONE;
  appdata_s *ad = data;

  ret =
      ml_pipeline_element_get_handle (ad->model_training_pipe,
      "tensor_trainer0", &ad->elem_handle);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to get tensor_trainer handle");
    return;
  }

  ret =
      ml_pipeline_element_set_property_bool (ad->elem_handle,
      "ready-to-complete", TRUE);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG,
        "Failed to set 'ready-to-complete' property");
    return;
  }

  if (ad->elem_handle) {
    ret = ml_pipeline_element_release_handle (ad->elem_handle);
    if (ret != ML_ERROR_NONE) {
      dlog_print (DLOG_INFO, LOG_TAG, "Failed to release elem_handle");
      return;
    }
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

  elm_object_text_set (ad->training_label1, NULL);
  elm_object_text_set (ad->training_label2, NULL);

  elm_object_text_set (ad->training_label1,
      "<align=center> MNIST model training complete</align>");

  ret = ml_pipeline_stop (ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to stop ml pipeline ret(%d)", ret);
    return;
  }

  ml_pipeline_sink_unregister (ad->sink_handle);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->sink_handle = NULL;

  ret = ml_pipeline_destroy (ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->model_training_pipe = NULL;

  text =
      g_strdup_printf
      ("<align=center><font_size=21>MNIST model has been created.</br>%s</font></align>",
      ad->model_save_path);
  elm_object_text_set (ad->training_label2, text);

  ecore_pipe_del (ad->training_pipe);
  ad->training_pipe = NULL;

  g_free (ad->model_save_path);
  g_free (ad->data);
  g_free (ad->json);
  g_free (ad->model_config);
}

/**
 * @brief evas object smart callback
 */
static void
inference_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  appdata_s *ad = data;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *res_path = app_get_resource_path ();

  dlog_print (DLOG_ERROR, LOG_TAG, "res_path: %s", res_path);
  ad->inference_idx = 0;
  ad->inference_file =
      g_strdup_printf ("%s/data/%d.jpg", res_path, ad->inference_idx++);
  dlog_print (DLOG_ERROR, LOG_TAG, "filename: %s", filename);

  elm_box_unpack_all (ad->box);

  ad->inference_box = elm_box_add (ad->conform);
  evas_object_size_hint_weight_set (ad->inference_box, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_object_content_set (ad->conform, ad->inference_box);

  create_inference_gui (ad);
  evas_object_show (ad->inference_box);
  evas_object_show (ad->win);

  elm_image_file_set (ad->image, ad->inference_file, NULL);
}

/**
 * @brief Creates essential UI objects.
 */
static void
create_base_gui (appdata_s * ad)
{
  /* Window */
  /** Create and initialize elm_win.
     elm_win is mandatory to manipulate window. */
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
  /** Create and initialize elm_conformant.
     elm_conformant is mandatory for base gui to have proper size
     when indicator or virtual keypad is visible. */
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
  elm_object_content_set (ad->conform, ad->box);
  evas_object_show (ad->box);

  ad->training_label1 = elm_label_add (ad->box);
  elm_object_text_set (ad->training_label1, "<align=center>Training MNIST model</align>");
  evas_object_size_hint_weight_set (ad->training_label1, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end (ad->box, ad->training_label1);
  evas_object_show (ad->training_label1);

  ad->training_label2 = elm_label_add (ad->box);
  //elm_object_text_set (ad->training_label2, "<align=center>Result</align>");
  evas_object_size_hint_weight_set (ad->training_label2, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end (ad->box, ad->training_label2);
  evas_object_show (ad->training_label2);

  ad->start_training_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->start_training_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->start_training_button, "Start model training");
  evas_object_show (ad->start_training_button);
  evas_object_smart_callback_add (ad->start_training_button, "clicked",
      start_training_button_cb, ad);
  elm_box_pack_end (ad->box, ad->start_training_button);

  ad->stop_training_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->stop_training_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->stop_training_button, "stop model training");
  evas_object_show (ad->stop_training_button);
  evas_object_smart_callback_add (ad->stop_training_button, "clicked",
      stop_training_button_cb, ad);
  elm_box_pack_end (ad->box, ad->stop_training_button);

  ad->end_training_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->end_training_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->end_training_button, "End model training");
  evas_object_show (ad->end_training_button);
  evas_object_smart_callback_add (ad->end_training_button, "clicked",
      end_training_button_cb, ad);
  elm_box_pack_end (ad->box, ad->end_training_button);

  ad->inference_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->inference_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->inference_button, "Inference with trained model");
  evas_object_show (ad->inference_button);
  evas_object_smart_callback_add (ad->inference_button, "clicked",
      inference_button_cb, ad);
  elm_box_pack_end (ad->box, ad->inference_button);

  ad->training_exit = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->training_exit, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->training_exit, "Exit");
  evas_object_show (ad->training_exit);
  evas_object_smart_callback_add (ad->training_exit, "clicked",
      win_delete_request_cb, ad);
  elm_box_pack_end (ad->box, ad->training_exit);
  evas_object_show (ad->win);
}

/**
 * @brief base code for basic UI app.
 */
static bool
app_create (void *data)
{
  /** Hook to take necessary actions before main event loop starts
     Initialize UI resources and application's data
     If this function returns true, the main loop of application starts
     If this function returns false, the application is terminated */
  appdata_s *ad = data;

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
