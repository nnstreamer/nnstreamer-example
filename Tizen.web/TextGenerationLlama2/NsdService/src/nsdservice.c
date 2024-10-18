/**
 * @file	main.c
 * @date 18 October 2024
 * @brief	Tizen native service for hybrid application
 * @author	Yelin Jeong <yelini.jeong@samsung.com>
 * @bug		No known bugs.
 */

#include "nsdservice.h"
#include <bundle.h>
#include <dns-sd.h>
#include <message_port.h>
#include <service_app.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <tizen.h>

/**
 * @brief Service app create callback
 */
static bool _create_cb(void *user_data) {
  dlog_print(DLOG_INFO, LOG_TAG, "Callback create\n");
  return true;
}

/**
 * @brief Service app terminate callback
 */
static void _terminate_cb(void *user_data) {
  dlog_print(DLOG_INFO, LOG_TAG, "Callback terminate\n");
}

/**
 * @brief Service app app control callback
 */
static void _app_control_cb(app_control_h app_control, void *user_data) {
  dlog_print(DLOG_INFO, LOG_TAG, "Callback app_control\n");
}

/**
 * @brief Send dnssd remote service information
 */
void _dnssd_send_found(dnssd_service_h dnssd_remote_service, bool is_available) {
  char *service_name = NULL;
  char *service_type = NULL;
  char *ip_v4_address = NULL;
  char *ip_v6_address = NULL;
  int ret, port = 0;

  ret = dnssd_service_get_type(dnssd_remote_service, &service_type);
  if (ret != DNSSD_ERROR_NONE || service_type == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get service type (%d)", ret);
    return;
  }

  ret = dnssd_service_get_name(dnssd_remote_service, &service_name);
  if (ret != DNSSD_ERROR_NONE || service_name == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get service name (%d)", ret);
    return;
  }

  ret = dnssd_service_get_ip(dnssd_remote_service, &ip_v4_address,
                             &ip_v6_address);
  if (ret != DNSSD_ERROR_NONE || ip_v4_address == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get IP address (%d)", ret);
    return;
  }

  ret = dnssd_service_get_port(dnssd_remote_service, &port);
  if (ret != DNSSD_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get port number (%d)", ret);
    return;
  }

  char port_buffer[CHAR_BIT];
  snprintf(port_buffer, CHAR_BIT, "%d", port);

  bundle *b = bundle_create();
  bundle_add_str(b, "name", service_name);
  bundle_add_str(b, "ip", ip_v4_address);
  bundle_add_str(b, "port", port_buffer);

  if (is_available)
    ret = message_port_send_message(REMOTE_APP_ID, "STATE_AVAILABLE", b);
  else
    ret = message_port_send_message(REMOTE_APP_ID, "STATE_UNAVAILABLE", b);

  if (ret != MESSAGE_PORT_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to send message to %s",
               REMOTE_APP_ID);
  }
  bundle_free(b);

  if (service_name)
    free(service_name);
  if (service_type)
    free(service_type);
  if (ip_v4_address)
    free(ip_v4_address);
  if (ip_v6_address)
    free(ip_v6_address);
}

/**
 * @brief dnssd found callback
 */
void _found_cb(dnssd_service_state_e state,
               dnssd_service_h dnssd_remote_service, void *user_data) {
  switch (state) {
  case DNSSD_SERVICE_STATE_AVAILABLE:
    /* DNS-SD service found */
    _dnssd_send_found(dnssd_remote_service, true);
    break;
  case DNSSD_SERVICE_STATE_UNAVAILABLE:
    /* DNS-SD service becomes unavailable */
    _dnssd_send_found(dnssd_remote_service, false);
    break;
  case DNSSD_SERVICE_STATE_NAME_LOOKUP_FAILED:
    /* Browsing failed */
    dlog_print(DLOG_ERROR, LOG_TAG, "Browse Failure\n");
    break;
  case DNSSD_SERVICE_STATE_HOST_NAME_LOOKUP_FAILED:
    /* Resolving service name failed */
    dlog_print(DLOG_ERROR, LOG_TAG, "Resolve Service Name Failure\n");
    break;
  case DNSSD_SERVICE_STATE_ADDRESS_LOOKUP_FAILED:
    /* Resolving service address failed */
    dlog_print(DLOG_ERROR, LOG_TAG, "Resolve Service Address\n");
    break;
  default:
    dlog_print(DLOG_ERROR, LOG_TAG, "Unknown Browse State\n");
    break;
  }
}

/**
 * @brief Main function.
 */
int main(int argc, char *argv[]) {
  dnssd_browser_h browser_handle;
  char *target = "_nsd_offloading._tcp";
  int ret;
  bool found;

  service_app_lifecycle_callback_s event_callback = {.create = _create_cb,
                                                     .terminate = _terminate_cb,
                                                     .app_control =
                                                         _app_control_cb};

  ret = dnssd_initialize();
  if (ret != DNSSD_ERROR_NONE)
    dlog_print(DLOG_ERROR, LOG_TAG, "Dnssd initialize failed");

  ret = dnssd_start_browsing_service(target, &browser_handle, _found_cb, NULL);
  if (ret == DNSSD_ERROR_NONE)
    dlog_print(DLOG_DEBUG, LOG_TAG, "Start browsing");

  ret = message_port_check_remote_port(REMOTE_APP_ID, "STATE_AVAILABLE", &found);
  if (ret != MESSAGE_PORT_ERROR_NONE || !found) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to check remote port");
  }

  ret = message_port_check_remote_port(REMOTE_APP_ID, "STATE_UNAVAILABLE", &found);
  if (ret != MESSAGE_PORT_ERROR_NONE || !found) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to check remote port");
  }

  return service_app_main(argc, argv, &event_callback, NULL);
}
