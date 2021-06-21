/**
 * @file view.h
 * @date 06 December 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#if !defined(_VIEW_H)
#define _VIEW_H
#include <tizen.h>
#include <dlog.h>
#include <app.h>
#include <efl_extension.h>
#include <Elementary.h>

/**
 * @brief Creates essential objects: window, conformant and layout.
 */
Eina_Bool view_create(void *user_data);

/**
 * @brief Creates a basic window named package.
 */
Evas_Object *view_create_win(const char *pkg_name);

/**
 * @brief The callback function to reset the image area
 */
Evas_Object *view_create_conformant_without_indicator(Evas_Object *win);

/**
 * @brief Creates a conformant without indicator for wearable app.
 */
Evas_Object *view_create_naviframe(Evas_Object *win);

/**
 * @brief Adds a log message to the debug box.
 */
void _add_entry_text(const char *text);

#define _PRINT_MSG_LOG_BUFFER_SIZE_ 1024
#define PRINT_MSG(fmt, args...) do { char _log_[_PRINT_MSG_LOG_BUFFER_SIZE_]; \
	snprintf(_log_, _PRINT_MSG_LOG_BUFFER_SIZE_, fmt, ##args); _add_entry_text(_log_); } while (0)

typedef enum {
	GET_LABEL_BTN,
	BUTTON_COUNT
} app_button;


/**
 * @brief Creates a new button.
 */
Evas_Object *_new_button(void *data, Evas_Object *display, char *name, void *cb);

/**
 * @brief Creates a basic display.
 */
Evas_Object *_create_new_cd_display(char *name, void *cb);

/**
 * @brief Called when the current view is popped.
 */
Eina_Bool _pop_cb(void *data, Elm_Object_Item *item);


/**
 * @brief Sets the 'disabled' state of a button.
 */
void _disable_button(app_button button, Eina_Bool disabled);

/**
 * @brief Sets the 'disabled' state of a button.
 */
void _create_button(app_button button, Evas_Object *display, char *name, void *callback);

/**
 * @brief Pops a navi that is on top of the stack
 */
void _pop_navi();

#endif
