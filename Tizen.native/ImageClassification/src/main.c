/**
 * @file main.c
 * @date 13 November 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
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
gchar **labels;
gchar * pipeline;

/**
 * @brief Hook to take necessary actions before main event loop starts.
 * Initialize UI resources and application's data.
 * If this function returns true, the main loop of application starts.
 * If this function returns false, the application is terminated.
 *
 * @param user_data The data passed from the callback registration function
 * @return @c true on success (the main loop is started), @c false otherwise (the app is terminated)
 */
static bool app_create(void *user_data) {
  app_check_and_request_permissions();
  init_label_data();
  return true;
}

/**
 * @brief This callback function is called when another application
 * sends a launch request to the application.
 *
 * @param app_control The launch request handle (not used here)
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_control(app_control_h app_control, void *user_data) {
  /* Handle the launch request. */
}

/**
 * @brief This callback function is called each time
 * the application is completely obscured by another application
 * and becomes invisible to the user.
 *
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_pause(void *user_data) {
  /* Take necessary actions when application becomes invisible. */
  ml_pipeline_stop(handle);
}

/**
 * @brief This callback function is called each time
 * the application becomes visible to the user.
 *
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_resume(void *user_data) {
  /* Take necessary actions when application becomes visible. */
  ml_pipeline_start(handle);
}

/**
 * @brief This callback function is called once after the main loop of the application exits.
 *
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void app_terminate(void *user_data) {
  /* Release all resources. */
  ml_pipeline_sink_unregister(sinkhandle);
  ml_pipeline_src_release_handle(srchandle);
  ml_pipeline_destroy(handle);
  ml_tensors_info_destroy(info);

  g_free(pipeline);
  g_free(inArray);
  g_strfreev(labels);
}

/**
 * @brief This function will be called when the language is changed.
 *
 * @param event_info The system event information (not used here)
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void ui_app_lang_changed(app_event_info_h event_info, void *user_data) {
  /* APP_EVENT_LANGUAGE_CHANGED */
  char *locale = NULL;

  system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE,
      &locale);

  if (locale != NULL) {
    elm_language_set(locale);
    free(locale);
  }
  return;
}

/**
 * @brief Initialize the pipeline with model path
 */
void init_pipeline() {
  char *res_path = app_get_resource_path();
  gchar * model_path = g_strdup_printf("%s/mobilenet_v1_1.0_224_quant.tflite",
      res_path);
  ml_tensor_dimension dim = { CH, CAM_WIDTH, CAM_HEIGHT, 1 };

  pipeline =
      g_strdup_printf(
          "appsrc name=srcx ! "
              "video/x-raw,format=RGB,width=%d,height=%d,framerate=0/1 ! "
              "videoconvert ! videoscale ! video/x-raw,format=RGB,width=%d,height=%d ! "
              "videoflip method=clockwise ! "
              "tensor_converter ! tensor_filter framework=tensorflow-lite model=\"%s\" ! "
              "tensor_sink name=sinkx", CAM_WIDTH, CAM_HEIGHT,
          MODEL_WIDTH, MODEL_HEIGHT, model_path);

  inArray = (uint8_t *) g_malloc(sizeof(uint8_t) * CAM_WIDTH * CAM_HEIGHT * CH);

  ml_pipeline_construct(pipeline, NULL, NULL, &handle);

  ml_pipeline_src_get_handle(handle, "srcx", &srchandle);
  ml_pipeline_sink_register(handle, "sinkx", get_label_from_tensor_sink, NULL,
      &sinkhandle);

  /**
   *  The media type `video/x-raw` cannot get valid info handle, you should set info handle in person like below.
   *  `ml_pipeline_src_get_tensors_info(srchandle, &info);`
   */

  ml_tensors_info_create(&info);
  ml_tensors_info_set_count(info, 1);
  ml_tensors_info_set_tensor_type(info, 0, ML_TENSOR_TYPE_UINT8);
  ml_tensors_info_set_tensor_dimension(info, 0, dim);

  g_free(model_path);
  free(res_path);
}

/**
 * @brief ask the privilege
 */
void app_request_multiple_response_cb(ppm_call_cause_e cause,
    ppm_request_result_e * results, const char **privileges,
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
      PRINT_MSG("Without Privilege, cannot start service.");
      break;
    case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
      PRINT_MSG("Without Privilege, cannot start service.");
      break;
    }
  }

  if (allowed_count == privileges_count) {
    init_pipeline();
    view_create(user_data);
  }
}

/**
 * @brief ask the privilege
 */
void app_check_and_request_permissions() {
  ppm_check_result_e results[1];
  const char *privileges[] = { "http://tizen.org/privilege/camera" };
  char *askable_privileges[1];
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

    if (allowed_count == privileges_count) {
      init_pipeline();
      view_create(NULL);
    } else {
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
 * @brief get label data from the file
 */
void init_label_data() {
  GError *err = NULL;
  gchar *contents = NULL;
  char *res_path = app_get_resource_path();
  gchar * label_path = g_strdup_printf("%s/labels.txt", res_path);

  if (!g_file_get_contents(label_path, &contents, NULL, &err)) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Unable to read file %s with error %s.",
        label_path, err->message);
    g_clear_error(&err);
  } else {
    labels = g_strsplit(contents, "\n", -1);
  }
  g_free(label_path);
  g_free(contents);
  free(res_path);
}

/**
 * @brief Main function of the application.
 */
int main(int argc, char *argv[]) {
  int ret;

  ui_app_lifecycle_callback_s event_callback = { 0, };
  app_event_handler_h handlers[5] = { NULL, };

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;

  /**
   * If you want to handle more events,
   * please check the application life cycle guide.
   */
  ui_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED],
      APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, NULL);

  ret = ui_app_main(argc, argv, &event_callback, NULL);
  if (ret != APP_ERROR_NONE)
    dlog_print(DLOG_ERROR, LOG_TAG, "ui_app_main() failed. err = %d", ret);

  return ret;
}
