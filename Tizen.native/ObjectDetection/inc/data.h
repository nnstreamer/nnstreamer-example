/**
 * @file data.h
 * @date 28 Feb 2020
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author Parichay Kapoor <pk.kapoor@samsung.com>
 * @bug No known bugs except for NYI items
 */

#if !defined(_DATA_H)
#define _DATA_H

#include "view.h"
#include <glib.h>

#define CAM_WIDTH     640
#define CAM_HEIGHT    480
#define MODEL_WIDTH   300
#define MODEL_HEIGHT  300
#define CH            3

extern ml_pipeline_h handle;
extern ml_pipeline_src_h srchandle;
extern ml_pipeline_sink_h sinkhandle;
extern ml_tensors_info_h info;

extern uint8_t * inArray;
extern gchar **labels;
extern gchar * pipeline;

void create_buttons_in_main_window(void);
void update_gui(void * data, void * buf, unsigned int size);

#endif
