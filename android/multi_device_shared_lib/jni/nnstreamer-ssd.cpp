/**
 * @file	nnstreamer-ssd.cpp
 * @date	1 April 2019
 * @brief	Tensor stream example with TF-Lite model for object detection
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @bug		No known bugs
 *
 * Before running this example, SSD model and labels should be in an internal storage of Android device.
 * /sdcard/nnstreamer/tflite_ssd/
 *
 * Required model and labels are stored at below link
 * https://github.com/nnsuite/testcases/tree/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco
 */

#include <vector>
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <algorithm>
#include <math.h>
#include "nnstreamer-ssd.h"

ssd_model_info_s ssd_model_info;
GMutex ssd_mutex;
std::vector<ssd_detected_object_s> ssd_detected_objects;

/**
 * @brief Read strings from file.
 */
static gboolean
read_lines (const gchar * file_name, GList ** lines)
{
  std::ifstream file (file_name);
  if (!file) {
    GST_ERROR ("Failed to open file %s", file_name);
    return FALSE;
  }

  std::string str;
  while (std::getline (file, str)) {
    *lines = g_list_append (*lines, g_strdup (str.c_str ()));
  }

  return TRUE;
}

/**
 * @brief Load box priors.
 */
static gboolean
load_box_priors (void)
{
  GList *box_priors = NULL;
  gchar *box_row;

  if (!read_lines (ssd_model_info.box_prior_path, &box_priors)) {
    GST_ERROR ("Failed to load box prior");
    return FALSE;
  }

  for (int row = 0; row < BOX_SIZE; row++) {
    int column = 0;
    int i = 0, j = 0;
    char buff[11];

    memset (buff, 0, 11);
    box_row = (gchar *) g_list_nth_data (box_priors, row);

    while ((box_row[i] != '\n') && (box_row[i] != '\0')) {
      if (box_row[i] != ' ') {
        buff[j] = box_row[i];
        j++;
      } else {
        if (j != 0) {
          ssd_model_info.box_priors[row][column++] = atof (buff);
          memset (buff, 0, 11);
        }
        j = 0;
      }
      i++;
    }

    ssd_model_info.box_priors[row][column++] = atof (buff);
  }

  g_list_free_full (box_priors, g_free);
  return TRUE;
}

/**
 * @brief Load labels.
 */
static gboolean
load_labels (void)
{
  return read_lines (ssd_model_info.label_path, &ssd_model_info.labels);
}

/**
 * @brief Free SSD model info.
 */
extern "C" void
ssd_free (void)
{
  if (ssd_model_info.model_path) {
    g_free (ssd_model_info.model_path);
    ssd_model_info.model_path = NULL;
  }

  if (ssd_model_info.label_path) {
    g_free (ssd_model_info.label_path);
    ssd_model_info.label_path = NULL;
  }

  if (ssd_model_info.box_prior_path) {
    g_free (ssd_model_info.box_prior_path);
    ssd_model_info.box_prior_path = NULL;
  }

  if (ssd_model_info.labels) {
    g_list_free_full (ssd_model_info.labels, g_free);
    ssd_model_info.labels = NULL;
  }

  g_mutex_clear (&ssd_mutex);
  ssd_detected_objects.clear ();
}

/**
 * @brief Init SSD model info.
 */
extern "C" gboolean
ssd_init (void)
{
  const gchar model_path[] = "/sdcard/nnstreamer/tflite_model";
  const gchar ssd_model[] = "ssd_mobilenet_v2_coco.tflite";
  const gchar ssd_label[] = "coco_labels_list.txt";
  //const gchar ssd_label[] = "labels_hand.txt";
  const gchar ssd_box_priors[] = "box_priors.txt";

  memset (&ssd_model_info, 0, sizeof (ssd_model_info_s));

  ssd_model_info.model_path = g_strdup_printf ("%s/%s", model_path, ssd_model);
  ssd_model_info.label_path = g_strdup_printf ("%s/%s", model_path, ssd_label);
  ssd_model_info.box_prior_path = g_strdup_printf ("%s/%s", model_path, ssd_box_priors);

  /* Check model and label files */
  if (!g_file_test (ssd_model_info.model_path, G_FILE_TEST_IS_REGULAR) ||
      !g_file_test (ssd_model_info.label_path, G_FILE_TEST_IS_REGULAR) ||
      !g_file_test (ssd_model_info.box_prior_path, G_FILE_TEST_IS_REGULAR)) {
    GST_ERROR ("Failed to init model info");
    goto error;
  }

  /* Load labels */
  if (!load_box_priors () || !load_labels ()) {
    GST_ERROR ("Failed to load labels");
    goto error;
  }

  g_mutex_init (&ssd_mutex);
  ssd_detected_objects.clear ();
  return TRUE;

error:
  ssd_free ();
  return FALSE;
}

/**
 * @brief Compare score of detected objects.
 */
static bool
compare_objs (ssd_detected_object_s &a, ssd_detected_object_s &b)
{
  return a.prob > b.prob;
}

/**
 * @brief Intersection of union
 */
static gfloat
iou (ssd_detected_object_s &A, ssd_detected_object_s &B)
{
  int x1 = std::max (A.x, B.x);
  int y1 = std::max (A.y, B.y);
  int x2 = std::min (A.x + A.width, B.x + B.width);
  int y2 = std::min (A.y + A.height, B.y + B.height);
  int w = std::max (0, (x2 - x1 + 1));
  int h = std::max (0, (y2 - y1 + 1));
  float inter = w * h;
  float areaA = A.width * A.height;
  float areaB = B.width * B.height;
  float o = inter / (areaA + areaB - inter);
  return (o >= 0) ? o : 0;
}

/**
 * @brief NMS (non-maximum suppression)
 */
static void
nms (std::vector<ssd_detected_object_s> &detected)
{
  const float threshold_iou = .5f;
  guint boxes_size;
  guint i, j;

  std::sort (detected.begin (), detected.end (), compare_objs);
  boxes_size = detected.size ();

  std::vector<bool> del (boxes_size, false);
  for (i = 0; i < boxes_size; i++) {
    if (!del[i]) {
      for (j = i + 1; j < boxes_size; j++) {
        if (iou (detected.at (i), detected.at (j)) > threshold_iou) {
          del[j] = true;
        }
      }
    }
  }

  /* update result */
  g_mutex_lock (&ssd_mutex);

  ssd_detected_objects.clear ();
  for (i = 0; i < boxes_size; i++) {
    if (!del[i]) {
      ssd_detected_objects.push_back (detected[i]);
    }
  }

  g_mutex_unlock (&ssd_mutex);
}

/**
 * @brief Update detected objects.
 */
extern "C" void
ssd_update_result (gfloat * detections, gfloat * boxes)
{
  const float threshold_score = .5f;
  std::vector<ssd_detected_object_s> detected;

  for (int d = 0; d < DETECTION_MAX; d++) {
    float ycenter = boxes[0] / Y_SCALE * ssd_model_info.box_priors[2][d] +
        ssd_model_info.box_priors[0][d];
    float xcenter = boxes[1] / X_SCALE * ssd_model_info.box_priors[3][d] +
        ssd_model_info.box_priors[1][d];
    float h = (float) expf (boxes[2] / H_SCALE) * ssd_model_info.box_priors[2][d];
    float w = (float) expf (boxes[3] / W_SCALE) * ssd_model_info.box_priors[3][d];

    float ymin = ycenter - h / 2.f;
    float xmin = xcenter - w / 2.f;
    float ymax = ycenter + h / 2.f;
    float xmax = xcenter + w / 2.f;

    int x = xmin * MODEL_WIDTH;
    int y = ymin * MODEL_HEIGHT;
    int width = (xmax - xmin) * MODEL_WIDTH;
    int height = (ymax - ymin) * MODEL_HEIGHT;

    for (int c = 1; c < LABEL_SIZE; c++) {
      gfloat score = 1.f / (1.f + expf (-1.f * detections[c]));
      /**
       * This score cutoff is taken from Tensorflow's demo app.
       * There are quite a lot of nodes to be run to convert it to the useful possibility
       * scores. As a result of that, this cutoff will cause it to lose good detections in
       * some scenarios and generate too much noise in other scenario.
       */
      if (score < threshold_score)
        continue;

      ssd_detected_object_s object;

      object.class_id = c;
      object.x = x;
      object.y = y;
      object.width = width;
      object.height = height;
      object.prob = score;
      detected.push_back (object);
    }

    detections += LABEL_SIZE;
    boxes += BOX_SIZE;
  }

  nms (detected);
}

/**
 * @brief Get detected objects.
 */
extern "C" guint
ssd_get_detected_objects (ssd_detected_object_s * objects, const guint size)
{
  guint index = 0;
  std::vector<ssd_detected_object_s> detected;
  std::vector<ssd_detected_object_s>::iterator iter;

  g_mutex_lock (&ssd_mutex);
  detected = ssd_detected_objects;
  g_mutex_unlock (&ssd_mutex);

  for (iter = detected.begin (); iter != detected.end (); ++iter) {
    objects[index].x = iter->x;
    objects[index].y = iter->y;
    objects[index].width = iter->width;
    objects[index].height = iter->height;
    objects[index].class_id = iter->class_id;
    objects[index].prob = iter->prob;

    if (++index >= size) {
      /* max objects to be drawn */
      break;
    }
  }

  return index;
}

/**
 * @brief Get tf-lite model path.
 */
extern "C" gboolean
ssd_get_model_path (gchar ** path)
{
  *path = ssd_model_info.model_path;
  return (*path != NULL);
}

/**
 * @brief Get label with given class id.
 */
extern "C" gboolean
ssd_get_label (gint class_id, gchar ** label)
{
  *label = (gchar *) g_list_nth_data (ssd_model_info.labels, class_id);
  return (*label != NULL);
}
