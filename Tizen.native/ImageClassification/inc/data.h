/**
 * @file data.h
 * @date 13 November 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#if !defined(_DATA_H)
#define _DATA_H

#include "view.h"
#include <glib.h>
#include <dlog.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "ImageClassification"

#define CAM_WIDTH     640
#define CAM_HEIGHT    480
#define MODEL_WIDTH   224
#define MODEL_HEIGHT  224
#define CH            3
#define TFLITE_FRAMEWORK 0
#define NNFW_FRAMEWORK 1

extern ml_pipeline_h handle;
extern ml_pipeline_src_h srchandle;
extern ml_pipeline_sink_h sinkhandle_1;
extern ml_pipeline_sink_h sinkhandle_2;
extern ml_tensors_info_h info;

extern uint8_t * inArray;
extern gchar **labels;
extern gchar * pipeline;

void create_buttons_in_main_window(void);
void get_label_from_tensor_sink(const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data);

#endif
