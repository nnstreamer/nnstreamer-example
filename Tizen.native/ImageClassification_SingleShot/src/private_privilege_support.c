/**
 * @file private_privilege_support.c
 * @date 06 December 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include <private_privilege_support.h>

#define BUFLEN 1024

static const char _deny_forever_statement[] =
    "%s To grant it, go to Settings -> Privacy and Security -> Privacy settings -> Privacy settings -> %s.";
static char _deny_forever_user_info[BUFLEN];
static char _deny_once_user_info[BUFLEN];

Evas_Object *_privilege_popup_win = NULL;
private_privilege_permission_cb _privilege_callback;
Evas_Object *privilege_popup = NULL;

/**
 * @brief Maps the privilege to its simple representation.
 *
 * @param       privilege   The privilege that is to be mapped.
 * @return      The simple string representation of the privilege
 */
char *
map_privilege_name(const char *privilege)
{
  if (strcmp(privilege, "http://tizen.org/privilege/account.read") == 0)
    return "Account";
  else if (strcmp(privilege, "http://tizen.org/privilege/account.write") == 0)
    return "Account";
  else if (strcmp(privilege,
          "http://tizen.org/privilege/apphistory.read") == 0)
    return "User history";
  else if (strcmp(privilege, "http://tizen.org/privilege/bookmark.admin") == 0)
    return "Bookmark";
  else if (strcmp(privilege, "http://tizen.org/privilege/calendar.read") == 0)
    return "Calendar";
  else if (strcmp(privilege, "http://tizen.org/privilege/calendar.write") == 0)
    return "Calendar";
  else if (strcmp(privilege, "http://tizen.org/privilege/call") == 0)
    return "Call";
  else if (strcmp(privilege,
          "http://tizen.org/privilege/callhistory.read") == 0)
    return "User history";
  else if (strcmp(privilege,
          "http://tizen.org/privilege/callhistory.write") == 0)
    return "User history";
  else if (strcmp(privilege, "http://tizen.org/privilege/camera") == 0)
    return "Camera";
  else if (strcmp(privilege, "http://tizen.org/privilege/contact.read") == 0)
    return "Contacts";
  else if (strcmp(privilege, "http://tizen.org/privilege/contact.write") == 0)
    return "Contacts";
  else if (strcmp(privilege, "http://tizen.org/privilege/healthinfo") == 0)
    return "Sensor";
  else if (strcmp(privilege, "http://tizen.org/privilege/location") == 0)
    return "Location";
  else if (strcmp(privilege,
          "http://tizen.org/privilege/location.coarse") == 0)
    return "Location";
  else if (strcmp(privilege,
          "http://tizen.org/privilege/location.enable") == 0)
    return "Location";
  else if (strcmp(privilege,
          "http://tizen.org/privilege/mediahistory.read") == 0)
    return "User history";
  else if (strcmp(privilege, "http://tizen.org/privilege/message.read") == 0)
    return "Message";
  else if (strcmp(privilege, "http://tizen.org/privilege/message.write") == 0)
    return "Message";
  else if (strcmp(privilege, "http://tizen.org/privilege/recorder") == 0)
    return "Microphone";
  else if (strcmp(privilege,
          "http://tizen.org/privilege/web-history.admin") == 0)
    return "User history";
  else if (strcmp(privilege, "http://tizen.org/privilege/mediastorage") == 0)
    return "Storage";
  else if (strcmp(privilege,
          "http://tizen.org/privilege/externalstorage") == 0)
    return "Storage";
  return "Unknown privilege";
}

/**
 * @brief Creates message to be displayed in the pop-up
 * @details     The message will be displayed when user denies
 *              the privilege
 *
 * @param       privilege   The privilege which the message concerns
 * @param       forever     Specifies the type of message that is to be set
 * @return      The message that is to be displayed in the pop-up
 */
char *
_create_message(const char *privilege, bool forever)
{
  static char target_message[BUFLEN];
  char *simple_privilege_name = map_privilege_name(privilege);
  if (forever)
    snprintf(target_message, BUFLEN, _deny_forever_statement,
        _deny_forever_user_info, simple_privilege_name);
  else
    strncpy(target_message, _deny_once_user_info, BUFLEN);
  return target_message;
}

/**
 * @brief Invokes function used when the user makes a decision
 * @details Called after the pop-up is dismissed.
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in EFL API.
 *
 * @param   data The data passed to the function(not used here)
 * @param   obj The object for which the 'clicked' event was triggered
 * @param   event_info Privilege which pop-up concerns
 */
void
_dismissed_cb(void *data, Evas_Object * obj, void *event_info)
{
 (*_privilege_callback)((char *) event_info, PRIVATE_PRIVILEGE_RESULT_DENY);
}

/**
 * @brief Makes the pop-up invisible
 * @details Called after 'OK' button of the pop-up is clicked
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in EFL API.
 * @param   data The data passed to the function(not used here)
 * @param   obj The object for which the 'clicked' event was triggered
 * @param   event_info Additional event information
 */
void
_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
  evas_object_hide(privilege_popup);
}

/**
 * @brief Shows a pop-up with information how to change the decision.
 * @details The pop-up will be displayed when the user denies the privilege.
 *          It informs the user how to change decision to grant the privilege.
 *
 * @param   privilege_popup_win   The object used as the pop-up window
 * @param   message               The message to be displayed in the pop-up
 * @param   privilege             The privilege which the pop-up concerns
 */
void
_show_popup(Evas_Object * privilege_popup_win, const char *message,
    const char *privilege)
{
  privilege_popup = elm_popup_add(privilege_popup_win);
  evas_object_size_hint_align_set(privilege_popup, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(privilege_popup, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_popup_orient_set(privilege_popup, ELM_POPUP_ORIENT_CENTER);
  elm_object_text_set(privilege_popup, message);

  Evas_Object *ok_button = elm_button_add(privilege_popup);
  elm_object_text_set(ok_button, "OK");
  evas_object_smart_callback_add(ok_button, "clicked", _clicked_cb, NULL);
  elm_object_part_content_set(privilege_popup, "button1", ok_button);

  evas_object_smart_callback_add(privilege_popup, "dismissed", _dismissed_cb,
     (void *) privilege);
  evas_object_show(privilege_popup);
}

/**
 * @brief       Takes action based on the user decision
 * @details     Called when the application receives a response upon calling ppm_request_permission().
 *
 * @param[in]   cause       The value representing a reason why this callback
 *                          has been called.
 * @param[in]   result      The result of a response triggered by calling ppm_request_permission().
 * @param[in]   privilege   The privilege that has been checked. This pointer is managed by the API and
 *                          it is valid only in the body of the callback function.
 * @param[in]   user_data   User specific data, this pointer has been passed
 *                          to ppm_request_permission().
 */
void
_request_response_cb(ppm_call_cause_e cause, ppm_request_result_e result,
    const char *privilege, void *user_data)
{
  if (cause == PRIVACY_PRIVILEGE_MANAGER_CALL_CAUSE_ERROR) {
    DLOG_PRINT_DEBUG_MSG
       ("ppm_request_response_cb was called because of an error");

    return;
  }

  switch (result) {
    case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_ALLOW_FOREVER:
    {
     (*_privilege_callback)(privilege, PRIVATE_PRIVILEGE_RESULT_ALLOW);
      return;
    }
    case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_FOREVER:
    {
      char *target_message = _create_message(privilege, true);
      _show_popup(_privilege_popup_win, target_message, privilege);
      return;
    }
    case PRIVACY_PRIVILEGE_MANAGER_REQUEST_RESULT_DENY_ONCE:
    {
      char *target_message = _create_message(privilege, false);
      _show_popup(_privilege_popup_win, target_message, privilege);
      return;
    }
    default:
    {
      DLOG_PRINT_DEBUG_MSG
         ("ppm_request_response_cb. Improper result of user response");
    }
  }
}

/**
 * @brief Checks whether the application was granted the privilege,
 *        and whether requesting permission is required.
 *
 * @param   privilege               The privilege that needs to be verified
 * @param   popup_parent            The object used as the parent of the pop-up
 * @param   callback                The callback function which will be invoked when
 *                                  the user makes a decision
 * @param   deny_forever_user_info  The text to be displayed if the user denies
 *                                  the requested access and checks the
 *                                  "Don't repeat" check box.
 *                                  In addition, a short instruction how to grant
 *                                  it in System Settings will be displayed.
 * @param   deny_once_user_info     The text to be displayed if the user denies
 *                                  the requested access and doesn't check
 *                                  the "Don't repeat" check box.
 */
void
check_and_request_permission(const char *privilege, Evas_Object * popup_parent,
    private_privilege_permission_cb callback,
    const char *deny_forever_user_info, const char *deny_once_user_info)
{
  _privilege_popup_win = popup_parent;
  _privilege_callback = callback;
  strncpy(_deny_forever_user_info, deny_forever_user_info, BUFLEN);
  strncpy(_deny_once_user_info, deny_once_user_info, BUFLEN);
  ppm_check_result_e result;

  int ret = ppm_check_permission(privilege, &result);
  if (ret != PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE) {
    DLOG_PRINT_ERROR("ppm_check_permission", ret);
    return;
  }
  switch (result) {
    case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ALLOW:
    {
     (*_privilege_callback)(privilege, PRIVATE_PRIVILEGE_RESULT_ALLOW);
      return;
    }
    case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_DENY:
    {
      char *target_message = _create_message(privilege, true);
      _show_popup(_privilege_popup_win, target_message, privilege);
      break;
    }
    case PRIVACY_PRIVILEGE_MANAGER_CHECK_RESULT_ASK:
    {
      ret = ppm_request_permission(privilege, _request_response_cb, NULL);
      if (ret != PRIVACY_PRIVILEGE_MANAGER_ERROR_NONE)
        DLOG_PRINT_ERROR("ppm_request_permission", ret);
      break;
    }
  }
}
