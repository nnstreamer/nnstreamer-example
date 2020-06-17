/**
 * @file	example_repo_custom_easy.c
 * @date	16 Jun 2020
 * @brief	Example with tensor_repo and custom_easy for experiment.
 * @author	HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug		No known bugs.
 * 
 *  +-------------------------- slot 0 <--------------------------+
 *  |                                                             |
 *  +-->repo_src:0 --┐                ┌--filter0 -->repo_sink:0 --+
 *          appsrc --┼-->mux -->tee --┼
 *  +-->repo_src:1 --┘                └--filter1 -->repo_sink:1 --+
 *  |                                                             |
 *  +-------------------------- slot 1 <--------------------------+
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/app.h>
#include <nnstreamer/tensor_filter_custom_easy.h>

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

/**
 * @brief Data structure for app.
 */
typedef struct
{
  gboolean running;
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
  GstElement *src; /**< appsrc element in pipeline */
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
 * @brief Function to handle input string.
 */
static void
push_app_src (app_data_s * app)
{
  GstBuffer *buf;
  guint8 *data_arr;

  data_arr = g_new0 (guint8, sizeof (guint8));
  g_assert (data_arr);

  buf = gst_buffer_new_wrapped (data_arr, sizeof (guint8));
  g_assert (buf);

  printf("buffer_n_memory: %d\n", gst_buffer_n_memory (buf));
  g_assert (gst_app_src_push_buffer (GST_APP_SRC (app->src),
          buf) == GST_FLOW_OK);
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  GstMemory *mem;
  GstMapInfo info;
  guint i;
  guint num_mems;

  num_mems = gst_buffer_n_memory (buffer);
  for (i = 0; i < num_mems; i++) {
    mem = gst_buffer_peek_memory (buffer, i);

    if (gst_memory_map (mem, &info, GST_MAP_READ)) {
      _print_log ("[%s:%d] %d: %d", __FUNCTION__, __LINE__, i, info.data[0]);
      gst_memory_unmap (mem, &info);
    }
  }
}

int f1_out=100;
int f2_out=10;

static int function_1 (void *data, const GstTensorFilterProperties *prop,
    const GstTensorMemory *in, GstTensorMemory *out)
{
  g_usleep (100);
  _print_log ("[%s:%d] prop->input_meta.num_tensors: %d", __FUNCTION__, __LINE__, prop->input_meta.num_tensors);
  _print_log ("[%s:%d] input[0]: %d", __FUNCTION__, __LINE__, ((guint8*)in[0].data)[0]);
  _print_log ("[%s:%d] input[1]: %d", __FUNCTION__, __LINE__, ((guint8*)in[1].data)[0]);
  _print_log ("[%s:%d] input[2]: %d", __FUNCTION__, __LINE__, ((guint8*)in[2].data)[0]);
  ((guint8*)out[0].data)[0] = f1_out++;
  return 0; // return 1 하면 안보냄
}

static int function_2 (void *data, const GstTensorFilterProperties *prop,
    const GstTensorMemory *in, GstTensorMemory *out)
{
  g_usleep (300);
  _print_log ("[%s:%d] prop->input_meta.num_tensors: %d", __FUNCTION__, __LINE__, prop->input_meta.num_tensors);
  _print_log ("[%s:%d] input[0]: %d", __FUNCTION__, __LINE__, ((guint8*)in[0].data)[0]);
  _print_log ("[%s:%d] input[1]: %d", __FUNCTION__, __LINE__, ((guint8*)in[1].data)[0]);
  _print_log ("[%s:%d] input[2]: %d", __FUNCTION__, __LINE__, ((guint8*)in[2].data)[0]);
  ((guint8*)out[0].data)[0] = f2_out++;
  return 0;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  app_data_s *app;
  gchar *pipeline;
  // GstElement *element;

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* init app variable */
  app = g_new0 (app_data_s, 1);
  g_assert (app);

  /* init pipeline */
  pipeline =
      g_strdup_printf (
        "tensor_mux name=mux sync_mode=slowest silent=false ! tee name=t "
          "t. ! queue ! tensor_filter framework=custom-easy silent=false model=model1 ! tensor_reposink slot-index=0 "
          "t. ! queue ! tensor_filter framework=custom-easy silent=false model=model2 ! tensor_reposink slot-index=1 "
          "t. ! queue ! tensor_sink silent=false name=sink async=false "

          "tensor_reposrc slot-index=0 caps=\"other/tensor,dimension=(string)1:1:1:1,type=(string)uint8,framerate=(fraction)0/1\" ! mux.sink_0 "
          "tensor_reposrc slot-index=1 caps=\"other/tensor,dimension=(string)1:1:1:1,type=(string)uint8,framerate=(fraction)0/1\" ! mux.sink_1 "
          "appsrc name=appsrc caps=application/octet-stream,framerate=(fraction)0/1 ! tensor_converter silent=false input-dim=1:1:1:1 input-type=uint8 ! mux.sink_2 "
      );

 /* setting tensor_filter custom-easy */
  const GstTensorsInfo info_in_1 = {
    .num_tensors = 3U,
    .info = {
      { .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}},
      { .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}},
      { .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}}},
    };
  const GstTensorsInfo info_out_1 = {
    .num_tensors = 1U,
    .info = {{ .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}}},
  };
  const GstTensorsInfo info_in_2 = {
    .num_tensors = 3U,
    .info = {
    { .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}},
    { .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}},
    { .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}}},
  };
  const GstTensorsInfo info_out_2 = {
    .num_tensors = 1U,
    .info = {{ .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}}},
  };

  NNS_custom_easy_register ("model1", function_1,
      NULL, &info_in_1, &info_out_1);
  
  NNS_custom_easy_register ("model2", function_2,
      NULL, &info_in_2, &info_out_2);

  app->pipeline = gst_parse_launch (pipeline, NULL);
  g_free (pipeline);
  g_assert (app->pipeline);

  /* bus and message callback */
  app->bus = gst_element_get_bus (app->pipeline);
  g_assert (app->bus);

  gst_bus_add_signal_watch (app->bus);
  g_signal_connect (app->bus, "message", G_CALLBACK (bus_message_cb), app);

  /* get tensor sink element using name */
  GstElement *element = gst_bin_get_by_name (GST_BIN (app->pipeline), "sink");
  g_assert (element != NULL);

  /* tensor sink signal : new data callback */
  g_signal_connect (element, "new-data", (GCallback) new_data_cb, NULL);
  gst_object_unref (element);

  /* appsrc element to push input buffer */
  app->src = gst_bin_get_by_name (GST_BIN (app->pipeline), "appsrc");
  g_assert (app->src);

  /* Start playing */
  gst_element_set_state (app->pipeline, GST_STATE_PLAYING);
  /* ready to get input sentence */
  app->running = TRUE;

  for(int i = 0; i < 10; i++){
    push_app_src (app);
    g_usleep(100000);
  }

  /* stop the pipeline */
  gst_element_set_state (app->pipeline, GST_STATE_NULL);

  /* close app */
  NNS_custom_easy_unregister("model1");
  NNS_custom_easy_unregister("model2");
  gst_bus_remove_signal_watch (app->bus);
  gst_object_unref (app->bus);
  gst_object_unref (app->pipeline);

  g_free (app);

  return 0;
}
