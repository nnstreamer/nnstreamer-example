/**
 * @file receiver.c
 * @date 21 October 2023
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
#include <nnstreamer-tizen-internal.h>
#include "receiver.h"

#define BROKER_IP "192.168.0.6"
#define RECT_SIZE 1000
#define EPOCHS 100
#define SAMPLES 1000
#define TOTAL_SAMPLES (SAMPLES * EPOCHS)
#define RATIO 100
#define MODEL_NAME "mnist_nntrainer_model.bin"
#define USE_HYBRID 1

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
  Evas_Object *send_model_button;
  Evas_Object *training_exit;
  Evas_Object *inference_exit;
  Evas_Object *gray_rect;
  Evas_Object *blue_rect;
  int win_width;
  int win_height;
  int rect_x;
  int rect_y;
  int blue_rect_width;

  /* ml pipe */
  gchar *model_config;
  gchar *model_save_path;
  gchar *transmission_model;
  Ecore_Pipe *training_pipe;    /* To use main loop */
  gchar *contents;
  ml_pipeline_h model_training_pipe;
  ml_pipeline_sink_h sink_handle0;
  ml_pipeline_sink_h sink_handle1;
  ml_pipeline_element_h elem_handle;
  int epochs;
  unsigned int num_of_data_offloaded;
  double result_data[4];
  gboolean click_stop_button;
  gboolean no_update;
  guint64 start_time, end_time;
  gdouble elapsed_time;
  ml_option_h option_h;
  ml_option_h service_option_h;
  ml_service_h service_h;
} appdata_s;

/**
 * @brief destroy ml service handles
 */
static void
destroy_ml_service_handle (appdata_s * ad)
{
  if (ad->option_h) {
    ml_option_destroy (ad->option_h);
    ad->option_h = NULL;
  }
  if (ad->service_option_h) {
    ml_option_destroy (ad->service_option_h);
    ad->service_option_h = NULL;
  }
  /** bug
     if (ad->service_h)
     ml_service_destroy (ad->service_h);
   */
}

/**
 * @brief evas object smart callback
 */
static void
win_delete_request_cb (void *data, Evas_Object * obj, void *event_info)
{
  appdata_s *ad = data;
  destroy_ml_service_handle (ad);
  g_free (ad->contents);
  g_free (ad->transmission_model);
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
 * @brief Called when data is written to the pipe
 */
static void
ecore_pipe_training_cb (void *data, void *buffer, unsigned int nbyte)
{
  appdata_s *ad = data;
  ad->end_time = g_get_monotonic_time ();
  ad->elapsed_time = (ad->end_time - ad->start_time) / (double) G_USEC_PER_SEC;

  if (ad->num_of_data_offloaded % RATIO == 0)
    ad->blue_rect_width++;

  g_autofree gchar *text =
      g_strdup_printf
      ("<align=center><font_size=21>Number of data offloaded: %d/%d "
      "[ Elapsed time: %.6f second ]</br></font></align>"
      "<align=center><font_size=21>%d/%d epochs = [training_loss: %f, training_accuracy: %f, validation_loss: %f, validation_accuracy: %f]</font></align>",
      ad->num_of_data_offloaded, TOTAL_SAMPLES,
      ad->elapsed_time,
      ad->epochs, EPOCHS, ad->result_data[0], ad->result_data[1],
      ad->result_data[2],
      ad->result_data[3]);
  elm_object_text_set (ad->training_label2, text);
  evas_object_resize (ad->blue_rect, ad->blue_rect_width, 50);
  evas_object_show (ad->blue_rect);
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
  ad->epochs++;

  ecore_pipe_write (ad->training_pipe, ad, (unsigned int) data_size);
}

/**
 * @brief get output from the tensor_sink
 */
static void
get_received_data_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  size_t data_size = 0;
  appdata_s *ad = (appdata_s *) user_data;

  ad->num_of_data_offloaded++;

  if (ad->click_stop_button && (ad->num_of_data_offloaded % SAMPLES == 0))
    ad->no_update = TRUE;

  if (ad->no_update)
    return;

  ecore_pipe_write (ad->training_pipe, ad, (unsigned int) data_size);
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
  ad->model_save_path = g_strdup_printf ("%s/%s", shared_path, MODEL_NAME);
  ad->transmission_model = g_strdup_printf ("%s/%s", shared_path, MODEL_NAME);

  dlog_print (DLOG_INFO, LOG_TAG, "model_config:%s", ad->model_config);
  dlog_print (DLOG_INFO, LOG_TAG, "model_save_path:%s", ad->model_save_path);

  pipeline_description =
      g_strdup_printf
      ("edgesrc dest-host=%s dest-port=1883 connect-type=AITT topic=tempTopic ! tee name=t ! queue ! "
      "other/tensors, format=static, num_tensors=2, framerate=0/1, dimensions=28:28:1:1.10:1:1:1, types=float32.float32 ! "
      "tensor_trainer name=tensor_trainer0 framework=nntrainer "
      "model-config=%s model-save-path=%s num-inputs=1 num-labels=1 "
      "num-training-samples=500 num-validation-samples=500 epochs=%d ! "
      "tensor_sink name=tensor_sink0 sync=true "
      "t. ! queue ! tensor_sink name=tensor_sink1 sync=true", BROKER_IP,
      ad->model_config, ad->model_save_path, EPOCHS);

  ret =
      ml_pipeline_construct (pipeline_description, NULL, NULL,
      &ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to construct ml pipeline");
    return;
  }

  ret =
      ml_pipeline_sink_register (ad->model_training_pipe, "tensor_sink0",
      get_training_output_from_tensor_sink, ad, &ad->sink_handle0);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to register tensor_sink0");
    ml_pipeline_destroy (ad->model_training_pipe);
    return;
  }

  ret =
      ml_pipeline_sink_register (ad->model_training_pipe, "tensor_sink1",
      get_received_data_from_tensor_sink, ad, &ad->sink_handle1);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to register tensor_sink0");
    ml_pipeline_destroy (ad->model_training_pipe);
    return;
  }
  ad->epochs = -1;
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
  evas_object_show (ad->gray_rect);

  ad->model_config = g_strdup_printf ("%s/%s", res_path, "mnist.ini");

  text =
      g_strdup_printf
      ("<align=center><font_size=21>Start training MNIST model with offloaded data from sender</br>"
      "MNIST model is configured by %s</font></align>", ad->model_config);

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

  ad->click_stop_button = TRUE;
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
  dlog_print (DLOG_INFO, LOG_TAG, "#### called ####");
  elm_object_text_set (ad->training_label1, NULL);
  elm_object_text_set (ad->training_label2, NULL);
  evas_object_hide (ad->gray_rect);
  evas_object_hide (ad->blue_rect);

  elm_object_text_set (ad->training_label1,
      "<align=center> MNIST model training complete</align>");

  ret = ml_pipeline_stop (ad->model_training_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to stop ml pipeline ret(%d)", ret);
    return;
  }

  ml_pipeline_sink_unregister (ad->sink_handle0);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->sink_handle0 = NULL;

  ml_pipeline_sink_unregister (ad->sink_handle1);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->sink_handle1 = NULL;

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
  g_free (ad->model_config);

  ad->no_update = FALSE;
  ad->click_stop_button = FALSE;
}

/**
 * @brief model sender for model offloading
 */
static void
start_model_sender (appdata_s * ad)
{
  int ret = ML_ERROR_NONE;
  guint dest_port = 1883;
  gsize len = 0;

  ret = ml_option_create (&ad->option_h);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create ml option (%d)", ret);
    return;
  }

  ret = ml_option_set (ad->option_h, "node-type", "remote_sender", g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG,
        "Failed to set node-type(remote_sender)(%d)", ret);
    return;
  }

  ret = ml_option_set (ad->option_h, "dest-host", BROKER_IP, g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set dest-host (%d)", ret);
    return;
  }

  ret = ml_option_set (ad->option_h, "dest-port", &dest_port, NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set dest-port (%d)", ret);
    return;
  }
#ifdef USE_HYBRID
  ret = ml_option_set (ad->option_h, "connect-type", "HYBRID", g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set connect-type (%d)", ret);
    return;
  }

  ret = ml_option_set (ad->option_h, "host", "192.168.0.4", g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set dest-host (%d)", ret);
    return;
  }
#else
  ret = ml_option_set (ad->option_h, "connect-type", "AITT", g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set connect-type (%d)", ret);
    return;
  }
#endif

  ret = ml_option_set (ad->option_h, "topic", "model_offloading_topic", g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set topic (%d)", ret);
    return;
  }

  ret = ml_remote_service_create (ad->option_h, &ad->service_h);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create ml remote service (%d)",
        ret);
  }

  ret = ml_option_create (&ad->service_option_h);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create service_option (%d)",
        ret);
    return;
  }

  ret =
      ml_option_set (ad->service_option_h, "service-type", "model_raw", g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set service-type (%d)", ret);
    return;
  }

  ret =
      ml_option_set (ad->service_option_h, "service-key",
      "model_registration_key", g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set service-key (%d)", ret);
    return;
  }

  ret = ml_option_set (ad->service_option_h, "activate", "true", g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set activate (%d)", ret);
    return;
  }

  ret = ml_option_set (ad->service_option_h, "name", MODEL_NAME, g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set name (%d)", ret);
    return;
  }

  gchar *model_description =
      "MNIST model created with nntrainer, total_samples:1000, sample_size:3176,"
      "gst_caps: other/tensors, format=(string)static, framerate=(fraction)30/1, "
      "num_tensors=(int)2, dimensions=(string)28:28:1:1.10:1:1:1, types=(string)float32.float32";

  ret =
      ml_option_set (ad->service_option_h, "description", model_description,
      g_free);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set description (%d)", ret);
    return;
  }

  dlog_print (DLOG_INFO, LOG_TAG, "transmission model:%s",
      ad->transmission_model);

  ret = g_file_get_contents (ad->transmission_model, &ad->contents, &len, NULL);
  if (FALSE == ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to get contents(%s)",
        ad->transmission_model);
    return;
  }

  g_usleep (1000000);

  ret =
      ml_remote_service_register (ad->service_h, ad->service_option_h,
      ad->contents, len);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed ml_remote_service_register (%d)",
        ret);
    return;
  }

  dlog_print (DLOG_INFO, LOG_TAG, "Wait ....");
  g_usleep (20000000);
  dlog_print (DLOG_INFO, LOG_TAG, "Done ....");

  return;
}

/**
 * @brief evas object smart callback
 */
static void
send_model_to_sender_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  g_autofree gchar *text1;
  g_autofree gchar *text2;
  appdata_s *ad = data;

  elm_object_text_set (ad->training_label1, NULL);
  elm_object_text_set (ad->training_label2, NULL);

  text1 =
      g_strdup_printf
      ("<align=center><font_size=21>Send MNIST model to machine that offloaded data.</br>"
      "model path: %s</font></align>", ad->transmission_model);
  elm_object_text_set (ad->training_label1, text1);

  destroy_ml_service_handle (ad);

  start_model_sender (ad);

  text2 = g_strdup
      ("<align=center><font_size=22>model description</br></font></align>"
      "<align=center><font_size=21>MNIST model created with nntrainer, total_samples:1000, sample_size:3176,</br>"
      "gst_caps: other/tensors, format=(string)static, framerate=(fraction)30/1,</br> "
      "num_tensors=(int)2, dimensions=(string)28:28:1:1.10:1:1:1, types=(string)float32.float32</br></br></font></align>"
      "<align=center><font_size=21>"
      "service-type: model_raw, service-key: model_registration_key, activate: ture, name: mnist_nntrainer_model.bin</br>"
      "connect-type: AITT, topic: model_offloading_topic</font></align>");

  elm_object_text_set (ad->training_label2, text2);
  return;
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
  elm_object_text_set (ad->training_label1,
      "<align=center>MNIST model training with offloaded data</align>");
  evas_object_size_hint_weight_set (ad->training_label1, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end (ad->box, ad->training_label1);
  evas_object_show (ad->training_label1);

  ad->training_label2 = elm_label_add (ad->box);
  evas_object_size_hint_weight_set (ad->training_label2, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end (ad->box, ad->training_label2);
  evas_object_show (ad->training_label2);

  elm_win_screen_size_get (ad->win, NULL, NULL, &ad->win_width,
      &ad->win_height);
  ad->rect_x = ad->win_width / 2 - RECT_SIZE / 2;
  if (ad->win_height <= 720)
    ad->rect_y = ad->win_height / 4;
  else
    ad->rect_y = ad->win_height / 2.5;

  ad->gray_rect = evas_object_rectangle_add (evas_object_evas_get (ad->box));
  evas_object_resize (ad->gray_rect, RECT_SIZE, 50);
  evas_object_color_set (ad->gray_rect, 128, 128, 128, 255);
  evas_object_move (ad->gray_rect, ad->rect_x, ad->rect_y);

  ad->blue_rect = evas_object_rectangle_add (evas_object_evas_get (ad->box));
  evas_object_resize (ad->blue_rect, RECT_SIZE, 50);
  evas_object_color_set (ad->blue_rect, 0, 0, 255, 128);
  evas_object_move (ad->blue_rect, ad->rect_x, ad->rect_y);
  ad->blue_rect_width = 0;

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
  elm_object_text_set (ad->stop_training_button, "Stop model training");
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

  ad->send_model_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->send_model_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->send_model_button, "Send model to sender");
  evas_object_show (ad->send_model_button);
  evas_object_smart_callback_add (ad->send_model_button, "clicked",
      send_model_to_sender_button_cb, ad);
  elm_box_pack_end (ad->box, ad->send_model_button);

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
