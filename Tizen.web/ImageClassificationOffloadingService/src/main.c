/**
 * @file	main.c
 * @date	04 Jul 2024
 * @brief	Tizen native service for hybrid application
 * @author	Yelin Jeong <yelini.jeong@samsung.com>
 * @bug		No known bugs.
 */

#include "main.h"
#include <service_app.h>
#include <tizen.h>

/**
 * @brief Service app create callback
 */
static bool _create_cb(void* user_data){
  dlog_print(DLOG_INFO, LOG_TAG, "Callback create\n");
  return true;
}

/**
 * @brief Service app terminate callback
 */
static void _terminate_cb(void* user_data){
  dlog_print(DLOG_INFO, LOG_TAG, "Callback terminate\n");
}

/**
 * @brief Service app app control callback
 */
static void _app_control_cb(app_control_h app_control, void* user_data){
  dlog_print(DLOG_INFO, LOG_TAG, "Callback app_control\n");
}

/**
 * @brief Main function.
 */
int main(int argc, char *argv[]) {
  service_app_lifecycle_callback_s event_callback = {
    .create = _create_cb,
    .terminate = _terminate_cb,
    .app_control = _app_control_cb
  };

  dlog_print(DLOG_ERROR, LOG_TAG,
             "Image classification offloading service start");
  return service_app_main(argc, argv, &event_callback, NULL);
}
