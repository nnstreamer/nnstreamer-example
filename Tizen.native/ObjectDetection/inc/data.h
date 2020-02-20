/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
