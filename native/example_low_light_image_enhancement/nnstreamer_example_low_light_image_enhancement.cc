/**
 * @file	nnstreamer_example_low_light_image_enhancement.cc
 * @date	14 Jan 2022
 * @brief	example with TF-Lite model for low light image enhancement
 * @see		https://github.com/nnstreamer/nnstreamer
 * @author	Yelin Jeong <yelini.jeong@samsung.com>
 * @bug		No known bugs.
 *
 * Get model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model.sh low-light-image-enhancement
 */

#include <glib.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <opencv4/opencv2/opencv.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define IMG_HEIGHT 400
#define IMG_WIDTH 600
#define MAX_FILE_NAME 50

using namespace cv;

/**
 * @brief Data structure for app.
 */
typedef struct {
  gboolean running;

  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus;          /**< gst bus for data pipeline */
  GstElement *src;      /**< appsrc element in pipeline */

  gchar *model_file; /**< tensorflow-lite model file */
  gchar *image_file;
} app_data_s;

/**
 * @brief Function to print error message.
 */
static void parse_err_message(GstMessage *message) {
  gchar *debug;
  GError *error;

  g_return_if_fail(message != NULL);

  switch (GST_MESSAGE_TYPE(message)) {
  case GST_MESSAGE_ERROR:
    gst_message_parse_error(message, &error, &debug);
    break;

  case GST_MESSAGE_WARNING:
    gst_message_parse_warning(message, &error, &debug);
    break;

  default:
    return;
  }

  gst_object_default_error(GST_MESSAGE_SRC(message), error, debug);
  g_error_free(error);
  g_free(debug);
}

/**
 * @brief Function to print qos message.
 */
static void parse_qos_message(GstMessage *message) {
  GstFormat format;
  guint64 processed;
  guint64 dropped;

  gst_message_parse_qos_stats(message, &format, &processed, &dropped);
  g_warning("format[%d] processed[%" G_GUINT64_FORMAT
            "] dropped[%" G_GUINT64_FORMAT "]",
            format, processed, dropped);
}

/**
 * @brief Callback for message.
 */
static void bus_message_cb(GstBus *bus, GstMessage *message,
                           gpointer user_data) {
  app_data_s *app;

  app = (app_data_s *)user_data;

  switch (GST_MESSAGE_TYPE(message)) {
  case GST_MESSAGE_EOS:
    app->running = FALSE;
    break;
  case GST_MESSAGE_ERROR:
    parse_err_message(message);
    app->running = FALSE;
    break;
  case GST_MESSAGE_WARNING:
    parse_err_message(message);
    break;
  case GST_MESSAGE_STREAM_START:
    break;
  case GST_MESSAGE_QOS:
    parse_qos_message(message);
    break;
  default:
    break;
  }
}

/**
 * @brief Function to load a model
 */
static gboolean load_model_file(app_data_s *app) {
  const gchar path[] = "./tflite_low_light_image_enhancement";

  gboolean failed = FALSE;

  gchar *model = g_build_filename(path, "lite-model_zero-dce_1.tflite", NULL);

  if (!g_file_test(model, G_FILE_TEST_EXISTS)) {
    g_critical("Failed to get model files.");
    g_print("Please enter as below to download and locate the model.\n");
    g_print("$ cd $NNST_ROOT/bin/ \n");
    g_print("$ ./get-model.sh low-light-image-enhancement \n");
    failed = TRUE;
  }

  app->model_file = model;
  return !failed;
}

/**
 * @brief Function to handle input string.
 */
static void handle_input_image(app_data_s *app) {
  GstBuffer *buf;
  gchar input[MAX_FILE_NAME] = {
      0,
  };

  g_print("Please enter your png file name : ");
  if (fgets(input, sizeof(input), stdin)) {
    if (input[strlen(input) - 1] == '\n') {
      input[strlen(input) - 1] = '\0';
    }

    if (g_strcmp0(input, "quit")) {
      app->image_file = g_strconcat(input, ".png", NULL);
    } else {
      app->running = FALSE;
      return;
    }
  }

  Mat img = imread(app->image_file);
  if (!img.empty()) {
    gint width = img.cols;
    gint height = img.rows;
    gint bpp = img.channels();
    gint imagesize = width * height * bpp;

    gint *buffer_array = (gint *)g_malloc(imagesize);
    memcpy(buffer_array, img.data, imagesize);

    buf = gst_buffer_new_wrapped(buffer_array, imagesize);
    g_assert(buf);
    g_assert(gst_app_src_push_buffer(GST_APP_SRC(app->src), buf) ==
             GST_FLOW_OK);
  } else {
    g_critical("Failed to find %s", app->image_file);
  }
}

/**
 * @brief Callback for tensor sink signal.
 */
static void new_data_cb(GstElement *element, GstBuffer *buffer,
                        gpointer user_data) {
  app_data_s *app;
  GstMemory *mem;
  GstMapInfo info;
  gfloat *output;

  mem = gst_buffer_peek_memory(buffer, 0);
  if (gst_memory_map(mem, &info, GST_MAP_READ)) {
    g_assert(info.size == IMG_HEIGHT * IMG_WIDTH * 3 * 4);
    output = (gfloat *)info.data;
    guchar image[IMG_HEIGHT][IMG_WIDTH][3];

    gint idx = 0;
    for (gint i = 0; i < IMG_HEIGHT; i++) {
      for (gint j = 0; j < IMG_WIDTH; j++) {
        for (gint k = 0; k < 3; k++) {
          output[idx] *= 255;
          if (output[idx] < 0)
            output[idx] = 0;
          else if (output[idx] > 255)
            output[idx] = 255;
          image[i][j][k] = output[idx++];
        }
      }
    }

    Mat enhancement_image = Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC3, image);
    app = (app_data_s *)user_data;

    gchar *enhancement_image_file =
        g_strconcat("low_light_enhancement_", app->image_file, NULL);
    imwrite(enhancement_image_file, enhancement_image);
    g_print("%s created!\n", enhancement_image_file);
    gst_memory_unmap(mem, &info);
  }
}

/**
 * @brief Main function.
 */
int main(int argc, char **argv) {
  app_data_s *app;
  gchar *pipeline;
  GstElement *element;
  GstStateChangeReturn ret;

  /* Initialize GStreamer */
  gst_init(&argc, &argv);

  /* Initialize app variable */
  app = g_new0(app_data_s, 1);
  g_assert(app);

  /* Load model files */
  g_assert(load_model_file(app));

  /* Initialize pipeline */
  pipeline = g_strdup_printf(
      "appsrc name=appsrc ! "
      "other/tensor,type=uint8,dimension=3:%d:%d:1,framerate=0/1 ! "
      "tensor_transform mode=arithmetic "
      "option=typecast:float32,add:0,div:255.0 ! "
      "tensor_filter name=tfilter framework=tensorflow-lite model=%s ! "
      "tensor_sink name=tensor_sink",
      IMG_WIDTH, IMG_HEIGHT, app->model_file);

  app->pipeline = gst_parse_launch(pipeline, NULL);
  g_free(pipeline);
  g_assert(app->pipeline);

  /* Listen to the bus */
  app->bus = gst_element_get_bus(app->pipeline);
  g_assert(app->bus);

  /* Message callback */
  gst_bus_add_signal_watch(app->bus);
  g_signal_connect(app->bus, "message", G_CALLBACK(bus_message_cb), app);

  /* get tensor sink element using name */
  element = gst_bin_get_by_name(GST_BIN(app->pipeline), "tensor_sink");
  g_assert(element != NULL);

  /* tensor sink signal : new data callback */
  g_signal_connect(element, "new-data", (GCallback)new_data_cb, app);
  gst_object_unref(element);

  /* appsrc element to push input buffer */
  app->src = gst_bin_get_by_name(GST_BIN(app->pipeline), "appsrc");
  g_assert(app->src);

  /* get tensor metadata to set caps in appsrc */
  element = gst_bin_get_by_name(GST_BIN(app->pipeline), "tfilter");
  g_assert(element);

  gst_object_unref(element);

  /* Start playing */
  ret = gst_element_set_state(app->pipeline, GST_STATE_PLAYING);

  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr("Unable to set the pipeline to the playing state.\n");
    gst_object_unref(app->pipeline);
    return -1;
  }

  app->running = TRUE;

  while (app->running) {
    handle_input_image(app);
    g_usleep(2000 * 1000);
  }

  /* Free resources */
  if (app->bus) {
    gst_object_unref(app->bus);
  }
  if (app->pipeline) {
    gst_element_set_state(app->pipeline, GST_STATE_NULL);
    gst_object_unref(app->pipeline);
  }
  return 0;
}
