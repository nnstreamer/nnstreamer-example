/**
 * @file my_file_selector.h
 * @date 06 December 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */
#ifndef MY_FILE_SELECTOR_H
#define MY_FILE_SELECTOR_H

#include "view.h"

#define PATHLEN 256

typedef void (*file_selector_selected_cb) (const char *file_path, void *data);

/**
* @brief This struct store the selected file path
*/
typedef struct file_selector_data_s {
	char dir_path[PATHLEN];
	char root_path[PATHLEN];
	const char *file_extension;
	Evas_Object *file_popup;
	Evas_Object *folder_path;
	file_selector_selected_cb selected_callback;
	void *extra_data;
} file_selector_data_s;
file_selector_data_s file_selector_data;

Evas_Object *create_file_popup(Evas_Object *file_popup,
							   file_selector_selected_cb selected_callback, void *data);
void fill_file_list(Evas_Object *file_list);
void set_file_extension(const char *extension);
void set_file_selected_callback(file_selector_selected_cb selected_callback);

#endif
