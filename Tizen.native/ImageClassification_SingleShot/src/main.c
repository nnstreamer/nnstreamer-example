/**
 * @file main.c
 * @date 06 December 2019
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

#include "main.h"
#include "view.h"
#include "data.h"

static const char *mediastorage_privilege =
    "http://tizen.org/privilege/mediastorage";

/**
 * @brief Hook to take necessary actions before main event loop starts.
 * Initialize UI resources and application's data.
 * If this function returns true, the main loop of application starts.
 * If this function returns false, the application is terminated.
 *
 * @param user_data The data passed from the callback registration function
 * @return @c true on success(the main loop is started), @c false otherwise(the app is terminated)
 */
static bool
app_create(void *user_data)
{
  view_create(user_data);
  data_initialize();
  return true;
}

/**
 * @brief This callback function is called when another application
 * sends a launch request to the application.
 *
 * @param app_control The launch request(not used here)
 * @param user_data The data passed from the callback registration function(not used here)
 */
static void
app_control(app_control_h app_control, void *user_data)
{
  /* Handle the launch request. */
}

/**
 * @brief This callback function is called each time
 * the application is completely obscured by another application
 * and becomes invisible to the user.
 *
 * @param user_data The data passed from the callback registration function(not used here)
 */
static void
app_pause(void *user_data)
{
  /* Take necessary actions when application becomes invisible. */
}

/**
 * @brief Verifies if permission to the privilege is granted
 * @details This callback function is called each time
 * the application becomes visible to the user.
 *
 * @param user_data The data passed from the callback registration function(not used here)
 */
static void
app_resume(void *user_data)
{
  verify_permission_granted(mediastorage_privilege);
}

/**
 * @brief This callback function is called once after the main loop of the application exits.
 *
 * @param user_data The data passed from the callback registration function(not used here)
 */
static void
app_terminate(void *user_data)
{
  /* Release all resources. */
  _pop_navi();
  data_finalize();
}

/**
 * @brief This function will be called when the language is changed.
 *
 * @param event_info The system event information(not used here)
 * @param user_data The data passed from the callback registration function(not used here)
 */
static void
ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
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
 * @brief Main function of the application.
 */
int
main(int argc, char *argv[])
{
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
