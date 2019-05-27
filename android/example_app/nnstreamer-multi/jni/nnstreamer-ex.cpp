/**
 * @file	nnstreamer-ex.cpp
 * @date	20 May 2019
 * @brief	Tensor stream example with TF-Lite model
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @bug		No known bugs
 *
 * Before running this example, model and labels should be in an internal storage of Android device.
 * /sdcard/nnstreamer/tflite_model/
 */

#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <math.h>
#include <gst/gst.h>
#include <cairo/cairo.h>

#include "nnstreamer-jni.h"

#define EX_MODEL_PATH "/sdcard/nnstreamer/tflite_model"

#define EX_BOX_PRIORS EX_MODEL_PATH "/box_priors.txt"

#define EX_OBJ_MODEL EX_MODEL_PATH "/ssd_mobilenet_v2_coco.tflite"
#define EX_OBJ_LABEL EX_MODEL_PATH "/coco_labels_list.txt"

#define EX_FACE_MODEL EX_MODEL_PATH "/detect_face.tflite"
#define EX_FACE_LABEL EX_MODEL_PATH "/labels_face.txt"

#define EX_HAND_MODEL EX_MODEL_PATH "/detect_hand.tflite"
#define EX_HAND_LABEL EX_MODEL_PATH "/labels_hand.txt"

#define EX_POSE_MODEL EX_MODEL_PATH "/detect_pose.tflite"

#define MODEL_FACE (1 << 1)
#define MODEL_HAND (1 << 2)
#define MODEL_OBJ (1 << 3)
#define MODEL_POSE (1 << 4)

#define IS_FACE(m) ((m) & MODEL_FACE)
#define IS_HAND(m) ((m) & MODEL_HAND)
#define IS_OBJ(m) ((m) & MODEL_OBJ)
#define IS_POSE(m) ((m) & MODEL_POSE)
#define USE_FRONT(m) ((m) & (1 << 8))

#define MEDIA_WIDTH     480
#define MEDIA_HEIGHT    480

#define Y_SCALE         10.0f
#define X_SCALE         10.0f
#define H_SCALE         5.0f
#define W_SCALE         5.0f

#define SSD_MODEL_WIDTH     300
#define SSD_MODEL_HEIGHT    300

#define SSD_BOX_SIZE        4
#define SSD_DETECTION_MAX   1917

#define POSE_MODEL_WIDTH    192
#define POSE_MODEL_HEIGHT   192

#define POSE_SIZE           14
#define POSE_OUT_W          96
#define POSE_OUT_H          96

/**
 * @brief Max objects to be displayed in the screen.
 * The application will draw the boxes and labels of detected objects with this limitation.
 */
#define MAX_OBJECT_DETECTION 5

/**
 * @brief Data structure for pose estimation.
 */
typedef struct
{
  gboolean valid;
  guint x;
  guint y;
  gfloat prob;
} pose_s;

/**
 * @brief Data structure for detected object.
 */
typedef struct
{
  guint x;
  guint y;
  guint width;
  guint height;
  guint class_id;
  gfloat prob;
} ssd_object_s;

/**
 * @brief Data structure for model info.
 */
typedef struct
{
  gfloat box_priors[SSD_BOX_SIZE][SSD_DETECTION_MAX]; /**< box prior */
  GSList *labels_obj;           /**< list of loaded labels (object detection) */
  GSList *labels_face;          /**< list of loaded labels (face detection) */
  GSList *labels_hand;          /**< list of loaded labels (hand detection) */
  gboolean is_initialized;
} nns_ex_model_info_s;

static gint launch_option = 0;
static nns_ex_model_info_s nns_ex_model_info;
static GMutex res_mutex;
static gchar *pipeline_description = NULL;
static std::vector<ssd_object_s> detected_face;
static std::vector<ssd_object_s> detected_hand;
static std::vector<ssd_object_s> detected_object;
static std::vector<pose_s> estimated_pose;

/**
 * @brief Read strings from file.
 */
static gboolean
nns_ex_read_lines (const gchar * file_name, GSList ** lines)
{
  std::ifstream file (file_name);
  if (!file) {
    nns_loge ("Failed to open file %s", file_name);
    return FALSE;
  }

  std::string str;
  while (std::getline (file, str)) {
    *lines = g_slist_append (*lines, g_strdup (str.c_str ()));
  }

  return TRUE;
}

/**
 * @brief Load box priors.
 */
static gboolean
nns_ex_load_box_priors (void)
{
  GSList *box_priors = NULL;
  gchar *box_row;

  if (!nns_ex_read_lines (EX_BOX_PRIORS, &box_priors)) {
    nns_loge ("Failed to load box prior");
    return FALSE;
  }

  for (guint row = 0; row < SSD_BOX_SIZE; row++) {
    gint column = 0;
    gint i = 0, j = 0;
    gchar buff[11];

    memset (buff, 0, 11);
    box_row = (gchar *) g_slist_nth_data (box_priors, row);

    while ((box_row[i] != '\n') && (box_row[i] != '\0')) {
      if (box_row[i] != ' ') {
        buff[j] = box_row[i];
        j++;
      } else {
        if (j != 0) {
          nns_ex_model_info.box_priors[row][column++] =
              (gfloat) g_ascii_strtod (buff, NULL);
          memset (buff, 0, 11);
        }
        j = 0;
      }
      i++;
    }

    nns_ex_model_info.box_priors[row][column] =
        (gfloat) g_ascii_strtod (buff, NULL);
  }

  g_slist_free_full (box_priors, g_free);
  return TRUE;
}

/**
 * @brief Load labels.
 */
static gboolean
nns_ex_load_labels (void)
{
  if (!nns_ex_read_lines (EX_OBJ_LABEL, &nns_ex_model_info.labels_obj) ||
      !nns_ex_read_lines (EX_FACE_LABEL, &nns_ex_model_info.labels_face) ||
      !nns_ex_read_lines (EX_HAND_LABEL, &nns_ex_model_info.labels_hand)) {
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Free pipeline info.
 */
static void
nns_ex_free (void)
{
  if (nns_ex_model_info.labels_obj) {
    g_slist_free_full (nns_ex_model_info.labels_obj, g_free);
    nns_ex_model_info.labels_obj = NULL;
  }

  if (nns_ex_model_info.labels_face) {
    g_slist_free_full (nns_ex_model_info.labels_face, g_free);
    nns_ex_model_info.labels_face = NULL;
  }

  if (nns_ex_model_info.labels_hand) {
    g_slist_free_full (nns_ex_model_info.labels_hand, g_free);
    nns_ex_model_info.labels_hand = NULL;
  }

  g_mutex_clear (&res_mutex);
  g_free (pipeline_description);

  detected_face.clear ();
  detected_hand.clear ();
  detected_object.clear ();
  estimated_pose.clear ();

  nns_ex_model_info.is_initialized = FALSE;
}

/**
 * @brief Init pipeline info.
 */
static gboolean
nns_ex_init (void)
{
  memset (&nns_ex_model_info, 0, sizeof (nns_ex_model_info_s));

  g_mutex_init (&res_mutex);

  detected_face.clear ();
  detected_hand.clear ();
  detected_object.clear ();
  estimated_pose.clear ();
  return TRUE;
}

/**
 * @brief Get total label size.
 */
static guint
nns_ex_get_label_size (const int model)
{
  GSList *list = NULL;

  if (IS_FACE (model))
    list = nns_ex_model_info.labels_face;
  else if (IS_HAND (model))
    list = nns_ex_model_info.labels_hand;
  else if (IS_OBJ (model))
    list = nns_ex_model_info.labels_obj;

  return g_slist_length (list);
}

/**
 * @brief Get label with given class id.
 */
static gboolean
nns_ex_get_label (const gint model, const guint class_id, gchar ** label)
{
  GSList *list = NULL;

  *label = NULL;

  if (IS_FACE (model))
    list = nns_ex_model_info.labels_face;
  else if (IS_HAND (model))
    list = nns_ex_model_info.labels_hand;
  else if (IS_OBJ (model))
    list = nns_ex_model_info.labels_obj;

  *label = (gchar *) g_slist_nth_data (list, class_id);
  return (*label != NULL);
}

/**
 * @brief Compare score of detected objects.
 */
static bool
ssd_compare_objs (ssd_object_s &a, ssd_object_s &b)
{
  return a.prob > b.prob;
}

/**
 * @brief Intersection of union
 */
static gfloat
ssd_iou (ssd_object_s &A, ssd_object_s &B)
{
  gint x1 = std::max (A.x, B.x);
  gint y1 = std::max (A.y, B.y);
  gint x2 = std::min (A.x + A.width, B.x + B.width);
  gint y2 = std::min (A.y + A.height, B.y + B.height);
  gint w = std::max (0, (x2 - x1 + 1));
  gint h = std::max (0, (y2 - y1 + 1));
  gfloat inter = w * h;
  gfloat areaA = A.width * A.height;
  gfloat areaB = B.width * B.height;
  gfloat o = inter / (areaA + areaB - inter);
  return (o >= 0) ? o : 0;
}

/**
 * @brief NMS (non-maximum suppression)
 */
static void
ssd_nms (std::vector<ssd_object_s> &detected, const gint model)
{
  const gfloat threshold_iou = .5f;
  gsize boxes_size;
  guint i, j;

  std::sort (detected.begin (), detected.end (), ssd_compare_objs);
  boxes_size = detected.size ();

  std::vector<bool> del (boxes_size, false);
  for (i = 0; i < boxes_size; i++) {
    if (!del[i]) {
      for (j = i + 1; j < boxes_size; j++) {
        if (ssd_iou (detected.at (i), detected.at (j)) > threshold_iou) {
          del[j] = true;
        }
      }
    }
  }

  /* update result */
  g_mutex_lock (&res_mutex);

  if (IS_FACE (model)) {
    detected_face.clear ();
    for (i = 0; i < boxes_size; i++) {
      if (!del[i])
        detected_face.push_back (detected[i]);
    }
  } else if (IS_HAND (model)) {
    detected_hand.clear ();
    for (i = 0; i < boxes_size; i++) {
      if (!del[i])
        detected_hand.push_back (detected[i]);
    }
  } else if (IS_OBJ (model)) {
    detected_object.clear ();
    for (i = 0; i < boxes_size; i++) {
      if (!del[i])
        detected_object.push_back (detected[i]);
    }
  }

  g_mutex_unlock (&res_mutex);
}

/**
 * @brief Update detected objects.
 */
static void
ssd_update_detection (gfloat * detections, gfloat * boxes, const gint model)
{
  const gfloat threshold_score = .5f;
  gfloat xcenter, ycenter, x, y, width, height;
  gfloat xmin, ymin, xmax, ymax, score;
  gfloat *detect, *box;
  guint label_size;
  std::vector<ssd_object_s> detected;

  label_size = nns_ex_get_label_size (model);

  for (guint d = 0; d < SSD_DETECTION_MAX; d++) {
    box = boxes + (SSD_BOX_SIZE * d);

    ycenter = box[0] / Y_SCALE * nns_ex_model_info.box_priors[2][d] +
        nns_ex_model_info.box_priors[0][d];
    xcenter = box[1] / X_SCALE * nns_ex_model_info.box_priors[3][d] +
        nns_ex_model_info.box_priors[1][d];
    height = (gfloat) expf (box[2] / H_SCALE) * nns_ex_model_info.box_priors[2][d];
    width = (gfloat) expf (box[3] / W_SCALE) * nns_ex_model_info.box_priors[3][d];

    ymin = ycenter - height / 2.f;
    xmin = xcenter - width / 2.f;
    ymax = ycenter + height / 2.f;
    xmax = xcenter + width / 2.f;

    x = xmin * SSD_MODEL_WIDTH;
    y = ymin * SSD_MODEL_HEIGHT;
    width = (xmax - xmin) * SSD_MODEL_WIDTH;
    height = (ymax - ymin) * SSD_MODEL_HEIGHT;

    detect = detections + (label_size * d);
    for (guint c = 1; c < label_size; c++) {
      score = 1.f / (1.f + expf (-1.f * detect[c]));

      /**
       * This score cutoff is taken from Tensorflow's demo app.
       * There are quite a lot of nodes to be run to convert it to the useful possibility
       * scores. As a result of that, this cutoff will cause it to lose good detections in
       * some scenarios and generate too much noise in other scenario.
       */
      if (score < threshold_score)
        continue;

      ssd_object_s object;

      object.class_id = c;
      object.x = (guint) x;
      object.y = (guint) y;
      object.width = (guint) width;
      object.height = (guint) height;
      object.prob = score;

      detected.push_back (object);
    }
  }

  ssd_nms (detected, model);
}

/**
 * @brief Get detected objects.
 */
static guint
ssd_get_detected_objects (ssd_object_s * objects, const gint model)
{
  guint index = 0;
  std::vector<ssd_object_s> detected;
  std::vector<ssd_object_s>::iterator iter;

  g_mutex_lock (&res_mutex);

  if (IS_FACE (model))
    detected = detected_face;
  else if (IS_HAND (model))
    detected = detected_hand;
  else if (IS_OBJ (model))
    detected = detected_object;

  g_mutex_unlock (&res_mutex);

  for (iter = detected.begin (); iter != detected.end (); ++iter) {
    objects[index].x = iter->x;
    objects[index].y = iter->y;
    objects[index].width = iter->width;
    objects[index].height = iter->height;
    objects[index].class_id = iter->class_id;
    objects[index].prob = iter->prob;

    if (++index >= MAX_OBJECT_DETECTION) {
      /* max objects to be drawn */
      break;
    }
  }

  return index;
}

/**
 * @brief Draw detected object.
 */
static void
ssd_draw_object (cairo_t * cr, ssd_object_s * objects,
    const gint size, const gint model)
{
  guint i;
  gdouble x, y, width, height;
  gdouble red, green, blue;
  gchar *label;

  /* Set clolr */
  if (IS_FACE (model)) {
    blue = 1.0;
    red = green = 0.0;
  } else if (IS_HAND (model)) {
    red = blue = 0.0;
    green = 0.5;
  } else {
    red = 1.0;
    green = blue = 0.0;
  }

  for (i = 0; i < size; ++i) {
    x = (gdouble) objects[i].x * MEDIA_WIDTH / SSD_MODEL_WIDTH;
    y = (gdouble) objects[i].y * MEDIA_HEIGHT / SSD_MODEL_HEIGHT;
    width = (gdouble) objects[i].width * MEDIA_WIDTH / SSD_MODEL_WIDTH;
    height = (gdouble) objects[i].height * MEDIA_HEIGHT / SSD_MODEL_HEIGHT;

    /* draw rectangle */
    cairo_rectangle (cr, x, y, width, height);
    cairo_set_source_rgb (cr, red, green, blue);
    cairo_set_line_width (cr, 1.5);
    cairo_stroke (cr);

    /* draw title */
    if (IS_OBJ (model)) {
      if (nns_ex_get_label (model, objects[i].class_id, &label)) {
        cairo_move_to (cr, x + 5, y + 15);
        cairo_text_path (cr, label);
        cairo_set_source_rgb (cr, red, green, blue);
        cairo_fill_preserve (cr);
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_set_line_width (cr, .3);
        cairo_stroke (cr);
        cairo_fill_preserve (cr);
      } else {
        nns_logd ("Failed to get label (class id %d)", objects[i].class_id);
      }
    }
  }
}

/**
 * @brief Update pose data.
 */
static void
pose_update_result (std::vector<pose_s> &detected)
{
  const gfloat threshold_score = .5f;
  gsize len = detected.size ();

  g_mutex_lock (&res_mutex);

  estimated_pose.clear ();
  for (guint i = 0; i < len; ++i) {
    if (detected[i].prob > threshold_score)
      detected[i].valid = TRUE;

    estimated_pose.push_back (detected[i]);
  }

  g_mutex_unlock (&res_mutex);
}

/**
 * @brief Draw line of pose estimation.
 */
static void
pose_draw_line (cairo_t * cr, std::vector<pose_s> &detected,
    guint start, guint end)
{
  gdouble xs, ys, xe, ye;

  if (detected[start].valid && detected[end].valid) {
    xs = detected[start].x * MEDIA_WIDTH / POSE_OUT_W;
    ys = detected[start].y * MEDIA_HEIGHT / POSE_OUT_H;

    xe = detected[end].x * MEDIA_WIDTH / POSE_OUT_W;
    ye = detected[end].y * MEDIA_HEIGHT / POSE_OUT_H;

    cairo_move_to (cr, xs, ys);
    cairo_line_to (cr, xe, ye);
    cairo_stroke (cr);
  }
}

/**
 * @brief Draw pose estimation.
 */
static void
pose_draw (cairo_t * cr)
{
  std::vector<pose_s> detected;
  gdouble x, y;

  g_mutex_lock (&res_mutex);
  detected = estimated_pose;
  g_mutex_unlock (&res_mutex);

  if (detected.size () != POSE_SIZE) {
    nns_logd ("Current vector size is %zd", detected.size ());
    return;
  }

  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
  cairo_set_line_width (cr, 3.0);

  /* line */
  pose_draw_line (cr, detected, 0, 1);
  pose_draw_line (cr, detected, 1, 2);
  pose_draw_line (cr, detected, 2, 3);
  pose_draw_line (cr, detected, 3, 4);
  pose_draw_line (cr, detected, 1, 5);
  pose_draw_line (cr, detected, 5, 6);
  pose_draw_line (cr, detected, 6, 7);
  pose_draw_line (cr, detected, 1, 8);
  pose_draw_line (cr, detected, 8, 9);
  pose_draw_line (cr, detected, 9, 10);
  pose_draw_line (cr, detected, 1, 11);
  pose_draw_line (cr, detected, 11, 12);
  pose_draw_line (cr, detected, 12, 13);

  /* dot */
  for (guint i = 0; i < POSE_SIZE; ++i) {
    if (detected[i].valid) {
      x = detected[i].x * MEDIA_WIDTH / POSE_OUT_W;
      y = detected[i].y * MEDIA_HEIGHT / POSE_OUT_H;

      cairo_arc (cr, x, y, 5, 0, 2 * M_PI);
      cairo_fill (cr);
    }
  }
}

/**
 * @brief Parse pose result.
 */
static void
nns_ex_parse_pose (GstBuffer * buffer)
{
  GstMemory *mem_pose;
  GstMapInfo info_pose;
  gfloat *pose_data;
  guint index, i, j;
  guint maxX, maxY;
  gfloat max, cen;
  std::vector<pose_s> detected;

  if (gst_buffer_n_memory (buffer) != 1) {
    nns_loge ("Invalid result, the number of memory blocks is different.");
    return;
  }

  mem_pose = gst_buffer_get_memory (buffer, 0);
  gst_memory_map (mem_pose, &info_pose, GST_MAP_READ);

  pose_data = (gfloat *) info_pose.data;

  for (index = 0; index < POSE_SIZE; ++index) {
    maxX = maxY = 0;
    max = .0f;

    for (j = 0; j < POSE_OUT_H; ++j) {
      for (i = 0; i < POSE_OUT_W; ++i) {
        cen = pose_data[i * POSE_SIZE + j * POSE_OUT_W * POSE_SIZE + index];

        if (cen > max) {
          max = cen;
          maxX = i;
          maxY = j;
        }
      }
    }

    pose_s p;

    p.valid = FALSE;
    p.x = maxX;
    p.y = maxY;
    p.prob = max;

    detected.push_back (p);
  }

  gst_memory_unmap (mem_pose, &info_pose);
  gst_memory_unref (mem_pose);

  pose_update_result (detected);
}

/**
 * @brief Parse detection result.
 */
static void
nns_ex_parse_ssd (GstBuffer * buffer, const gint model)
{
  GstMemory *mem_boxes, *mem_detections;
  GstMapInfo info_boxes, info_detections;
  gfloat *boxes, *detections;

  if (gst_buffer_n_memory (buffer) != 2) {
    nns_loge ("Invalid result, the number of memory blocks is different.");
    return;
  }

  /* boxes */
  mem_boxes = gst_buffer_get_memory (buffer, 0);
  gst_memory_map (mem_boxes, &info_boxes, GST_MAP_READ);
  boxes = (gfloat *) info_boxes.data;

  /* detections */
  mem_detections = gst_buffer_get_memory (buffer, 1);
  gst_memory_map (mem_detections, &info_detections, GST_MAP_READ);
  detections = (gfloat *) info_detections.data;

  ssd_update_detection (detections, boxes, model);

  gst_memory_unmap (mem_boxes, &info_boxes);
  gst_memory_unmap (mem_detections, &info_detections);

  gst_memory_unref (mem_boxes);
  gst_memory_unref (mem_detections);
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
nns_ex_new_data_face_cb (GstElement * element, GstBuffer * buffer,
    gpointer user_data)
{
  nns_ex_parse_ssd (buffer, MODEL_FACE);
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
nns_ex_new_data_hand_cb (GstElement * element, GstBuffer * buffer,
    gpointer user_data)
{
  nns_ex_parse_ssd (buffer, MODEL_HAND);
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
nns_ex_new_data_obj_cb (GstElement * element, GstBuffer * buffer,
    gpointer user_data)
{
  nns_ex_parse_ssd (buffer, MODEL_OBJ);
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
nns_ex_new_data_pose_cb (GstElement * element, GstBuffer * buffer,
    gpointer user_data)
{
  nns_ex_parse_pose (buffer);
}

/**
 * @brief Callback to draw the overlay.
 */
static void
nns_ex_draw_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp,
    guint64 duration, gpointer user_data)
{
  guint max_objects;
  ssd_object_s objects[MAX_OBJECT_DETECTION];

  /* set font props */
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, 18.0);

  if (IS_FACE (launch_option)) {
    max_objects = ssd_get_detected_objects (objects, MODEL_FACE);
    ssd_draw_object (cr, objects, max_objects, MODEL_FACE);
  }

  if (IS_HAND (launch_option)) {
    max_objects = ssd_get_detected_objects (objects, MODEL_HAND);
    ssd_draw_object (cr, objects, max_objects, MODEL_HAND);
  }

  if (IS_OBJ (launch_option)) {
    max_objects = ssd_get_detected_objects (objects, MODEL_OBJ);
    ssd_draw_object (cr, objects, max_objects, MODEL_OBJ);
  }

  if (IS_POSE (launch_option)) {
    pose_draw (cr);
  }
}

/**
 * @brief Get model name.
 */
static gboolean
nns_ex_get_name (gchar ** name, const gint option)
{
  static gchar model_name_face[] = "Face Detection";
  static gchar model_name_hand[] = "Hand Detection";
  static gchar model_name_obj[] = "Object Detection";
  static gchar model_name_pose[] = "Pose Estimation";

  if (IS_FACE (option)) {
    *name = model_name_face;
  } else if (IS_HAND (option)) {
    *name = model_name_hand;
  } else if (IS_OBJ (option)) {
    *name = model_name_obj;
  } else if (IS_POSE (option)) {
    *name = model_name_pose;
  } else {
    nns_loge ("Cannot get the model name with option %d", option);
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Get current pipeline description.
 */
static gboolean
nns_ex_get_description (gchar ** desc, const gint option)
{
  *desc = pipeline_description;
  return (*desc != NULL);
}

/**
 * @brief Append new text.
 */
static gchar *
nns_ex_append_text (gchar * old, gchar * str)
{
  gchar *new_pipeline;

  new_pipeline = g_strconcat (old, str, NULL);

  g_free (str);
  g_free (old);

  return new_pipeline;
}

/**
 * @brief Append pipeline description.
 */
static gchar *
nns_ex_append_description (gchar * old, gchar * str, const gchar * color)
{
  gchar *wrap, *new_desc;

  wrap = g_strdup_printf ("<p style=\"color:%s;\">%s</p>", color, str);
  new_desc = g_strconcat (old, wrap, NULL);

  g_free (wrap);
  g_free (str);
  g_free (old);

  return new_desc;
}

/**
 * @brief Update pipeline description.
 */
static void
nns_ex_set_description (const gint option)
{
  gchar *str_desc, *extra;

  str_desc = g_strdup
      ("<html><meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"><body>");

  /* camera-source and video-sink*/
  extra = g_strdup
      ("ahc2src ! videoflip ! videocrop ! tee name=traw "
      "traw. ! queue ! cairooverlay ! videosink ");
  str_desc = nns_ex_append_description (str_desc, extra, "#000080");

  if (IS_POSE (option)) {
    extra = g_strdup
        ("traw. ! queue ! videoscale ! tensor_converter ! tensor_transform ! "
        "tensor_filter pose ! tensor_sink ");
    str_desc = nns_ex_append_description (str_desc, extra, "#D2691E");
  }

  if (IS_FACE (option) || IS_HAND (option) || IS_OBJ (option)) {
    extra = g_strdup
        ("traw. ! queue ! videoscale ! tensor_converter ! tensor_transform ! tee name=tssd ");
    str_desc = nns_ex_append_description (str_desc, extra, "#000080");

    if (IS_FACE (option)) {
      extra = g_strdup ("tssd. ! queue ! tensor_filter model=face ! tensor_sink ");
      str_desc = nns_ex_append_description (str_desc, extra, "#0000FF");
    }

    if (IS_HAND (option)) {
      extra = g_strdup ("tssd. ! queue ! tensor_filter model=hand ! tensor_sink ");
      str_desc = nns_ex_append_description (str_desc, extra, "#008000");
    }

    if (IS_OBJ (option)) {
      extra = g_strdup ("tssd. ! queue ! tensor_filter model=object ! tensor_sink ");
      str_desc = nns_ex_append_description (str_desc, extra, "#FF0000");
    }
  }

  extra = g_strdup ("</body></html>");
  str_desc = nns_ex_append_text (str_desc, extra);

  g_free (pipeline_description);
  pipeline_description = str_desc;
}

/**
 * @brief Start pipeline.
 */
static gboolean
nns_ex_launch_pipeline (GstElement ** pipeline, const gint option)
{
  GstElement *element;
  GError *error = NULL;
  gchar *str_pipeline, *extra;
  gboolean front_cam;

  launch_option = option;
  front_cam = USE_FRONT (option) ? TRUE : FALSE;

  /* Update pipeline description */
  nns_ex_set_description (option);

  /* videocrop 640x480 > 480x480, front camera */
  str_pipeline = g_strdup_printf
      ("ahc2src camera-index=%d ! videoconvert ! video/x-raw,format=RGB,width=640,height=480,framerate=30/1 ! "
      "videoflip method=%s ! videocrop left=0 right=0 top=80 bottom=80 ! tee name=traw "
      "traw. ! queue min-threshold-buffers=8 ! videoconvert ! "
      "cairooverlay name=res_cairooverlay ! glimagesink sync=false ",
      (front_cam) ? 1 : 0, (front_cam) ? "upper-right-diagonal" : "clockwise");

  if (option) {
    if (IS_POSE (option)) {
      /**
       * pose estimation
       * input[0] float32 [3:192:192:1] (RGB:POSE_MODEL_WIDTH:POSE_MODEL_HEIGHT:1)
       * output[0] float32 [14:96:96:1] (POSE_SIZE:POSE_OUT_W:POSE_OUT_H:1)
       */
      extra = g_strdup_printf
          ("traw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw,format=RGB,width=192,height=192 ! "
          "tensor_converter ! tensor_transform mode=typecast option=float32 ! "
          "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=res_pose ",
          EX_POSE_MODEL);
      str_pipeline = nns_ex_append_text (str_pipeline, extra);
    }

    if (IS_FACE (option) || IS_HAND (option) || IS_OBJ (option)) {
      /**
       * object detection base (videoscale 480x480 > 300x300)
       */
      extra = g_strdup_printf
          ("traw. ! queue ! videoscale ! video/x-raw,format=RGB,width=300,height=300 ! "
          "tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
          "tee name=tssd ", SSD_MODEL_WIDTH, SSD_MODEL_HEIGHT);
      str_pipeline = nns_ex_append_text (str_pipeline, extra);

      if (IS_FACE (option)) {
        /**
         * face detection
         * input[0] float32 [3:300:300:1] (RGB:SSD_MODEL_WIDTH:SSD_MODEL_HEIGHT:1)
         * output[0] float32 [4:1:1917:1] (SSD_BOX_SIZE:1:SSD_DETECTION_MAX:1)
         * output[1] float32 [2:1917:1:1] (LABEL_SIZE:SSD_DETECTION_MAX:1:1)
         */
        extra = g_strdup_printf
            ("tssd. ! queue leaky=2 max-size-buffers=2 ! "
            "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=res_face ",
            EX_FACE_MODEL);
        str_pipeline = nns_ex_append_text (str_pipeline, extra);
      }

      if (IS_HAND (option)) {
        /**
         * hand detection
         * input[0] float32 [3:300:300:1] (RGB:SSD_MODEL_WIDTH:SSD_MODEL_HEIGHT:1)
         * output[0] float32 [4:1:1917:1] (SSD_BOX_SIZE:1:SSD_DETECTION_MAX:1)
         * output[1] float32 [2:1917:1:1] (LABEL_SIZE:SSD_DETECTION_MAX:1:1)
         */
        extra = g_strdup_printf
            ("tssd. ! queue leaky=2 max-size-buffers=2 ! "
            "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=res_hand ",
            EX_HAND_MODEL);
        str_pipeline = nns_ex_append_text (str_pipeline, extra);
      }

      if (IS_OBJ (option)) {
        /**
         * object detection
         * input[0] float32 [3:300:300:1] (RGB:SSD_MODEL_WIDTH:SSD_MODEL_HEIGHT:1)
         * output[0] float32 [4:1:1917:1] (SSD_BOX_SIZE:1:SSD_DETECTION_MAX:1)
         * output[1] float32 [91:1917:1:1] (LABEL_SIZE:SSD_DETECTION_MAX:1:1)
         */
        extra = g_strdup_printf
            ("tssd. ! queue leaky=2 max-size-buffers=2 ! "
            "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=res_obj ",
            EX_OBJ_MODEL);
        str_pipeline = nns_ex_append_text (str_pipeline, extra);
      }
    }
  }

  nns_logd ("Pipeline: %s", str_pipeline);
  *pipeline = gst_parse_launch (str_pipeline, &error);
  g_free (str_pipeline);

  if (error) {
    nns_logd ("Unable to build pipeline: %s", error->message);
    g_clear_error (&error);
    *pipeline = NULL;
    return FALSE;
  }

  /**
   * tensor_sink new-data signal
   * tensor_sink emits the signal when new buffer incomes to the sink pad.
   * The application can connect this signal with the callback.
   * Please be informed that, the memory blocks in the buffer object passed from tensor_sink is available only in this callback function.
   */
  if (IS_FACE (option)) {
    element = gst_bin_get_by_name (GST_BIN (*pipeline), "res_face");
    g_signal_connect (element, "new-data", G_CALLBACK (nns_ex_new_data_face_cb), NULL);
    gst_object_unref (element);
  }

  if (IS_HAND (option)) {
    element = gst_bin_get_by_name (GST_BIN (*pipeline), "res_hand");
    g_signal_connect (element, "new-data", G_CALLBACK (nns_ex_new_data_hand_cb), NULL);
    gst_object_unref (element);
  }

  if (IS_OBJ (option)) {
    element = gst_bin_get_by_name (GST_BIN (*pipeline), "res_obj");
    g_signal_connect (element, "new-data", G_CALLBACK (nns_ex_new_data_obj_cb), NULL);
    gst_object_unref (element);
  }

  if (IS_POSE (option)) {
    element = gst_bin_get_by_name (GST_BIN (*pipeline), "res_pose");
    g_signal_connect (element, "new-data", G_CALLBACK (nns_ex_new_data_pose_cb), NULL);
    gst_object_unref (element);
  }

  /* cairooverlay draw */
  element = gst_bin_get_by_name (GST_BIN (*pipeline), "res_cairooverlay");
  g_signal_connect (element, "draw", G_CALLBACK (nns_ex_draw_overlay_cb), NULL);
  gst_object_unref (element);

  return TRUE;
}

/**
 * @brief Check condition to start the pipeline.
 */
static gboolean
nns_ex_prepare_pipeline (const gint option)
{
  /* Check model and label files */
  if (!g_file_test (EX_BOX_PRIORS, G_FILE_TEST_EXISTS) ||
      !g_file_test (EX_OBJ_MODEL, G_FILE_TEST_EXISTS) ||
      !g_file_test (EX_OBJ_LABEL, G_FILE_TEST_EXISTS) ||
      !g_file_test (EX_FACE_MODEL, G_FILE_TEST_EXISTS) ||
      !g_file_test (EX_FACE_LABEL, G_FILE_TEST_EXISTS) ||
      !g_file_test (EX_HAND_MODEL, G_FILE_TEST_EXISTS) ||
      !g_file_test (EX_HAND_LABEL, G_FILE_TEST_EXISTS) ||
      !g_file_test (EX_POSE_MODEL, G_FILE_TEST_EXISTS)) {
    nns_loge ("Cannot find model files");
    return FALSE;
  }

  /* Load labels */
  if (!nns_ex_model_info.is_initialized) {
    if (!nns_ex_load_box_priors () || !nns_ex_load_labels ()) {
      nns_loge ("Failed to load labels");
      nns_ex_free ();
      return FALSE;
    }

    nns_ex_model_info.is_initialized = TRUE;
  }

  return TRUE;
}

static NNSPipelineInfo nns_ex_pipeline = {
  .id = 1,
  .name = "NNStreamer with multi models",
  .description = "Example with Tensorflow-lite models.",
  .init = nns_ex_init,
  .free = nns_ex_free,
  .get_name = nns_ex_get_name,
  .get_description = nns_ex_get_description,
  .prepare_pipeline = nns_ex_prepare_pipeline,
  .launch_pipeline = nns_ex_launch_pipeline
};

/**
 * @brief Register nnstreamer example.
 */
extern "C" void
nns_ex_register_pipeline (void)
{
  if (!nns_register_pipeline (&nns_ex_pipeline)) {
    nns_loge ("Failed to register pipeline.");
  }
}
