/**
 * @file data.h
 * @date 06 December 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#if !defined(_DATA_H)
#define _DATA_H

#include "view.h"

/**
 * @brief Initialize the data component.
 */
void data_initialize(void);

/**
 * @brief Finalize the data component.
 */
void data_finalize(void);

/**
 * @brief Create the buttons for the application
 */
void create_buttons_in_main_window(void);

/**
 * @brief The callback function to reset the image area
 */
void _image_util_reset_cb(void *data, Evas_Object *obj, void *event_info);

#endif
