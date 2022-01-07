/**
 * @file    nnstreamer_example_pose_estimation_tflite.cc
 * @date    19 February 2020
 * @brief   Tensor Stream example with TensorFlow Lite model for pose estimation
 * @author  Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 * @bug     No known bugs.
 *
 * NNStreamer example for pose estimation on single person using
 * public TensorFlow Lite model in Ubuntu environment.
 *
 * Get the required model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model.sh pose-estimation-tflite
 *
 * It downloads the model from this link:
 * https://storage.googleapis.com/download.tensorflow.org/models/tflite/posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite
 * and make text file of key point labels for this model (total 17 key points
 * include nose, left ear, right ankle, etc.)
 *
 * Run example:
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<path/to/nnstreamer/plugin/path>
 * $ ./nnstreamer_example_pose_estimation_tflite
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/video.h>

/**
 * @brief Macro for debug mode.
 */
#ifndef DBG
#define DBG FALSE
#endif

/**
 * @brief Macro for debug message.
 */
#define _print_log(...) if (DBG) g_message (__VA_ARGS__)

/**
 * @brief Macro to check error case.
 */
#define _check_cond_err(cond) \
  do { \
    if (!(cond)) { \
      _print_log ("app failed! [line : %d]", __LINE__); \
      goto error; \
    } \
  } while (0)


#define VIDEO_HEIGHT        480
#define VIDEO_WIDTH         640

#define MODEL_INPUT_HEIGHT  257
#define MODEL_INPUT_WIDTH   257

/**
 * @brief Data structure for tflite model and label info.
 */
typedef struct
{
  gchar *model_path;
  gchar *label_path;
} tflite_info_s;

/**
 * @brief Data structure for app.
 */
typedef struct
{
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */

  gboolean running; /**< true when app is running */
  guint received; /**< received buffer count */
  GMutex mutex; /**< mutex for processing */
  tflite_info_s tflite_info; /**< tflite models info */
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Check and load tflite models.
 */
static gboolean
_tflite_init_info (tflite_info_s * tflite_info, const gchar * path)
{
  const gchar tflite_model[] =
      "posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite";
  const gchar tflite_label[] = "point_labels.txt";

  g_return_val_if_fail (tflite_info != NULL, FALSE);

  tflite_info->model_path = NULL;
  tflite_info->label_path = NULL;

  /* check model file can be loaded */
  tflite_info->model_path = g_strdup_printf ("%s/%s", path, tflite_model);

  if (access (tflite_info->model_path, F_OK) != 0) {
    g_critical ("cannot find model file [%s]", tflite_info->model_path);
    return FALSE;
  }

  /* check label file can be loaded */
  tflite_info->label_path = g_strdup_printf ("%s/%s", path, tflite_label);

  if (access (tflite_info->label_path, F_OK) != 0) {
    g_critical ("cannot find label file [%s]", tflite_info->label_path);
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Free data in tflite info structure.
 */
static void
_tflite_free_info (tflite_info_s * tflite_info)
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
}

/**
 * @brief Free resource in app data.
 */
static void
_free_app_data (void)
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

  _tflite_free_info (&g_app.tflite_info);
}

/**
 * @brief Function to print error message.
 */
static void
_parse_err_message (GstMessage * message)
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
_parse_qos_message (GstMessage * message)
{
  GstFormat format;
  guint64 processed;
  guint64 dropped;

  gst_message_parse_qos_stats (message, &format, &processed, &dropped);
  _print_log ("format[%d] processed[%" G_GUINT64_FORMAT "] dropped[%"
      G_GUINT64_FORMAT "]", format, processed, dropped);
}

/**
 * @brief Callback for message.
 */
static void
_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      _print_log ("received eos message");
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_ERROR:
      _print_log ("received error message");
      _parse_err_message (message);
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_WARNING:
      _print_log ("received warning message");
      _parse_err_message (message);
      break;

    case GST_MESSAGE_STREAM_START:
      _print_log ("received start message");
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
main (int argc, char ** argv)
{
  const gchar tflite_model_path[] = "./tflite_pose_estimation";

  gchar *str_pipeline;
  gulong handle_id;
  GstElement *element;

  _print_log ("start app..");

  /* init app variable */
  g_app.running = FALSE;
  g_app.received = 0;

  _check_cond_err (_tflite_init_info (&g_app.tflite_info, tflite_model_path));

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("v4l2src name=src ! videoconvert ! videoscale ! "
      "video/x-raw,width=%d,height=%d,format=RGB ! tee name=t_raw "
      "t_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! "
      "video/x-raw,width=%d,height=%d,format=RGB ! tensor_converter ! "
      "tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
      "tensor_filter framework=tensorflow-lite model=%s ! "
      "tensor_decoder mode=pose_estimation option1=640:480 option2=257:257 option3=%s option4=heatmap-offset ! "
      "compositor name=mix sink_0::zorder=1 sink_1::zorder=0 ! videoconvert ! ximagesink "
      "t_raw. ! queue ! mix.",
      VIDEO_WIDTH, VIDEO_HEIGHT, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT,
      g_app.tflite_info.model_path, g_app.tflite_info.label_path);

  _print_log ("%s\n", str_pipeline);

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  handle_id = g_signal_connect (g_app.bus, "message", G_CALLBACK (_message_cb), NULL);
  _check_cond_err (handle_id > 0);

  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  g_app.running = TRUE;

  /* run main loop */
  g_main_loop_run (g_app.loop);

  /* quit when received eos or error message */
  g_app.running = FALSE;

  /* video test source element */
  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "src");

  gst_element_set_state (element, GST_STATE_READY);
  gst_element_set_state (g_app.pipeline, GST_STATE_READY);

  g_usleep (200 * 1000);

  gst_element_set_state (element, GST_STATE_NULL);
  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

  g_usleep (200 * 1000);
  gst_object_unref (element);

error:
  _print_log ("close app..");

  _free_app_data ();
  return 0;
}
