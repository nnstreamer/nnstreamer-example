/**
 * @file	nnstreamer_example_video_crop_tflite.c
 * @date	3 JULY 2023
 * @brief	Tensor Crop example with TF2-Lite model for cropping video around detected objects
 * @author	Harsh Jain <hjain24in@gmail.com>
 * @bug		No known bugs.
 *
 * Get model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model.sh object-detection-tf
 *
 * Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./nnstreamer_example_video_crop_tflite
 *
 * Required model and resources are stored at below link
 * https://github.com/nnsuite/testcases/tree/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco
 */

#include <glib.h>
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * @brief Macro for debug mode.
 */
#ifndef DBG
#define DBG FALSE
#endif

/**
 * @brief Macro for debug message.
 */
#define _print_log(...) \
  if (DBG)              \
  g_message (__VA_ARGS__)

/**
 * @brief Macro to check error case.
 */
#define _check_cond_err(cond)                           \
  do {                                                  \
    if (!(cond)) {                                      \
      _print_log ("app failed! [line : %d]", __LINE__); \
      goto error;                                       \
    }                                                   \
  } while (0)

/**
 * @brief Data structure for tflite model info.
 */
typedef struct {
  gchar *model_path; /**< tflite model file path */
  gchar *label_path; /**< label file path */
  gchar *box_prior_path; /**< box prior file path */
} TFLiteModelInfo;

/**
 * @brief Data structure for app.
 */
typedef struct {
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
  gboolean running; /**< true when app is running */
  GMutex mutex; /**< mutex for processing */
  TFLiteModelInfo tflite_info; /**< tflite model info */
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Check tflite model and load labels.
 */
static gboolean
tflite_init_info (TFLiteModelInfo *tflite_info, const gchar *path)
{
  const gchar tflite_model[] = "ssd_mobilenet_v2_coco.tflite";
  const gchar tflite_label[] = "coco_labels_list.txt";
  const gchar tflite_box_priors[] = "box_priors.txt";

  g_return_val_if_fail (tflite_info != NULL, FALSE);

  tflite_info->model_path = g_strdup_printf ("%s/%s", path, tflite_model);
  tflite_info->label_path
      = g_strdup_printf ("%s/%s", path, tflite_label);
  tflite_info->box_prior_path
      = g_strdup_printf ("%s/%s", path, tflite_box_priors);

  if (!g_file_test (tflite_info->model_path, G_FILE_TEST_IS_REGULAR)) {
    g_critical ("cannot find tflite model [%s]", tflite_info->model_path);
    return FALSE;
  }
  if (!g_file_test (tflite_info->label_path, G_FILE_TEST_IS_REGULAR)) {
    g_critical ("cannot find tflite label [%s]", tflite_info->label_path);
    return FALSE;
  }
  if (!g_file_test (tflite_info->box_prior_path, G_FILE_TEST_IS_REGULAR)) {
    g_critical ("cannot find tflite box_prior [%s]", tflite_info->box_prior_path);
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Free data in tflite info structure.
 */
static void
tflite_free_info (TFLiteModelInfo *tflite_info)
{
  g_return_if_fail (tflite_info != NULL);

  if (tflite_info->model_path) {
    g_free (tflite_info->model_path);
    tflite_info->model_path = NULL;
  }

  if (tflite_info->label_path) {
    g_free (tflite_info->label_path);
    tflite_info->label_path = NULL;
  }

  if (tflite_info->box_prior_path) {
    g_free (tflite_info->box_prior_path);
    tflite_info->box_prior_path = NULL;
  }
}

/**
 * @brief Free resources in app data.
 */
static void
free_app_data (void)
{
  if (g_app.loop) {
    g_main_loop_unref (g_app.loop);
    g_app.loop = NULL;
  }

  if (g_app.bus) {
    gst_bus_remove_signal_watch (g_app.bus);
    gst_object_unref (g_app.bus);
    g_app.bus = NULL;
  }

  if (g_app.pipeline) {
    gst_object_unref (g_app.pipeline);
    g_app.pipeline = NULL;
  }

  tflite_free_info (&g_app.tflite_info);
  g_mutex_clear (&g_app.mutex);
}

/**
 * @brief Function to print error message.
 */
static void
_parse_err_message (GstMessage *message)
{
  gchar *debug;
  GError *error;

  g_return_if_fail (message != NULL);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &error, &debug);
      break;

    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (message, &error, &debug);
      break;

    default:
      return;
  }

  gst_object_default_error (GST_MESSAGE_SRC (message), error, debug);
  g_error_free (error);
  g_free (debug);
}

/**
 * @brief Function to print qos message.
 */
static void
_parse_qos_message (GstMessage *message)
{
  GstFormat format;
  guint64 processed;
  guint64 dropped;

  gst_message_parse_qos_stats (message, &format, &processed, &dropped);
  _print_log ("format[%d] processed[%" G_GUINT64_FORMAT "] dropped[%" G_GUINT64_FORMAT "]",
      format, processed, dropped);
}

/**
 * @brief Callback for message.
 */
static void
bus_message_cb (GstBus *bus, GstMessage *message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      _print_log ("Received EOS message");
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_ERROR:
      _print_log ("Received error message");
      _parse_err_message (message);
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_WARNING:
      _print_log ("Received warning message");
      _parse_err_message (message);
      break;

    case GST_MESSAGE_STREAM_START:
      _print_log ("Received stream start message");
      break;

    case GST_MESSAGE_QOS:
      _parse_qos_message (message);
      break;

    default:
      break;
  }
}


/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  const gchar tflite_model_path[] = "./tflite_model";

  gchar *str_pipeline;

  _print_log ("start app..");

  /** init app variable */
  g_app.running = FALSE;
  g_app.loop = NULL;
  g_app.bus = NULL;
  g_app.pipeline = NULL;
  g_mutex_init (&g_app.mutex);

  _check_cond_err (tflite_init_info (&g_app.tflite_info, tflite_model_path));

  /** init gstreamer */
  gst_init (&argc, &argv);

  /** main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /** init pipeline */
  str_pipeline = g_strdup_printf (
      "v4l2src name=src ! decodebin ! videoconvert ! videoscale ! video/x-raw,format=RGB,width=640,height=480 ! videocrop left=170 right=170 top=90 bottom=90 ! tee name=t "
      "t. ! queue leaky=2 max-size-buffers=2 ! tensor_converter ! crop.raw "
      "t. ! queue leaky=2 max-size-buffers=10 ! tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
      "tensor_filter framework=tensorflow2-lite model=%s ! "
      "tensor_decoder mode=tensor_region option1=1 option2=%s option3=%s option4=300:300 ! crop.info "
      "tensor_crop name=crop ! other/tensors,format=flexible ! tensor_converter ! tensor_decoder mode=direct_video ! "
      "videoconvert ! videoscale !     video/x-raw,format=RGB ! videoconvert ! autovideosink name=cropped ",
      g_app.tflite_info.model_path, g_app.tflite_info.label_path,
      g_app.tflite_info.box_prior_path);

  _print_log ("%s\n", str_pipeline);

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /** bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  g_signal_connect (g_app.bus, "message", G_CALLBACK (bus_message_cb), NULL);

  /** start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  g_app.running = TRUE;


  /** run main loop */
  g_main_loop_run (g_app.loop);

  /** quit when received eos or error message */
  g_app.running = FALSE;

  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

error:
  _print_log ("close app..");

  free_app_data ();
  return 0;
}
