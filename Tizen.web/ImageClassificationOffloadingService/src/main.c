/**
 * @file	main.c
 * @date	04 Jul 2024
 * @brief	Tizen native service for hybrid application
 * @author	Yelin Jeong <yelini.jeong@samsung.com>
 * @bug		No known bugs.
 */

#include "main.h"
#include <bundle.h>
#include <message_port.h>
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

  bool found;
  int ret = message_port_check_remote_port(REMOTE_APP_ID, PORT_NAME, &found);

  if (ret != MESSAGE_PORT_ERROR_NONE || !found) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to check remote port");
  }

  /* Todo: Send network information to ui application */
  bundle *b = bundle_create();
  bundle_add_str(b, "command", "connected");
  ret = message_port_send_message(REMOTE_APP_ID, PORT_NAME, b);
  if (ret != MESSAGE_PORT_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to send message to %s:%s",
               REMOTE_APP_ID, PORT_NAME);
  }
  bundle_free(b);

  return service_app_main(argc, argv, &event_callback, NULL);
}
