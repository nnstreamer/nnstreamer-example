/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * NNStreamer example for orientation detection using tensorflow-lite model
 * and tensor_src_tizensensor element
 * Copyright (C) 2020 Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 */
/**
 * @file    orientationdetection.c
 * @date    15 Jul 2020
 * @brief   Tizen Native (wearable) Example for orientation detection
 *          with NNStreamer/C-API and tizensensor
 * @author  Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 * @bug     No known bugs.
 * 
 * NNStreamer example for orientation detection using
 * TensorFlow Lite model in Tizen wearable device
 * 
 * Used pipeline:
 * tensor_src_tizensensor (accelerometer) -- tensor_filter -- tensor_sink
 */

#include <glib.h>
#include <nnstreamer.h>

#include "orientationdetection.h"

/**
 * @brief UI and pipe related objects
 */
typedef struct appdata {
  Evas_Object *win;
  Evas_Object *conform;
  Evas_Object *label;
  Ecore_Pipe *data_output_pipe;
  ml_pipeline_h pipeline_handle;
  ml_pipeline_sink_h sink_handle;
  int orientation;
} appdata_s;

static appdata_s app_data = {0,};

/**
 * @brief get output from the tensor_sink and make call for gui update
 */
void
get_output_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  int ret;
  size_t data_size;
  float *output;
  appdata_s *ad = &app_data;
  int orientation;

  ret = ml_tensors_data_get_tensor_data (data, 0, (void **) &output, &data_size);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_tensors_data_get_tensor_data failed");
    return;
  }

  orientation = 0;
  for (int i = 1; i < 4; ++i) {
    if (output[orientation] < output[i])
      orientation = i;
  }

  orientation = orientation * 3;
  if (orientation == 0)
    orientation = 12;

  ad->orientation = orientation;
  ecore_pipe_write (ad->data_output_pipe, NULL, 0);
}

/**
 * @brief Destroy the pipeline and related elements
 */
void
destroy_pipeline (appdata_s *ad)
{
  ml_pipeline_sink_unregister (ad->sink_handle);
  ml_pipeline_destroy (ad->pipeline_handle);
}

/**
 * @brief Initialize the pipeline with model path
 * @return 0 on success, -errno on error
 */
int
init_pipeline (appdata_s *ad)
{
  int status = -1;
  gchar *res_path = app_get_resource_path ();
  gchar *model_path = g_strdup_printf ("%s/%s", res_path, "orientation_detection.tflite");
  gchar *pipeline = g_strdup_printf (
      "tensor_src_tizensensor type=accelerometer framerate=10/1 ! "
      "tensor_filter framework=tensorflow-lite model=%s ! "
      "tensor_sink name=sinkx sync=true", model_path
  );

  status = ml_pipeline_construct (pipeline, NULL, NULL, &ad->pipeline_handle);
  if (status != ML_ERROR_NONE) {
    destroy_pipeline (ad);
    goto fail_exit;
  }

  status = ml_pipeline_sink_register (ad->pipeline_handle, "sinkx",
      get_output_from_tensor_sink, NULL, &ad->sink_handle);
  if (status != ML_ERROR_NONE) {
    destroy_pipeline (ad);
    goto fail_exit;
  }

fail_exit:
  g_free (res_path);
  g_free (model_path);
  g_free (pipeline);

  return status;
}

/**
 * @brief base code generated from tizen-studio
 */
static void
win_delete_request_cb (void *data, Evas_Object *obj, void *event_info)
{
  ui_app_exit ();
}

/**
 * @brief base code generated from tizen-studio
 */
static void
win_back_cb (void *data, Evas_Object *obj, void *event_info)
{
  appdata_s *ad = data;
  elm_win_lower (ad->win);
}

/**
 * @brief Update the UI text
 * @param data The data passed from the callback registration function (not used here)
 * @param buf The data passed from pipe (not used here)
 * @param size The data size (not used here)
 */
void
update_gui (void *data, void *buf, unsigned int size)
{
  appdata_s *ad = &app_data;
  const gchar *text = g_strdup_printf (
      "<align=center><br><br><font_size=100>%d<br>UP</font_size></align>",
      ad->orientation);
  elm_object_text_set (ad->label, text);
}

/**
 * @brief Creates essential objects: window, conformant and layout.
 * @param ad The passed app data
 */
static void
create_base_gui (appdata_s *ad)
{
  ad->win = elm_win_util_standard_add (PACKAGE, PACKAGE);
  elm_win_autodel_set (ad->win, EINA_TRUE);

  evas_object_smart_callback_add (ad->win, "delete,request",
      win_delete_request_cb, NULL);
  eext_object_event_callback_add (ad->win, EEXT_CALLBACK_BACK,
      win_back_cb, ad);

  ad->conform = elm_conformant_add (ad->win);
  elm_win_indicator_mode_set (ad->win, ELM_WIN_INDICATOR_SHOW);
  elm_win_indicator_opacity_set (ad->win, ELM_WIN_INDICATOR_OPAQUE);
  evas_object_size_hint_weight_set (ad->conform, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_win_resize_object_add (ad->win, ad->conform);

  evas_object_show (ad->conform);

  ad->label = elm_label_add (ad->conform);
  evas_object_size_hint_weight_set (ad->label,
      EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set (ad->label, EVAS_HINT_FILL, 0.0);
  elm_object_content_set (ad->conform, ad->label);

  evas_object_show (ad->win);

  ad->data_output_pipe = ecore_pipe_add (update_gui, NULL);
  if (!ad->data_output_pipe) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to make data_output_pipe");
  }
}

/**
 * @brief Hook to take necessary actions before main event loop starts.
 * Initialize UI resources and application's data.
 * If this function returns true, the main loop of application starts.
 * If this function returns false, the application is terminated.
 * @param user_data The data passed from the callback registration function
 * @return @c true on success (the main loop is started), @c false otherwise (the app is terminated)
 */
static bool
app_create (void *data)
{
  appdata_s *ad = data;

  int ret;
  ret = init_pipeline (ad);

  if (ret != ML_ERROR_NONE)
    return false;
  create_base_gui (ad);

  return true;
}

/**
 * @brief This callback function is called when another application
 * sends a launch request to the application.
 * @param app_control The launch request handle (not used here)
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void
app_control (app_control_h app_control, void *data)
{
  return;
}

/**
 * @brief This callback function is called each time
 * the application is completely obscured by another application
 * and becomes invisible to the user.
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void
app_pause (void *data)
{
  ml_pipeline_stop (app_data.pipeline_handle);
}

/**
 * @brief This callback function is called each time
 * the application becomes visible to the user.
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void
app_resume (void *data)
{
  ml_pipeline_start (app_data.pipeline_handle);
}

/**
 * @brief This callback function is called once after the main loop of the application exits.
 * @param user_data The data passed from the callback registration function (not used here)
 */
static void
app_terminate (void *data)
{
  appdata_s *ad = data;
  ecore_pipe_del (ad->data_output_pipe);
  destroy_pipeline (ad);
}

/**
 * @brief Main function of the application.
 */
int
main (int argc, char *argv[])
{
  appdata_s *ad = &app_data;
  int ret = 0;

  ui_app_lifecycle_callback_s event_callback = {0,};

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;

  ret = ui_app_main (argc, argv, &event_callback, ad);
  if (ret != APP_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
  }

  return ret;
}
