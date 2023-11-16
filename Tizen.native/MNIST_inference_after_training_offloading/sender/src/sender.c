/**
 * @file sender.c
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
#include <nnstreamer-tizen-internal.h>
#include "sender.h"

#define BROKER_IP "192.168.0.6"
#define RECT_SIZE 1000
#define EPOCHS 100
#define SAMPLES 1000
#define TOTAL_SAMPLES (SAMPLES * EPOCHS)
#define RATIO 100
#define USE_HYBRID 1

/**
 * @brief Appdata struct
 */
typedef struct appdata
{
  Evas_Object *win;
  Evas_Object *conform;

  /* offloading */
  Evas_Object *box;
  Evas_Object *offloading_label1;
  Evas_Object *offloading_label2;
  Evas_Object *start_offloading_button;
  Evas_Object *end_offloading_button;
  Evas_Object *receive_model_button;
  Evas_Object *inference_button;
  Evas_Object *offloading_exit;
  Evas_Object *inference_exit;
  Evas_Object *gray_rect;
  Evas_Object *blue_rect;
  int win_width;
  int win_height;
  int rect_x;
  int rect_y;
  int blue_rect_width;

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
  gchar *inference_file;
  gchar *offloaded_model_path;
  Ecore_Pipe *offloading_pipe;  /* To use main loop */
  Ecore_Pipe *inference_pipe;   /* To use main loop */
  ml_pipeline_h data_offloading_pipe;
  ml_pipeline_h model_inference_pipe;
  ml_pipeline_sink_h sink_handle;
  ml_pipeline_element_h elem_handle;
  unsigned int epochs;
  unsigned int num_of_offloading_data;
  unsigned int inference_idx;
  float inference[10];
  unsigned int inference_result;
  guint64 start_time, end_time;
  gdouble elapsed_time;
  ml_option_h option_h;
  ml_service_h service_h;
  ml_information_h activated_model_info;
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
  if (ad->service_h) {
    ml_service_destroy (ad->service_h);
    ad->service_h = NULL;
  }
  if (ad->activated_model_info) {
    ml_information_destroy (ad->activated_model_info);
    ad->activated_model_info = NULL;
  }
}

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
  g_autofree gchar *res_path = app_get_resource_path ();

  ad->model_config = g_strdup_printf ("%s/%s", res_path, "data/inference.ini");

  /* Save_Path in inference.ini must be set to trained model path for inference */
  dlog_print (DLOG_INFO, LOG_TAG, "model_config:%s", ad->model_config);

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
    ml_pipeline_destroy (ad->data_offloading_pipe);
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
  destroy_ml_service_handle (ad);
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
ecore_pipe_offloading_cb (void *data, void *buffer, unsigned int nbyte)
{
  appdata_s *ad = data;
  ad->end_time = g_get_monotonic_time ();
  ad->elapsed_time = (ad->end_time - ad->start_time) / (double) G_USEC_PER_SEC;

  if (ad->num_of_offloading_data % RATIO == 0)
    ad->blue_rect_width++;

  g_autofree gchar *text =
      g_strdup_printf
      ("<align=center><font_size=21>Number of data offloading: %d/%d "
      "[ Elapsed time: %.6f second ]</br></font></align>",
      ad->num_of_offloading_data, TOTAL_SAMPLES, ad->elapsed_time);
  elm_object_text_set (ad->offloading_label2, text);
  evas_object_resize (ad->blue_rect, ad->blue_rect_width, 50);
  evas_object_show (ad->blue_rect);
}

/**
 * @brief get output from the tensor_sink
 */
static void
get_offloading_data_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  size_t data_size = 0;
  appdata_s *ad = (appdata_s *) user_data;

  ad->num_of_offloading_data++;
  ecore_pipe_write (ad->offloading_pipe, ad, (unsigned int) data_size);
}

/**
 * @brief create pipeline for MNIST model training
 */
static void
create_data_offloading_pipeline (appdata_s * ad)
{
  int ret = ML_ERROR_NONE;
  g_autofree gchar *pipeline_description = NULL;

  dlog_print (DLOG_INFO, LOG_TAG, "data:%s", ad->data);
  dlog_print (DLOG_INFO, LOG_TAG, "json:%s", ad->json);

  pipeline_description =
      g_strdup_printf
      ("datareposrc location=%s json=%s epochs=%d ! tee name=t ! queue ! "
      "edgesink port=0 connect-type=AITT topic=tempTopic dest-host=%s "
      "dest-port=1883 wait-connection=true connection-timeout=10000 "
      "t. ! queue ! tensor_sink name=tensor_sink0 sync=true ", ad->data,
      ad->json, EPOCHS, BROKER_IP);

  ret =
      ml_pipeline_construct (pipeline_description, NULL, NULL,
      &ad->data_offloading_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to construct ml pipeline");
    return;
  }

  ret =
      ml_pipeline_sink_register (ad->data_offloading_pipe, "tensor_sink0",
      get_offloading_data_from_tensor_sink, ad, &ad->sink_handle);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to register tensor_sink");
    ml_pipeline_destroy (ad->data_offloading_pipe);
    return;
  }
}

/**
 * @brief evas object smart callback
 */
static void
start_offloading_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  int ret = ML_ERROR_NONE;
  appdata_s *ad = data;
  g_autofree gchar *res_path = app_get_resource_path ();
  gchar *text;

  elm_object_text_set (ad->offloading_label1, NULL);
  elm_object_text_set (ad->offloading_label2, NULL);
  evas_object_show (ad->gray_rect);

  ad->data = g_strdup_printf ("%s/%s", res_path, "data/mnist.data");
  ad->json = g_strdup_printf ("%s/%s", res_path, "data/mnist.json");


  text =
      g_strdup_printf
      ("<align=center><font_size=21>Start MNIST data offloading </br> %s </br>"
      "</font></align>", ad->data);

  elm_object_text_set (ad->offloading_label1, text);
  g_free (text);

  /* Add ecore_pipe */
  ad->offloading_pipe =
      ecore_pipe_add ((Ecore_Pipe_Cb) ecore_pipe_offloading_cb, ad);
  if (!ad->offloading_pipe) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to register callback");
  }

  create_data_offloading_pipeline (ad);

  ret = ml_pipeline_start (ad->data_offloading_pipe);
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
end_offloading_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  int ret = ML_ERROR_NONE;
  appdata_s *ad = data;

  elm_object_text_set (ad->offloading_label1, NULL);
  elm_object_text_set (ad->offloading_label2, NULL);
  evas_object_hide (ad->gray_rect);
  evas_object_hide (ad->blue_rect);

  elm_object_text_set (ad->offloading_label1,
      "<align=center><font_size=21>MNIST data offloading complete</font></align>");

  ret = ml_pipeline_stop (ad->data_offloading_pipe);
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

  ret = ml_pipeline_destroy (ad->data_offloading_pipe);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "Failed to destroy ml pipeline ret(%d)",
        ret);
    return;
  }
  ad->data_offloading_pipe = NULL;

  ecore_pipe_del (ad->offloading_pipe);
  ad->offloading_pipe = NULL;

  g_free (ad->data);
  g_free (ad->json);
  g_free (ad->model_config);
}

/**
 * @brief model receiver for model offloading 
 */
static void
start_model_receiver (appdata_s * ad)
{
  int ret = ML_ERROR_NONE;
  guint dest_port = 1883;

  ret = ml_option_create (&ad->option_h);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create ml option (%d)", ret);
    return;
  }

  ret = ml_option_set (ad->option_h, "node-type", "remote_receiver", NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG,
        "Failed to set node-type(remote_receiver)(%d)", ret);
    return;
  }

  ret = ml_option_set (ad->option_h, "dest-host", BROKER_IP, NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set dest-host (%d)", ret);
    return;
  }

  ret = ml_option_set (ad->option_h, "dest-port", &dest_port, NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set dest-port (%d)", ret);
    return;
  }

  /** A path to save the received model file */
  g_autofree gchar *shared_path = app_get_shared_data_path ();
  dlog_print (DLOG_ERROR, LOG_TAG, "path (%s)", shared_path);

  /** Currently, the path setting function cannot be used because
    it is not applied to the Tizen platform binary. It will be updated ASAP. */
#if 0
  ret = ml_option_set (ad->option_h, "path", shared_path, NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set path (%d)", ret);
    return;
  }
#endif

#ifdef USE_HYBRID
  ret = ml_option_set (ad->option_h, "connect-type", "HYBRID", NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set connect-type (%d)", ret);
    return;
  }
#if 0
  ret = ml_option_set (ad->option_h, "host", "192.168.0.6", NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set dest-host (%d)", ret);
    return;
  }
#endif
#else
  ret = ml_option_set (ad->option_h, "connect-type", "AITT", NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set connect-type (%d)", ret);
    return;
  }
#endif
  ret = ml_option_set (ad->option_h, "topic", "model_offloading_topic", NULL);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to set topic (%d)", ret);
    return;
  }

  ret = ml_service_remote_create (ad->option_h, &ad->service_h);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create ml remote service (%d)",
        ret);
  }

  /** Wait for the server to register the pipeline. */
  dlog_print (DLOG_INFO, LOG_TAG,
      "Wait for the server to register the pipeline.");
  g_usleep (20000000);
  dlog_print (DLOG_INFO, LOG_TAG, "Done");

  ret =
      ml_service_model_get_activated ("model_registration_key",
      &ad->activated_model_info);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to get model (%d)", ret);
    return;
  }

  ret =
      ml_information_get (ad->activated_model_info, "path",
      (void **) &ad->offloaded_model_path);
  if (ML_ERROR_NONE != ret) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to get model (%d)", ret);
    return;
  }

  dlog_print (DLOG_INFO, LOG_TAG, "Offloaded model path:%s",
      ad->offloaded_model_path);

  return;
}

/**
 * @brief evas object smart callback
 */
static void
receive_model_button_cb (void *data, Evas_Object * obj, void *event_info)
{
  appdata_s *ad = data;
  g_autofree gchar *text1;
  g_autofree gchar *text2;

  elm_object_text_set (ad->offloading_label1, NULL);
  elm_object_text_set (ad->offloading_label2, NULL);

  text1 =
      g_strdup_printf
      ("<align=center><font_size=21>Receive trained MNIST model from peer devices.</br></font></align>");
  elm_object_text_set (ad->offloading_label1, text1);

  destroy_ml_service_handle (ad);
  start_model_receiver (ad);

  text2 = g_strdup_printf
      ("<align=center><font_size=21>Offloaded model path</br>"
      "%s</font></align>", ad->offloaded_model_path);
  elm_object_text_set (ad->offloading_label2, text2);
  return;
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

  destroy_ml_service_handle (ad);

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

  ad->offloading_label1 = elm_label_add (ad->box);
  elm_object_text_set (ad->offloading_label1,
      "<align=center>MNIST inference after training offloading</align>");
  evas_object_size_hint_weight_set (ad->offloading_label1, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end (ad->box, ad->offloading_label1);
  evas_object_show (ad->offloading_label1);

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

  ad->offloading_label2 = elm_label_add (ad->box);
  evas_object_size_hint_weight_set (ad->offloading_label2, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end (ad->box, ad->offloading_label2);
  evas_object_show (ad->offloading_label2);

  ad->start_offloading_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->start_offloading_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->start_offloading_button, "Start data offloading");
  evas_object_show (ad->start_offloading_button);
  evas_object_smart_callback_add (ad->start_offloading_button, "clicked",
      start_offloading_button_cb, ad);
  elm_box_pack_end (ad->box, ad->start_offloading_button);

  ad->end_offloading_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->end_offloading_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->end_offloading_button, "End data offloading");
  evas_object_show (ad->end_offloading_button);
  evas_object_smart_callback_add (ad->end_offloading_button, "clicked",
      end_offloading_button_cb, ad);
  elm_box_pack_end (ad->box, ad->end_offloading_button);

  ad->receive_model_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->receive_model_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->receive_model_button, "Receive model");
  evas_object_show (ad->receive_model_button);
  evas_object_smart_callback_add (ad->receive_model_button, "clicked",
      receive_model_button_cb, ad);
  elm_box_pack_end (ad->box, ad->receive_model_button);

  ad->inference_button = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->inference_button, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->inference_button, "Inference with received model");
  evas_object_show (ad->inference_button);
  evas_object_smart_callback_add (ad->inference_button, "clicked",
      inference_button_cb, ad);
  elm_box_pack_end (ad->box, ad->inference_button);

  ad->offloading_exit = elm_button_add (ad->box);
  evas_object_size_hint_align_set (ad->offloading_exit, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  elm_object_text_set (ad->offloading_exit, "Exit");
  evas_object_show (ad->offloading_exit);
  evas_object_smart_callback_add (ad->offloading_exit, "clicked",
      win_delete_request_cb, ad);
  elm_box_pack_end (ad->box, ad->offloading_exit);
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
