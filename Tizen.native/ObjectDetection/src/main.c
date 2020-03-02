/**
 * @file main.c
 * @date 28 Feb 2020
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author Parichay Kapoor <pk.kapoor@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include <app.h>
#include <Elementary.h>
#include <system_settings.h>
#include <efl_extension.h>
#include <dlog.h>
#include <privacy_privilege_manager.h>

#include "main.h"
#include "view.h"
#include "data.h"

ml_pipeline_h handle;
ml_pipeline_src_h srchandle;
ml_pipeline_sink_h sinkhandle;
ml_tensors_info_h info;

uint8_t * inArray;
gchar * pipeline;

const gchar ssd_model[] = "ssd_mobilenet_v2_coco.tflite";
const gchar ssd_label[] = "coco_labels_list.txt";
const gchar ssd_box_priors[] = "box_priors.txt";

/**
 * @brief Destroy the pipeline and related elements
 */
void destroy_pipeline()
{
  ml_pipeline_sink_unregister(sinkhandle);
  ml_pipeline_src_release_handle(srchandle);
  ml_pipeline_destroy(handle);
  ml_tensors_info_destroy(info);

  if (pipeline)
    g_free(pipeline);
  if (inArray)
    g_free(inArray);
}

/**
 * @brief get output from the tensor_sink and send to the caller
 */
void
get_output_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  size_t data_size;
  uint8_t *output;

  ml_tensors_data_get_tensor_data (data, 0, (void **) &output, &data_size);
//  update_gui (NULL, output, data_size);
  ecore_pipe_write(data_output_pipe, output, data_size);
}

/**
 * @brief Initialize the pipeline with model path
 * @return 0 on success, -errno on error
 */
int init_pipeline()
{
  int status = -1;
  gchar *res_path = app_get_resource_path();
  gchar *model_path = g_strdup_printf ("%s/%s", res_path, ssd_model);
  gchar *label_path = g_strdup_printf ("%s/%s", res_path, ssd_label);
  gchar *box_prior_path = g_strdup_printf ("%s/%s", res_path, ssd_box_priors);
  ml_tensor_dimension dim = { CH, CAM_WIDTH, CAM_HEIGHT, 1 };

  pipeline = NULL;
  inArray = NULL;

  pipeline = g_strdup_printf(
      /** Set the source size and format */
      "appsrc name=srcx ! videoconvert ! videoscale ! "
      "video/x-raw,width=%d,height=%d,format=RGB,framerate=30/1 ! tee name=t "
        /** Find the objects and make the boxes */
        "t. ! queue leaky=2 max-size-buffers=2 ! "
        "videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=RGB ! "
        "tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
        "tensor_filter framework=tensorflow-lite model=%s !"
        "tensor_decoder name=dec mode=bounding_boxes option1=tflite-ssd option2=%s option3=%s option4=%d:%d option5=%d:%d "
        /** Pass the decoder output output to the video mixer */
        "dec. ! videomixer name=mix sink_0::zorder=2 sink_1::zorder=1 ! "
        "videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=RGB,framerate=30/1 ! "
        "tensor_converter ! tensor_sink name=sinkix "
        /** Also give the original video to the video mixer */
        "t. ! queue leaky=2 max-size-buffers=10 ! mix.",
      CAM_WIDTH, CAM_HEIGHT, MODEL_WIDTH, MODEL_HEIGHT,
      model_path, label_path, box_prior_path,
      CAM_WIDTH, CAM_HEIGHT, MODEL_WIDTH, MODEL_HEIGHT,
      CAM_WIDTH, CAM_HEIGHT);

  inArray = (uint8_t *) g_malloc(sizeof(uint8_t) * CAM_WIDTH * CAM_HEIGHT * CH);
  if (!inArray)
    goto fail_exit;

  status = ml_pipeline_construct(pipeline, NULL, NULL, &handle);
  if (status != ML_ERROR_NONE)
    goto fail_exit;

  status = ml_pipeline_src_get_handle(handle, "srcx", &srchandle);
  if (status != ML_ERROR_NONE)
    goto fail_exit;

  status = ml_pipeline_sink_register(handle, "sinkx",
      get_output_from_tensor_sink, NULL, &sinkhandle);
  if (status != ML_ERROR_NONE)
    goto fail_exit;

  /**
   *  The media type `video/x-raw` cannot get valid info handle, you should set info handle in person like below.
   *  `ml_pipeline_src_get_tensors_info(srchandle, &info);`
   */
  status = ml_tensors_info_create(&info);
  if (status != ML_ERROR_NONE)
    goto fail_exit;

  status = ml_tensors_info_set_count(info, 1);
  if (status != ML_ERROR_NONE)
    goto fail_exit;

  status = ml_tensors_info_set_tensor_type(info, 0, ML_TENSOR_TYPE_UINT8);
  if (status != ML_ERROR_NONE)
    goto fail_exit;

  status = ml_tensors_info_set_tensor_dimension(info, 0, dim);
  if (status != ML_ERROR_NONE)
    goto fail_exit;

  g_free(res_path);
  g_free(model_path);
  g_free(label_path);
  g_free(box_prior_path);

  return 0;

fail_exit:
  g_free(res_path);
  g_free(model_path);
  g_free(label_path);
  g_free(box_prior_path);

  destroy_pipeline();
  return status;
}

/**
 * @brief ask the privilege
 */
void app_request_multiple_response_cb(ppm_call_cause_e cause,
    const ppm_request_result_e * results, const char **privileges,
    size_t privileges_count, void *user_data) {
  unsigned int allowed_count = 0;

  if (cause == PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to make privilege checker");
    return;
  }
  for (int it = 0; it < privileges_count; ++it) {
    switch (results[it]) {
    case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_ALLOW_FOREVER:
      allowed_count++;
      break;
    case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER:
      break;
    case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
      break;
    }
  }

  if (allowed_count == privileges_count) {
    init_pipeline();
    view_create(user_data);
  }
}

/**
 * @brief check and ask the privilege
 */
void app_check_and_request_permissions()
{
  ppm_check_result_e results[2];
  const char *privileges[] = { "http://tizen.org/privilege/camera",
    "http://tizen.org/privilege/mediastorage" };
  const char *askable_privileges[2];
  int askable_privileges_count = 0;
  unsigned int allowed_count = 0;
  unsigned int privileges_count = sizeof(privileges) / sizeof(privileges[0]);

  int ret = ppm_check_permissions(privileges, privileges_count, results);

  dlog_print(DLOG_DEBUG, LOG_TAG, "PRIVILEGE CHECK IS CALLED");

  if (ret == PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
    for (int it = 0; it < privileges_count; ++it) {
      switch (results[it]) {
      case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ALLOW:
        /* Update UI and start accessing protected functionality */
        dlog_print(DLOG_DEBUG, LOG_TAG, "PRIVILEGE CHECK IS ALLOWED: %s",
            privileges[it]);
        allowed_count++;
        break;
      case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_DENY:
        dlog_print(DLOG_DEBUG, LOG_TAG, "PRIVILEGE CHECK IS DENYED: %s",
            privileges[it]);
        /* Show a message and terminate the application */
        break;
      case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ASK:
        askable_privileges[askable_privileges_count++] = privileges[it];
        dlog_print(DLOG_DEBUG, LOG_TAG, "PRIVILEGE CHECK IS ASKED: %s",
            privileges[it]);
        /* Log and handle errors */
        break;
      }
    }

    if (allowed_count != privileges_count) {
      dlog_print(DLOG_DEBUG, LOG_TAG, "PRIVILEGE CHECK IS REQUESTED");
      ret = ppm_request_permissions(askable_privileges,
          askable_privileges_count, app_request_multiple_response_cb, NULL);
    }
  } else {
    /* ret != PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE */
    dlog_print(DLOG_DEBUG, LOG_TAG, "PRIVILEGE CHECK IS ELSE");
    /* Handle errors */
  }
  dlog_print(DLOG_DEBUG, LOG_TAG, "PRIVILEGE CHECK IS DONE");
}

/**
 * @brief Hook to take necessary actions before main event loop starts.
 * Initialize UI resources and application's data.
 * If this function returns true, the main loop of application starts.
 * If this function returns false, the application is terminated.
 *
 * @param user_data The data passed from the callback registration function
 * @return @c true on success (the main loop is started), @c false otherwise (the app is terminated)
 */
static bool app_create(void *user_data)
{
  app_check_and_request_permissions();
  if (init_pipeline() != 0) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to initialize the pipeline");
    return false;
  }

  data_output_pipe = ecore_pipe_add (update_gui, NULL);
  if (!data_output_pipe) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to make data_output_pipe");
    destroy_pipeline();
    return false;
  }

  view_create(user_data);
  return true;
}

/**
 * @brief This callback function is called when another application
 * sends a launch request to the application.
 *
 * @param app_control The launch request handle (not used here)
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_control(app_control_h app_control, void *user_data)
{
  /* Handle the launch request. */
}

/**
 * @brief This callback function is called each time
 * the application is completely obscured by another application
 * and becomes invisible to the user.
 *
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_pause(void *user_data)
{
  /* Take necessary actions when application becomes invisible. */
  ml_pipeline_stop(handle);
}

/**
 * @brief This callback function is called each time
 * the application becomes visible to the user.
 *
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_resume(void *user_data)
{
  /* Take necessary actions when application becomes visible. */
  ml_pipeline_start(handle);
}

/**
 * @brief This callback function is called once after the main loop of the application exits.
 *
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_terminate(void *user_data)
{
  /* Release all resources. */
  ecore_pipe_del(data_output_pipe);
  destroy_pipeline();
}

/**
 * @brief This function will be called when the language is changed.
 *
 * @param event_info The system event information (not used here)
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
  /* APP_EVENT_LANGUAGE_CHANGED */
  char *locale = NULL;

  system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE, &locale);

  if (locale != NULL) {
    elm_language_set(locale);
    free(locale);
  }
  return;
}

/**
 * @brief Main function of the application.
 */
int main(int argc, char *argv[])
{
  //	g_setenv("GST_DEBUG", "5", true);
  int ret;

  ui_app_lifecycle_callback_s event_callback = {0, };
  app_event_handler_h handlers[5] = {NULL, };

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;

  /**
   * If you want to handle more events,
   * please check the application life cycle guide.
   */
  ui_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, NULL);

  ret = ui_app_main(argc, argv, &event_callback, NULL);
  if (ret != APP_ERROR_NONE)
    dlog_print(DLOG_ERROR, LOG_TAG, "ui_app_main() failed. err = %d", ret);

  return ret;
}
