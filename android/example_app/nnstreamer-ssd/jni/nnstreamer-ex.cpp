/**
 * @file	nnstreamer-ex.cpp
 * @date	1 April 2019
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

#define MEDIA_WIDTH     640
#define MEDIA_HEIGHT    480

#define Y_SCALE         10.0f
#define X_SCALE         10.0f
#define H_SCALE         5.0f
#define W_SCALE         5.0f

#define SSD_MODEL_WIDTH     300
#define SSD_MODEL_HEIGHT    300

#define SSD_BOX_SIZE        4
#define SSD_LABEL_SIZE      91
#define SSD_DETECTION_MAX   1917

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
  gboolean is_initialized;
} nns_ex_model_info_s;

static gint launch_option = 0;
static nns_ex_model_info_s nns_ex_model_info;
static GMutex res_mutex;
static std::vector<ssd_object_s> detected_object;

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
  return nns_ex_read_lines (EX_OBJ_LABEL, &nns_ex_model_info.labels_obj);
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

  g_mutex_clear (&res_mutex);
  detected_object.clear ();

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

  detected_object.clear ();
  return TRUE;
}

/**
 * @brief Get total label size.
 */
static guint
nns_ex_get_label_size (void)
{
  GSList *list = NULL;

  list = nns_ex_model_info.labels_obj;

  return g_slist_length (list);
}

/**
 * @brief Get label with given class id.
 */
static gboolean
nns_ex_get_label (const guint class_id, gchar ** label)
{
  GSList *list = NULL;

  *label = NULL;

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
ssd_nms (std::vector<ssd_object_s> &detected)
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

  detected_object.clear ();
  for (i = 0; i < boxes_size; i++) {
    if (!del[i]) {
      detected_object.push_back (detected[i]);
    }
  }

  g_mutex_unlock (&res_mutex);
}

/**
 * @brief Update detected objects.
 */
static void
ssd_update_detection (gfloat * detections, gfloat * boxes)
{
  const gfloat threshold_score = .5f;
  gfloat xcenter, ycenter, x, y, width, height;
  gfloat xmin, ymin, xmax, ymax, score;
  gfloat *detect, *box;
  guint label_size;
  std::vector<ssd_object_s> detected;

  label_size = nns_ex_get_label_size ();

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

  ssd_nms (detected);
}

/**
 * @brief Get detected objects.
 */
static guint
ssd_get_detected_objects (ssd_object_s * objects)
{
  guint index = 0;
  std::vector<ssd_object_s> detected;
  std::vector<ssd_object_s>::iterator iter;

  g_mutex_lock (&res_mutex);
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
ssd_draw_object (cairo_t * cr, ssd_object_s * objects, const gint size)
{
  guint i;
  gdouble x, y, width, height;
  gdouble red, green, blue;
  gchar *label;

  /* Set clolr */
  red = 1.0;
  green = blue = 0.0;

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
    if (nns_ex_get_label (objects[i].class_id, &label)) {
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

/**
 * @brief Callback for tensor sink signal.
 */
static void
nns_ex_new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
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

  ssd_update_detection (detections, boxes);

  gst_memory_unmap (mem_boxes, &info_boxes);
  gst_memory_unmap (mem_detections, &info_detections);

  gst_memory_unref (mem_boxes);
  gst_memory_unref (mem_detections);
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

  max_objects = ssd_get_detected_objects (objects);
  ssd_draw_object (cr, objects, max_objects);
}

/**
 * @brief Get model name.
 */
static gboolean
nns_ex_get_name (gchar ** name, const gint option)
{
  static gchar model_name_obj[] = "Object Detection";

  *name = model_name_obj;

  return TRUE;
}

/**
 * @brief Get current pipeline description.
 */
static gboolean
nns_ex_get_description (gchar ** desc, const gint option)
{
  static gchar pipeline_description[] = "Object Detection Example";

  *desc = pipeline_description;

  return TRUE;
}

/**
 * @brief Start pipeline.
 */
static gboolean
nns_ex_launch_pipeline (GstElement ** pipeline, const gint option)
{
  GstElement *element;
  GError *error = NULL;
  gchar *str_pipeline;

  launch_option = option;

  str_pipeline = g_strdup_printf
      ("ahc2src ! videoconvert ! video/x-raw,format=RGB,width=640,height=480,framerate=30/1 ! tee name=traw "
      "traw. ! queue ! videoconvert ! cairooverlay name=res_cairooverlay ! glimagesink "
      "traw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw,format=RGB,width=%d,height=%d ! tensor_converter ! "
      "tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
      "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=res_obj",
      SSD_MODEL_WIDTH, SSD_MODEL_HEIGHT, EX_OBJ_MODEL);

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
  element = gst_bin_get_by_name (GST_BIN (*pipeline), "res_obj");
  g_signal_connect (element, "new-data", G_CALLBACK (nns_ex_new_data_cb), NULL);
  gst_object_unref (element);

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
      !g_file_test (EX_OBJ_LABEL, G_FILE_TEST_EXISTS)) {
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
  .name = "NNStreamer Object Detection",
  .description = "Example with Tensorflow-lite model.",
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
