/**
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(_DETECT_H)
#define _DETECT_H

#include <glib.h>
//#include <gst/gst.h>
#include <camera.h>
#include "view.h"

#define Y_SCALE         10.0f
#define X_SCALE         10.0f
#define H_SCALE         5.0f
#define W_SCALE         5.0f

#define MODEL_WIDTH     300
#define MODEL_HEIGHT    300

#define BOX_SIZE        4
#define LABEL_SIZE      91
#define DETECTION_MAX   1917

#define MAX_BOXES (128)
struct boxes {
  int num_boxes;
  int img_width;
  int img_height;
  int x1[MAX_BOXES];
  int x2[MAX_BOXES];
  int y1[MAX_BOXES];
  int y2[MAX_BOXES];
  int class[MAX_BOXES];
  char name[MAX_BOXES][80];
  int probability[MAX_BOXES]; // 0 .. 10000
};

struct boxes_evas_objects {
  int num_boxes;
  int num_allocated;
  Evas_Object *box[MAX_BOXES];
  Evas_Object *text[MAX_BOXES];
};

/**
 * @brief Max objects to be displayed in the screen.
 * The application will draw the boxes and labels of detected objects with this limitation.
 */
#define MAX_OBJECT_DETECTION 5

/**
 * @brief Data structure for detected object.
 */
//typedef struct
//{
//  gint x;
//  gint y;
//  gint width;
//  gint height;
//  gint class_id;
//  gfloat prob;
//} ssd_detected_object_s;

/**
 * @brief Data structure for SSD model info.
 */
//typedef struct
//{
//  gchar *model_path;       /**< tflite model file path */
//  gchar *label_path;       /**< label file path */
//  gchar *box_prior_path;   /**< box prior file path */
//  gfloat box_priors[BOX_SIZE][DETECTION_MAX];   /**< box prior */
//  GList *labels;           /**< list of loaded labels */
//} ssd_model_info_s;


typedef struct _CustomData
{
//  GstElement *pipeline;         /**< The running pipeline */
//  GstElement *appsrc;
//  GstElement *appsink;
//  GMainContext *context;        /**< GLib context used to run the main loop */
  int media_width;              /**< The video width */
  int media_height;             /**< The video height */
  int initialized;         /**< To avoid informing the UI multiple times about the initialization */

  Ecore_Pipe * pipe;

//  GMainLoop *main_loop;         /**< GLib main loop */
//  GstElement *video_sink;       /**< The video sink element which receives XOverlay commands */
//  ANativeWindow *native_window; /**< The Android native window where video will be rendered */
//  jobject app;                  /**< Application instance, used to call its methods. A global reference is kept. */
} CustomData;


void gst_initialize (CustomData * data);

int gst_run (CustomData * data, camera_preview_data_s *frame);

void gst_uninitialize (CustomData * data);

#endif
