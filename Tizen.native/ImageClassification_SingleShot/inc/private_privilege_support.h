/**
 * @file private_privilege_support.h
 * @date 06 December 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#ifndef PRIVATE_PRIVILEGE_SUPPORT_H_
#define PRIVATE_PRIVILEGE_SUPPORT_H_

#include <stddef.h>
#include <privacy_privilege_manager.h>

#include "view.h"
#include "main.h"

/**
 * @brief Enumeration for user's response to privilege permission request.
 */
typedef enum {
	/* The application was granted the privilege. */
	PRIVATE_PRIVILEGE_RESULT_ALLOW,
	/* The application was denied the privilege. */
	PRIVATE_PRIVILEGE_RESULT_DENY,
} private_privilege_permission_result;


/**
 * @brief Called to inform about user's decision regarding granting a privilege.
 * @param   privilege             The privilege which the decision concerns.
 * @param   result                The user's response to privilege permission request.
 */
typedef void (*private_privilege_permission_cb) (const char *privilege,
		private_privilege_permission_result result);

/**
 * @brief Maps the privilege to its simple representation.
 *
 * @param       privilege   The privilege that is to be mapped.
 * @return      The simple string representation of the privilege
 */
char *map_privilege_name(const char* privilege);

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
void check_and_request_permission(const char *privilege, Evas_Object *popup_parent,
		private_privilege_permission_cb callback, const char *deny_forever_user_info,
		const char *deny_once_user_info);

#endif /* PRIVATE_PRIVILEGE_SUPPORT_H_ */
