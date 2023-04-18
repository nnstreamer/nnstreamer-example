/**
 * @file mobilenetv2.c
 * @date 18 Apr 2023
 * @brief Tizen Native Example App for ML-API-Service
 * @see https://github.com/nnstreamer/nnstreamer
 * @author Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 * @bug No known bugs except for NYI items.
 */

#include "mobilenetv2.h"
#include <glib.h>
#include <nnstreamer.h>
#include <ml-api-service.h>

/**
 * @brief UI related objects
 */
typedef struct appdata {
  Evas_Object *win;
  Evas_Object *conform;
  Evas_Object *label;
} appdata_s;

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
  evas_object_size_hint_weight_set(ad->label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_object_content_set(ad->conform, ad->label);

  evas_object_show(ad->win);
}

/**
 * @brief base code for basic UI app.
 */
static bool
app_create(void *data)
{
  appdata_s *ad = data;

  create_base_gui(ad);

  const gchar *key = "imgcls-mobilenet-224-224-fp";

  dlog_print(DLOG_ERROR, LOG_TAG, "Let's start call ml-api-service apis");
  const char *app_shared_resource_path = app_get_shared_resource_path();
  gchar *shared_resource_model = g_strdup_printf("%s/%s", app_shared_resource_path, "lite-model_mobilenet_v2_100_224_fp32_1.tflite");
  dlog_print(DLOG_ERROR, LOG_TAG, "shared_resource_model: %s", shared_resource_model);

  elm_object_text_set(ad->label, "<align=center>Installed model file `lite-model_mobilenet_v2_100_224_fp32_1.tflite`</align>");

  unsigned int version;

  int ret = ml_service_model_register(key, shared_resource_model, true, "this is mobilenet_v2_224_fp model from mobilenetv2", &version);
  dlog_print(DLOG_ERROR, LOG_TAG, "ret: %d, version: %u", ret, version);

  g_free (shared_resource_model);

  return true;
}

/**
 * @brief base code for basic UI app.
 */
static void
app_control(app_control_h app_control, void *data)
{
  /* Handle the launch request. */
}

/**
 * @brief base code for basic UI app.
 */
static void
app_pause(void *data)
{
  return;
}

/**
 * @brief base code for basic UI app.
 */
static void
app_resume(void *data)
{
  return;
}

/**
 * @brief base code for basic UI app.
 */
static void
app_terminate(void *data)
{
  return;
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
