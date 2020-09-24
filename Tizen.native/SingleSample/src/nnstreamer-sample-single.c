/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * Copyright (c) 2019 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * @file nnstreamer-sample-single.c
 * @date 11 October 2019
 * @brief NNStreamer/C-API SingleShot example.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include <nnstreamer-sample-single.h>

/**
 * @brief application data structure
 */
typedef struct appdata
{
  Evas_Object *win;
  Evas_Object *conform;
  Evas_Object *label;

  /* NNStreamer handle and tensors info */
  ml_single_h single;
  ml_tensors_info_h in_info;
} appdata_s;

/**
 * @brief Function to initialize NNStreamer SingleShot handle.
 */
static bool
nns_single_initialize (appdata_s * ad)
{
  char test_model[512];
  char *shared_path = app_get_resource_path ();
  int status;

  /**
   * Tensorflow-lite model for image classification ()
   *
   * tensor info (mobilenet_v1_1.0_224_quant.tflite)
   * input[0] >> type:5 (uint8), dim[3:224:224:1] video stream (RGB 224x224)
   * output[0] >> type:5 (uint8), dim[1001:1] LABEL_SIZE
   */
  snprintf (test_model, 512, "%s%s", shared_path,
      "mobilenet_v1_1.0_224_quant.tflite");
  dlog_print (DLOG_ERROR, LOG_TAG, "Test modal path: %s", test_model);

  /* Create SingleShot handle */
  status = ml_single_open (&ad->single, test_model, NULL, NULL,
      ML_NNFW_TYPE_TENSORFLOW_LITE, ML_NNFW_HW_ANY);
  if (status != ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG,
        "Failed to create single handle with model %s.", test_model);
    return false;
  }

  status = ml_single_get_input_info (ad->single, &ad->in_info);
  if (status != ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG,
        "Failed to get the input tensors information.");
    ml_single_close (ad->single);
    return false;
  }

  return true;
}

/**
 * @brief Function to close NNStreamer SingleShot handle.
 */
static void
nns_single_close (appdata_s * ad)
{
  int status;

  if (ad) {
    /* Close all handles */
    status = ml_single_close (ad->single);
    if (status != ML_ERROR_NONE) {
      dlog_print (DLOG_ERROR, LOG_TAG, "Failed to close single handle.");
    }

    ml_tensors_info_destroy (ad->in_info);
  }
  else
  {
    dlog_print (DLOG_ERROR, LOG_TAG, "Invalid state, the appdata is null.");
  }
}

/**
 * @brief Sample code to invoke Tensorflow-lite model with NNStreamer SingleShot handle.
 */
static void
nns_single_invoke (appdata_s * ad)
{
  ml_tensors_data_h input, output;
  int status;
  void *data_ptr;
  size_t data_size;

  if (!ad) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Invalid state, the appdata is null.");
    return;
  }

  /* generate dummy data */
  ml_tensors_data_create (ad->in_info, &input);

  status = ml_single_invoke (ad->single, input, &output);
  if (status == ML_ERROR_NONE) {
    ml_tensors_data_get_tensor_data (output, 0, &data_ptr, &data_size);
    dlog_print (DLOG_INFO, LOG_TAG, "Invoked, received data size %d",
        data_size);
  } else {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to invoke test model.");
  }

  ml_tensors_data_destroy (output);
  ml_tensors_data_destroy (input);
}

/**
 * @brief base code generated from tizen-studio
 */
static void
win_delete_request_cb (void *data, Evas_Object * obj, void *event_info)
{
  ui_app_exit ();
}

/**
 * @brief base code generated from tizen-studio
 */
static void
win_back_cb (void *data, Evas_Object * obj, void *event_info)
{
  appdata_s *ad = data;
  /* Let window go to hide state. */
  elm_win_lower (ad->win);
}

/**
 * @brief base code generated from tizen-studio
 */
static void
create_base_gui (appdata_s * ad)
{
  /**
   * Window
   * Create and initialize elm_win.
   * elm_win is mandatory to manipulate window.
   */
  ad->win = elm_win_util_standard_add (PACKAGE, PACKAGE);
  elm_win_autodel_set (ad->win, EINA_TRUE);

  if (elm_win_wm_rotation_supported_get (ad->win)) {
    int rots[4] = { 0, 90, 180, 270 };
    elm_win_wm_rotation_available_rotations_set (ad->win,
        (const int *) (&rots), 4);
  }

  evas_object_smart_callback_add (ad->win, "delete,request",
      win_delete_request_cb, NULL);
  eext_object_event_callback_add (ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

  /**
   * Conformant
   * Create and initialize elm_conformant.
   * elm_conformant is mandatory for base gui to have proper size
   * when indicator or virtual keypad is visible.
   */
  ad->conform = elm_conformant_add (ad->win);
  elm_win_indicator_mode_set (ad->win, ELM_WIN_INDICATOR_SHOW);
  elm_win_indicator_opacity_set (ad->win, ELM_WIN_INDICATOR_OPAQUE);
  evas_object_size_hint_weight_set (ad->conform, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_win_resize_object_add (ad->win, ad->conform);
  evas_object_show (ad->conform);

  /**
   * Label
   * Create an actual view of the base gui.
   * Modify this part to change the view.
   */
  ad->label = elm_label_add (ad->conform);
  elm_object_text_set (ad->label,
      "<align=center>NNStreamer SingleShot Sample</align>");
  evas_object_size_hint_weight_set (ad->label, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_object_content_set (ad->conform, ad->label);

  /* Show window after base gui is set up */
  evas_object_show (ad->win);
}

/**
 * @brief base code generated from tizen-studio
 */
static bool
app_create (void *data)
{
  /**
   * Hook to take necessary actions before main event loop starts
   * Initialize UI resources and application's data
   * If this function returns true, the main loop of application starts
   * If this function returns false, the application is terminated
   */
  appdata_s *ad = (appdata_s *) data;

  create_base_gui (ad);
  return true;
}

/**
 * @brief base code generated from tizen-studio
 */
static void
app_control (app_control_h app_control, void *data)
{
  /* Handle the launch request. */
}

/**
 * @brief base code generated from tizen-studio
 */
static void
app_pause (void *data)
{
  /* Take necessary actions when application becomes invisible. */
}

/**
 * @brief base code generated from tizen-studio
 */
static void
app_resume (void *data)
{
  /* Take necessary actions when application becomes visible. */
}

/**
 * @brief base code generated from tizen-studio
 */
static void
app_terminate (void *data)
{
  /* Release all resources. */
  nns_single_close ((appdata_s *) data);
}

/**
 * @brief base code generated from tizen-studio
 */
static void
ui_app_lang_changed (app_event_info_h event_info, void *user_data)
{
  /* APP_EVENT_LANGUAGE_CHANGED */

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
 * @brief base code generated from tizen-studio
 */
static void
ui_app_orient_changed (app_event_info_h event_info, void *user_data)
{
  /* APP_EVENT_DEVICE_ORIENTATION_CHANGED */
  return;
}

/**
 * @brief base code generated from tizen-studio
 */
static void
ui_app_region_changed (app_event_info_h event_info, void *user_data)
{
  /* APP_EVENT_REGION_FORMAT_CHANGED */
}

/**
 * @brief base code generated from tizen-studio
 */
static void
ui_app_low_battery (app_event_info_h event_info, void *user_data)
{
  /* APP_EVENT_LOW_BATTERY */
}

/**
 * @brief base code generated from tizen-studio
 */
static void
ui_app_low_memory (app_event_info_h event_info, void *user_data)
{
  /* APP_EVENT_LOW_MEMORY */
}

/**
 * @brief main function
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

  /* Initialize SingleShot handle and tensors info */
  nns_single_initialize (&ad);

  /* Invoke test model */
  nns_single_invoke (&ad);

  ret = ui_app_main (argc, argv, &event_callback, &ad);
  if (ret != APP_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
  }

  return ret;
}
