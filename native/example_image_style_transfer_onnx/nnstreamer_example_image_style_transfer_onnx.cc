/**
 * @file	nnstreamer_example_image_style_transfer_onnx.cc
 * @date	02 Jan 2024
 * @brief	image style transfer example
 * @author	Suyeon Kim <suyeon5.kim@samsung.com>
 * @bug		No known bugs.
 *
 * Sample code for image style transfer, this app shows video frame.
 *
 * Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer
 * plug-in. $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./nnstreamer_example_image_style_transfer_onnx
 */

#include <fcntl.h>
#include <unistd.h>

#include <gst/gst.h>

/**
 * @brief Macro for debug mode.
 */
#ifndef DBG
#define DBG false
#endif

/**
 * @brief Macro for debug message.
 */
#define _print_log(...)                                                        \
  if (DBG)                                                                     \
  g_message(__VA_ARGS__)

/**
 * @brief Macro to check error case.
 */
#define _check_cond_err(cond)                                                  \
  do {                                                                         \
    if (!(cond)) {                                                             \
      _print_log("app failed! [line : %d]", __LINE__);                         \
      goto error;                                                              \
    }                                                                          \
  } while (0)

/**
 * @brief Data structure for app.
 */
typedef struct {
  GMainLoop *loop;      /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus;          /**< gst bus for data pipeline */
  gchar *model_file[4]; /**< onnx model file */

} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Free resources in app data.
 */
static void _free_app_data(void) {
  if (g_app.loop) {
    g_main_loop_unref(g_app.loop);
    g_app.loop = NULL;
  }

  if (g_app.bus) {
    gst_bus_remove_signal_watch(g_app.bus);
    gst_object_unref(g_app.bus);
    g_app.bus = NULL;
  }

  if (g_app.pipeline) {
    gst_object_unref(g_app.pipeline);
    g_app.pipeline = NULL;
  }
}

/**
 * @brief Function to print error message.
 */
static void _parse_err_message(GstMessage *message) {
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
 * @brief Callback for message.
 */
static void _message_cb(GstBus *bus, GstMessage *message, gpointer user_data) {
  switch (GST_MESSAGE_TYPE(message)) {
  case GST_MESSAGE_EOS:
    _print_log("received eos message");
    g_main_loop_quit(g_app.loop);
    break;

  case GST_MESSAGE_ERROR:
    _print_log("received error message");
    _parse_err_message(message);
    g_main_loop_quit(g_app.loop);
    break;

  case GST_MESSAGE_WARNING:
    _print_log("received warning message");
    _parse_err_message(message);
    break;

  case GST_MESSAGE_STREAM_START:
    _print_log("received start message");
    break;

  default:
    break;
  }
}

/**
 * @brief Function to load a model
 */
static gboolean load_model_file(AppData *app) {
  const gchar path[] = "./onnx_image_style_transfer";

  gboolean failed = FALSE;
  gchar *model[4];
  guint i;

  model[0] = g_build_filename(path, "candy.onnx", NULL);
  model[1] = g_build_filename(path, "la_muse.onnx", NULL);
  model[2] = g_build_filename(path, "mosaic.onnx", NULL);
  model[3] = g_build_filename(path, "udnie.onnx", NULL);

  for (i = 0; i < 4; i++) {
    if (!g_file_test(model[i], G_FILE_TEST_EXISTS)) {
      g_critical("Failed to get %s model files.", model[i]);
      g_print("Please enter as below to download and locate the model.\n");
      g_print("$ cd $NNST_ROOT/bin/ \n");
      g_print("$ ./get-model.sh image_style_transfer_onnx \n");

      failed = TRUE;
    }
  }

  app->model_file[0] = model[0];
  app->model_file[1] = model[1];
  app->model_file[2] = model[2];
  app->model_file[3] = model[3];

  return !failed;
}

/**
 * @brief Set window title.
 * @param name GstXImageSink element name
 * @param title window title
 */
static void _set_window_title(const gchar *name, const gchar *title) {
  GstTagList *tags;
  GstPad *sink_pad;
  GstElement *element;

  element = gst_bin_get_by_name(GST_BIN(g_app.pipeline), name);

  g_return_if_fail(element != NULL);

  sink_pad = gst_element_get_static_pad(element, "sink");

  if (sink_pad) {
    tags = gst_tag_list_new(GST_TAG_TITLE, title, NULL);
    gst_pad_send_event(sink_pad, gst_event_new_tag(tags));
    gst_object_unref(sink_pad);
  }

  gst_object_unref(element);
}

/**
 * @brief Main function.
 */
int main(int argc, char **argv) {
  const guint width = 720;
  const guint height = 720;
  const gchar *model[4] = {"candy", "la_muse", "mosaic", "udnie"};

  gchar *str_pipeline;
  gchar *prev_pipeline;
  guint i;
  gulong handle_id;
  GstElement *element;

  _print_log("start app..");

  /* init gstreamer */
  gst_init(&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new(NULL, FALSE);
  _check_cond_err(g_app.loop != NULL);

  /* Load model files */
  g_assert(load_model_file(&g_app));

  /* init pipeline */
  str_pipeline = g_strdup_printf(
      "v4l2src name=cam_src ! videoconvert ! videoscale ! "
      "video/x-raw,width=%d,height=%d,format=RGB ! tee name=t_raw "
      "t_raw. ! queue ! videoconvert ! ximagesink name=img_origin ",
      width, height);

  for (i = 0; i < 4; i++) {
    prev_pipeline = str_pipeline;
    str_pipeline = g_strdup_printf(
        "%s"
        "t_raw. ! queue leaky=2 max-size-buffers=10 ! tensor_converter ! "
        "tensor_transform mode=transpose option=1:2:0:3 ! "
        "tensor_transform mode=arithmetic option=typecast:float32,add:0.0 ! "
        "tensor_filter framework=onnxruntime model=%s ! "
        "tensor_converter ! tensor_transform mode=transpose option=2:0:1:3 ! "
        "tensor_transform mode=arithmetic option=typecast:uint8,add:0.0 ! "
        "tensor_decoder mode=direct_video ! videoconvert ! "
        "ximagesink name=%s_img sync=false ",
        prev_pipeline, g_app.model_file[i], model[i]);
        
    g_free(prev_pipeline);
  }
  _print_log("%s\n", str_pipeline);
  g_app.pipeline = gst_parse_launch(str_pipeline, NULL);
  g_free(str_pipeline);
  _check_cond_err(g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus(g_app.pipeline);
  _check_cond_err(g_app.bus != NULL);

  gst_bus_add_signal_watch(g_app.bus);
  handle_id =
      g_signal_connect(g_app.bus, "message", (GCallback)_message_cb, NULL);
  _check_cond_err(handle_id > 0);

  /* start pipeline */
  gst_element_set_state(g_app.pipeline, GST_STATE_PLAYING);

  /* set window title */
  _set_window_title("img_origin", "Original");
  _set_window_title("candy_img", "candy");
  _set_window_title("la_muse_img", "la_muse");
  _set_window_title("mosaic_img", "mosaic");
  _set_window_title("udnie_img", "udnie");

  /* run main loop */
  g_main_loop_run(g_app.loop);

  /* cam source element */
  element = gst_bin_get_by_name(GST_BIN(g_app.pipeline), "cam_src");

  gst_element_set_state(element, GST_STATE_READY);
  gst_element_set_state(g_app.pipeline, GST_STATE_READY);

  g_usleep(200 * 1000);

  gst_element_set_state(element, GST_STATE_NULL);
  gst_element_set_state(g_app.pipeline, GST_STATE_NULL);

  g_usleep(200 * 1000);
  gst_object_unref(element);

error:
  _print_log("close app..");
  _free_app_data();
  return 0;
}
