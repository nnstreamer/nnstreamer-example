/**
 * @file	nnstreamer_example_object_detection_tf.cc
 * @date	8 Jan 2019
 * @brief	Tensor stream example with Tensorflow model for object detection
 * @author	HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug		No known bugs.
 *
 * Get model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model.sh object-detection-tf
 * 
 * Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./nnstreamer_example_object_detection_tf
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>

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

/**
 * @brief Data structure for tf model info.
 */
typedef struct
{
  gchar *model_path; /**< tf model file path */
  gchar *label_path; /**< label file path */
  GList *labels; /**< list of loaded labels */
} TFModelInfo;

/**
 * @brief Data structure for app.
 */
typedef struct
{
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
  gboolean running; /**< true when app is running */
  GMutex mutex; /**< mutex for processing */
  TFModelInfo tf_info; /**< tf model info */
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Check tf model and load labels.
 */
static gboolean
tf_init_info (TFModelInfo * tf_info, const gchar * path)
{
  const gchar tf_model[] = "ssdlite_mobilenet_v2.pb";
  const gchar tf_label[] = "coco_labels_list.txt";

  g_return_val_if_fail (tf_info != NULL, FALSE);

  tf_info->model_path = g_strdup_printf ("%s/%s", path, tf_model);
  tf_info->label_path = g_strdup_printf ("%s/%s", path, tf_label);

  if (!g_file_test (tf_info->model_path, G_FILE_TEST_IS_REGULAR)) {
    g_critical ("cannot find tf model [%s]", tf_info->model_path);
    return FALSE;
  }
  if (!g_file_test (tf_info->label_path, G_FILE_TEST_IS_REGULAR)) {
    g_critical ("cannot find tf label [%s]", tf_info->label_path);
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Free data in tf info structure.
 */
static void
tf_free_info (TFModelInfo * tf_info)
{
  g_return_if_fail (tf_info != NULL);

  if (tf_info->model_path) {
    g_free (tf_info->model_path);
    tf_info->model_path = NULL;
  }

  if (tf_info->label_path) {
    g_free (tf_info->label_path);
    tf_info->label_path = NULL;
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

  tf_free_info (&g_app.tf_info);
  g_mutex_clear (&g_app.mutex);
}

/**
 * @brief Function to print error message.
 */
static void
parse_err_message (GstMessage * message)
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
parse_qos_message (GstMessage * message)
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
bus_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      _print_log ("received eos message");
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_ERROR:
      _print_log ("received error message");
      parse_err_message (message);
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_WARNING:
      _print_log ("received warning message");
      parse_err_message (message);
      break;

    case GST_MESSAGE_STREAM_START:
      _print_log ("received start message");
      break;

    case GST_MESSAGE_QOS:
      parse_qos_message (message);
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
  const gchar tf_model_path[] = "./tf_model";

  gchar *str_pipeline;
  GstElement *element;

  _print_log ("start app..");

  /* init app variable */
  g_app.running = FALSE;
  g_app.loop = NULL;
  g_app.bus = NULL;
  g_app.pipeline = NULL;
  g_mutex_init (&g_app.mutex);

  _check_cond_err (tf_init_info (&g_app.tf_info, tf_model_path));

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("v4l2src name=src ! videoscale ! videoconvert ! video/x-raw,width=640,height=480,format=RGB,framerate=30/1 ! tee name=t "
          "t. ! queue leaky=2 max-size-buffers=2 ! videoscale ! tensor_converter ! "
              "tensor_filter framework=tensorflow model=%s "
                  "input=3:640:480:1 inputname=image_tensor inputtype=uint8 output=1,100:1,100:1,4:100:1 "
                  "outputname=num_detections,detection_classes,detection_scores,detection_boxes outputtype=float32,float32,float32,float32 ! "
              "tensor_decoder mode=bounding_boxes option1=mobilenet-ssd-postprocess option2=%s option4=640:480 option5=640:480 ! "
              "compositor name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! ximagesink "
          "t. ! queue leaky=2 max-size-buffers=10 ! mix.",
          g_app.tf_info.model_path, g_app.tf_info.label_path);

  _print_log ("%s\n", str_pipeline);

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  g_signal_connect (g_app.bus, "message", G_CALLBACK (bus_message_cb), NULL);

  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  g_app.running = TRUE;

  /* run main loop */
  g_main_loop_run (g_app.loop);

  /* quit when received eos or error message */
  g_app.running = FALSE;

  /* cam source element */
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

  free_app_data ();
  return 0;
}
