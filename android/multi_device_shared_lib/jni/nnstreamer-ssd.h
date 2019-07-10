/**
 * @file	nnstreamer-ssd.h
 * @date	1 April 2019
 * @brief	Tensor stream example with TF-Lite model for object detection
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @bug		No known bugs
 */

#ifndef __NNSTREAMER_SSD_H__
#define __NNSTREAMER_SSD_H__

#include <gst/gst.h>

#define Y_SCALE         10.0f
#define X_SCALE         10.0f
#define H_SCALE         5.0f
#define W_SCALE         5.0f

#define MODEL_WIDTH     480
#define MODEL_HEIGHT    480

#define BOX_SIZE        4
#define LABEL_SIZE      91
#define DETECTION_MAX   1917

/**
 * @brief Max objects to be displayed in the screen.
 * The application will draw the boxes and labels of detected objects with this limitation.
 */
#define MAX_OBJECT_DETECTION 5

/**
 * @brief Data structure for detected object.
 */
typedef struct
{
  gint x;
  gint y;
  gint width;
  gint height;
  gint class_id;
  gfloat prob;
} ssd_detected_object_s;

/**
 * @brief Data structure for SSD model info.
 */
typedef struct
{
  gchar *model_path;       /**< tflite model file path */
  gchar *label_path;       /**< label file path */
  gchar *box_prior_path;   /**< box prior file path */
  gfloat box_priors[BOX_SIZE][DETECTION_MAX];   /**< box prior */
  GList *labels;           /**< list of loaded labels */
} ssd_model_info_s;

#endif /* __NNSTREAMER_SSD_H__ */

