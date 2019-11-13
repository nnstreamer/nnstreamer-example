/**
 * @file view.h
 * @date 13 November 2019
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

Eina_Bool view_create(void *user_data);
Evas_Object *view_create_win(const char *pkg_name);
Evas_Object *view_create_layout(Evas_Object *parent, const char *file_path, const char *group_name, Eext_Event_Cb cb_function, void *user_data);
Evas_Object *view_create_conformant_without_indicator(Evas_Object *win);
Evas_Object *view_create_naviframe(Evas_Object *win);
void view_destroy(void);
void view_destroy_layout(Evas_Object *layout);
void _add_entry_text(const char *text);

#define _PRINT_MSG_LOG_BUFFER_SIZE_ 1024
#define PRINT_MSG(fmt, args...) do { char _log_[_PRINT_MSG_LOG_BUFFER_SIZE_]; \
    snprintf(_log_, _PRINT_MSG_LOG_BUFFER_SIZE_, fmt, ##args); _add_entry_text(_log_); } while (0)

Evas_Object *_new_button(Evas_Object *display, char *name, void *cb);
Evas_Object *_create_new_cd_display(char *name, void *cb);

#endif
