/**
 * @file imgcls.c
 * @date 18 Apr 2023
 * @brief Tizen Native Example App for ML-API-Service
 * @see https://github.com/nnstreamer/nnstreamer
 * @author Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 * @bug No known bugs except for NYI items.
 */

#include "imgcls.h"
#include <glib.h>
#include <nnstreamer.h>
#include <ml-api-service.h>

/**
 * @brief UI and pipeline related objects
 */
typedef struct appdata {
  Evas_Object *win;
  Evas_Object *conform;
  Evas_Object *label;
  Evas_Object *llabel;
  Ecore_Pipe *data_output_pipe;
  ml_pipeline_h pipeline_handle;
  ml_pipeline_sink_h sink_handle;
  gchar result_text[32];
} appdata_s;

/**
 * @brief get output from the tensor_sink and make call for gui update
 */
void
get_output_from_tensor_sink(const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  int ret;
  size_t data_size;
  gchar *output;
  appdata_s *ad = (appdata_s *) user_data;

  ret = ml_tensors_data_get_tensor_data(data, 0, (void **) &output, &data_size);
  if (ret != ML_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "ml_tensors_data_get_tensor_data failed");
    return;
  }

  dlog_print(DLOG_INFO, LOG_TAG, "output: %s, size: %zu", output, data_size);
  g_strlcpy(ad->result_text, output, 32);

  ecore_pipe_write(ad->data_output_pipe, NULL, 0);
}

/**
 * @brief base code for basic UI app.
 */
static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
  ui_app_exit();
}

/**
 * @brief base code for basic UI app.
 */
static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
  appdata_s *ad = data;
  elm_win_lower(ad->win);
}

/**
 * @brief base code for basic UI app.
 */
static void
update_gui(void *data, void *buf, unsigned int size)
{
  appdata_s *ad = data;
  gchar *tt = g_strdup_printf(
    "<align=center>Detected object is.. %s</align>", ad->result_text);

  elm_object_text_set(ad->label, tt);
  g_free (tt);
}

/**
 * @brief Creates essential UI objects.
 */
static void
create_base_gui(appdata_s *ad)
{
  ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
  elm_win_autodel_set(ad->win, EINA_TRUE);

  if (elm_win_wm_rotation_supported_get(ad->win)) {
    int rots[4] = { 0, 90, 180, 270 };
    elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
  }
  evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
  eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

  ad->conform = elm_conformant_add(ad->win);
  elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
  elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
  evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_win_resize_object_add(ad->win, ad->conform);
  evas_object_show(ad->conform);

  ad->label = elm_label_add(ad->conform);
  elm_object_text_set(ad->label, "<align=center>Hello Tizen</align>");
  evas_object_move(ad-> label, 120, 80);
  evas_object_resize(ad->label, 600, 80);
  evas_object_show (ad->label);

  ad->llabel = elm_label_add(ad->conform);
  elm_object_text_set(ad->llabel, "<align=center>Model info here</align>");
  evas_object_move(ad->llabel, 0, 200);
  evas_object_resize(ad->llabel, 800, 200);
  evas_object_show (ad->llabel);

  evas_object_show(ad->win);

  ad->data_output_pipe = ecore_pipe_add(update_gui, ad);
  if (!ad->data_output_pipe) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to create data output pipe");
  }
}

/**
 * @brief Destroy the pipeline and related elements
 */
void
destroy_pipeline(appdata_s *ad)
{
  ml_pipeline_sink_unregister(ad->sink_handle);
  ml_pipeline_destroy(ad->pipeline_handle);
}

/**
 * @brief Initialize the pipeline with retrieved model path
 * @return 0 on success, -errno on error
 */
int
init_pipeline(appdata_s *ad)
{
  int status = -1;
  const gchar *key = "imgcls-mobilenet-224-224-fp";
  gchar *model_path;
  gchar *res_path = app_get_resource_path();
  gchar *label_path = g_strdup_printf("%s/%s", res_path, "labels.txt");

  // first check if there is registered / activated model via ml-service-api
  ml_option_h activated_model_info;
  status = ml_service_model_get_activated(key, &activated_model_info);
  if (status == ML_ERROR_NONE) {
    gchar *activated_model_path;
    status = ml_option_get(activated_model_info, "path", (void **) &activated_model_path);
    dlog_print(DLOG_INFO, LOG_TAG, "activated model path: %s", activated_model_path);

    model_path = g_strdup(activated_model_path);
    ml_option_destroy(activated_model_info);
  } else {
    // if there is no registered model, use the default model in the app.
    model_path = g_strdup_printf("%s/%s", res_path, "lite-model_mobilenet_v1_100_224_fp32_1.tflite");
  }

  gchar *pipeline = g_strdup_printf(
"v4l2src ! " // get video stream from the usb connected webcam.
"videoscale ! videoconvert ! video/x-raw,width=640,height=480,format=RGB,framerate=15/1 ! "
"tee name=t "
  "t. ! queue ! videoscale ! video/x-raw,width=224,height=224 ! tensor_converter ! "
  "other/tensors,num_tensors=1,types=uint8,format=static,dimensions=3:224:224:1 ! "
  "tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
  "queue leaky=2 max-size-buffers=2 ! "
  "tensor_filter framework=tensorflow2-lite model=%s custom=Delegate:XNNPACK,NumThreads:2 latency=1 throughput=1 ! "
  "other/tensors,format=static,num_tensors=1 ! "
  "tensor_decoder mode=image_labeling option1=%s ! "
  "tensor_converter input-dim=32 ! " // get 32-byte size text
  "tensor_sink name=sinkx", model_path, label_path);

  status = ml_pipeline_construct(pipeline, NULL, NULL, &ad->pipeline_handle);
  if (status != ML_ERROR_NONE) {
    destroy_pipeline (ad);
    goto fail_exit;
  }

  status = ml_pipeline_sink_register(ad->pipeline_handle, "sinkx", get_output_from_tensor_sink, ad, &ad->sink_handle);
  if (status != ML_ERROR_NONE) {
    destroy_pipeline(ad);
    goto fail_exit;
  }

  gchar *model_text = g_strdup_printf("Used Model path:<br><br><font_size=16><align=center>%s</align></font_size>", model_path);
  elm_object_text_set(ad->llabel, model_text);
  g_free(model_text);

fail_exit:
  g_free(res_path);
  g_free(label_path);
  g_free(model_path);
  g_free(pipeline);

  return status;
}

/**
 * @brief base code for basic UI app.
 */
static bool
app_create(void *data)
{
  appdata_s *ad = data;

  create_base_gui(ad);
  init_pipeline(ad);

  return true;
}

/**
 * @brief base code for basic UI app.
 */
static void
app_control(app_control_h app_control, void *data)
{
  return;
}

/**
 * @brief base code for basic UI app.
 */
static void
app_pause(void *data)
{
  appdata_s *ad = data;
  ml_pipeline_stop(ad->pipeline_handle);
}

/**
 * @brief base code for basic UI app.
 */
static void
app_resume(void *data)
{
  appdata_s *ad = data;
  ml_pipeline_start(ad->pipeline_handle);
}

/**
 * @brief base code for basic UI app.
 */
static void
app_terminate(void *data)
{
  appdata_s *ad = data;
  destroy_pipeline(ad);
  ecore_pipe_del(ad->data_output_pipe);
}

/**
 * @brief base code for basic UI app.
 */
int
main(int argc, char *argv[])
{
  appdata_s ad = {0,};
  int ret = 0;

  ui_app_lifecycle_callback_s event_callback = {0,};

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;

  ret = ui_app_main(argc, argv, &event_callback, &ad);
  if (ret != APP_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
  }

  return ret;
}
