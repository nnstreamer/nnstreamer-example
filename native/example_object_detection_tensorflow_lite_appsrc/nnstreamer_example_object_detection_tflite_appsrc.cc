/**
 * @file	nnstreamer_example_object_detection_tflite_appsrc.c
 * @date	25 May 2020
 * @brief	Example with tensorflow-lite model for experiment.
 * @author	HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug		No known bugs.
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/app.h>

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480
// #define VIDEO_WIDTH 1920
// #define VIDEO_HEIGHT 1080

#define MODEL_WIDTH 320
#define MODEL_HEIGHT 320

#define CH 3

#define LABEL_SIZE 91
#define DETECTION_MAX 2034
#define BOX_SIZE 4

#define TEST_LOOP 10

/**
 * @brief Macro for debug mode.
 */
#ifndef DBG
#define DBG TRUE
#endif

/**
 * @brief Macro for debug message.
 */
#define _print_log(...) if (DBG) g_message (__VA_ARGS__)

gint64 start_time, end_time, avg_time = 0;

/**
 * @brief Data structure for app.
 */
typedef struct
{
  gboolean running;

  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
  GstElement *src; /**< appsrc element in pipeline */

  gchar *model_file; /**< tensorflow-lite model file */
} app_data_s;


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
  g_warning ("format[%d] processed[%" G_GUINT64_FORMAT "] dropped[%"
      G_GUINT64_FORMAT "]", format, processed, dropped);
}

/**
 * @brief Callback for message.
 */
static void
bus_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  app_data_s *app;

  app = (app_data_s *) user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      app->running = FALSE;
      break;
    case GST_MESSAGE_ERROR:
      parse_err_message (message);
      app->running = FALSE;
      break;
    case GST_MESSAGE_WARNING:
      parse_err_message (message);
      break;
    case GST_MESSAGE_STREAM_START:
      break;
    case GST_MESSAGE_QOS:
      parse_qos_message (message);
      break;
    default:
      break;
  }
}

/**
 * @brief Function to load dictionary.
 */
static gboolean
load_model_files (app_data_s * app)
{
  const gchar path[] = "./mediapipe/models";

  gboolean failed = FALSE;
  gchar *model = g_build_filename (path, "ssdlite_object_detection.tflite", NULL);

  /* check the model files */
  if (!g_file_test (model, G_FILE_TEST_EXISTS)) {
    g_critical ("Failed to get model files.");
    g_print ("Please enter as below to download and locate the model in [%s].\n", path);
    g_print ("https://github.com/google/mediapipe/blob/master/mediapipe/models/ssdlite_object_detection.tflite\n");
    failed = TRUE;
  }

  app->model_file = model;
  return !failed;
}

/**
 * @brief Function to handle input string.
 */
static void
handle_input_string (app_data_s * app)
{
  GstBuffer *buf;
  guint8 *float_array;

  float_array = g_new0 (guint8, sizeof (guint8) * VIDEO_WIDTH * VIDEO_HEIGHT * CH);
  g_assert (float_array);

  buf =
      gst_buffer_new_wrapped (float_array,
      sizeof (guint8) * VIDEO_WIDTH * VIDEO_HEIGHT * CH);
  g_assert (buf);

  printf("buffer_n_memory: %d\n", gst_buffer_n_memory (buf));

  g_assert (gst_app_src_push_buffer (GST_APP_SRC (app->src),
          buf) == GST_FLOW_OK);
  start_time = g_get_real_time ();
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  end_time = g_get_real_time ();
  avg_time += end_time - start_time;
  _print_log ("Latency: %" G_GINT64_FORMAT, end_time - start_time);
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  app_data_s *app;
  gchar *pipeline;
  GstElement *element;
  gchar *str_dim, *str_type, *str_caps;
  GstCaps *caps;

  /* init gstreamer */
  gst_init (NULL, NULL);

  /* init app variable */
  app = g_new0 (app_data_s, 1);
  g_assert (app);

  /* load model files */
  g_assert (load_model_files (app));

  /* init pipeline */
  if (g_strcmp0 (argv[1], "mediapipe") == 0)
    pipeline =
    /* framework=mediapipe */
      g_strdup_printf
      ("appsrc name=appsrc ! tensor_converter ! "
      "tensor_filter name=tfilter framework=mediapipe model=mediapipe/graphs/object_detection/object_detection_profiling.pbtxt "
      "inputname=input_video input=3:%d:%d:1 inputtype=uint8 "
      "outputname=detection_tensors output=4:2034:1:1 outputtype=uint8 ! "
      "tensor_sink name=tensor_sink", VIDEO_WIDTH, VIDEO_HEIGHT);
  else
    pipeline =
    /* framework=tensorflow-lite*/
      g_strdup_printf
      ("appsrc name=appsrc ! video/x-raw,width=%d,height=%d,format=RGB,framerate=30/1 ! "
      "videoconvert ! videoscale ! video/x-raw,width=%d,height=%d,format=RGB,framerate=30/1 ! "
      "tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
      "tensor_filter name=tfilter framework=tensorflow-lite model=%s ! "
      "tensor_sink name=tensor_sink sync=false", VIDEO_WIDTH, VIDEO_HEIGHT, MODEL_WIDTH, MODEL_HEIGHT, app->model_file);

  app->pipeline = gst_parse_launch (pipeline, NULL);
  g_free (pipeline);
  g_assert (app->pipeline);

  /* bus and message callback */
  app->bus = gst_element_get_bus (app->pipeline);
  g_assert (app->bus);

  gst_bus_add_signal_watch (app->bus);
  g_signal_connect (app->bus, "message", G_CALLBACK (bus_message_cb), app);

  /* get tensor sink element using name */
  element = gst_bin_get_by_name (GST_BIN (app->pipeline), "tensor_sink");
  g_assert (element != NULL);

  /* tensor sink signal : new data callback */
  g_signal_connect (element, "new-data", (GCallback) new_data_cb, NULL);
  gst_object_unref (element);

  /* appsrc element to push input buffer */
  app->src = gst_bin_get_by_name (GST_BIN (app->pipeline), "appsrc");
  g_assert (app->src);

  /* get tensor metadata to set caps in appsrc */
  element = gst_bin_get_by_name (GST_BIN (app->pipeline), "tfilter");
  g_assert (element);

  str_dim = str_type = NULL;
  g_object_get (element, "input", &str_dim, "inputtype", &str_type, NULL);

  str_caps =
      g_strdup_printf
      ("video/x-raw,width=%d,height=%d,format=RGB,framerate=30/1", VIDEO_WIDTH, VIDEO_HEIGHT);
  caps = gst_caps_from_string (str_caps);

  gst_app_src_set_caps (GST_APP_SRC (app->src), caps);

  g_free (str_dim);
  g_free (str_type);
  g_free (str_caps);
  gst_caps_unref (caps);
  gst_object_unref (element);

  /* Start playing */
  gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
  g_usleep(1000);
  /* ready to get input sentence */
  app->running = TRUE;

  /* get user input */
  for (int i = 0; i < TEST_LOOP; i++){
    handle_input_string (app);
    g_usleep(100000);
  }

  _print_log ("Average Latency: %" G_GINT64_FORMAT, avg_time / TEST_LOOP);
  /* stop the pipeline */
  gst_element_set_state (app->pipeline, GST_STATE_NULL);

  /* close app */
  gst_bus_remove_signal_watch (app->bus);
  gst_object_unref (app->bus);
  gst_object_unref (app->pipeline);

  g_free (app->model_file);

  g_free (app);

  return 0;
}
