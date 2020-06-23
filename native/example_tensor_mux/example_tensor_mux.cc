/**
 * @file	example_tensor_mux.c
 * @date	22 Jun 2020
 * @brief	Example with tensor_mux
 * @author	HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug		No known bugs.
 * 
 * appsrc ──> tee ──> tensor_filter:0 ──> tensor_mux ──> tensor_sink
 *             │                              ↑
 *             └────> tensor_filter:1 ────────┘ 
 * 
 * At the above pipeline, the `tensor_filter:1` has 3 modes
 *  1. on
 *  2. off
 *  3. delay (for N secs)
 * 
 * According to the mode of `tensor_filter:1`, the sync_mode of 
 * `tensor_mux` will also be changed.
 *  1. sync_mode=slowest
 *  2. sync_mode=refresh
 *  3. sync_mode=slowest
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/app.h>
#include <nnstreamer/tensor_filter_custom_easy.h>

#define MODE_1 '1'
#define MODE_2 '2'
#define MODE_3 '3'

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

static gchar TEST_MODE = MODE_1;

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
  data_arr[0] = 1;

  buf = gst_buffer_new_wrapped (data_arr, sizeof (guint8));
  g_assert (buf);

  g_message ("[%s] push %d tensor(s)", __FUNCTION__, gst_buffer_n_memory (buf));
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

int out_1 = 10;
static int function_1 (void *data, const GstTensorFilterProperties *prop,
    const GstTensorMemory *in, GstTensorMemory *out)
{
  ( (guint8*)out[0].data)[0] = out_1++;
  return 0;
}

int is_sent = FALSE;
static int function_2 (void *data, const GstTensorFilterProperties *prop,
    const GstTensorMemory *in, GstTensorMemory *out)
{
  int ret = 0;
  switch (TEST_MODE) {
    case MODE_1:
      ( (guint8*) out[0].data)[0] = 100;
      ret = 0;
      break;
    case MODE_2:
      /* push no data */
      ( (guint8*) out[0].data)[0] = 150;
      if (is_sent)
        /* send data just once */
        ret = 1;
      else
        ret = 0;
      break;
    case MODE_3:
      ( (guint8*) out[0].data)[0] = 200;
      g_message ("wait for 3 seconds at model2");
      g_usleep (1000000);
      g_message ("1");
      g_usleep (1000000);
      g_message ("2");
      g_usleep (1000000);
      g_message ("3. Send!");
      ret = 0;
      break;

    default:
      ret = 1;
      break;
  }
  is_sent = TRUE;
  return ret;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  app_data_s *app;
  gchar *pipeline;
  gchar *sync_mode;
  
  if (argv[1] == NULL){
    g_critical ("please input the TEST MODE: 1-3. e.g. nnstreamer_example_tensor_mux 1");
    return -1;
  }
  TEST_MODE = argv[1][0];
  switch (TEST_MODE){
    case MODE_1:
      sync_mode = g_strdup_printf ("slowest");
      break;
    
    case MODE_2:
      sync_mode = g_strdup_printf ("refresh");
      break;

    case MODE_3:
      sync_mode = g_strdup_printf ("slowest");
      break;
    
    default:
      g_critical ("please input the TEST MODE: 1-3");
      return -1;
  }
  /* init gstreamer */
  gst_init (&argc, &argv);

  /* init app variable */
  app = g_new0 (app_data_s, 1);
  g_assert (app);

  /* init pipeline */
  pipeline =
      g_strdup_printf (
        "tensor_mux name=mux sync_mode=%s silent=false ! tensor_sink name=sink "

        "appsrc name=appsrc caps=other/tensor,dimension=(string)1:1:1:1,type=(string)uint8,framerate=(fraction)0/1 ! "
        "tee name=t "
          "t. ! queue ! tensor_filter framework=custom-easy model=model1 ! queue ! mux.sink_0 "
          "t. ! queue ! tensor_filter framework=custom-easy model=model2 ! queue ! mux.sink_1 "
        , sync_mode);

 /* setting tensor_filter custom-easy */
  const GstTensorsInfo info_in = {
    .num_tensors = 1U,
    .info = {{ .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}}},
  };
  const GstTensorsInfo info_out = {
    .num_tensors = 1U,
    .info = {{ .name = NULL, .type = _NNS_UINT8, .dimension = { 1, 1, 1, 1}}},
  };

  NNS_custom_easy_register ("model1", function_1,
      NULL, &info_in, &info_out);
  
  NNS_custom_easy_register ("model2", function_2,
      NULL, &info_in, &info_out);

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

  /* insert the starting input */
  push_app_src (app);

  /* push once more at MODE_2 */
  if (TEST_MODE == MODE_2) {
    g_usleep (10000);
    push_app_src (app);
  }

  /* run the pipeline for seconds */
  if (TEST_MODE == MODE_3)
    g_usleep (3 * 1000000);
  else
    g_usleep (500000);

  if (gst_app_src_end_of_stream (GST_APP_SRC (app->src)) != GST_FLOW_OK) {
    g_critical ("failed to set eos");
  }

  /* wait until EOS done */
  g_usleep (200000);

  /* stop the pipeline */
  gst_element_set_state (app->pipeline, GST_STATE_READY);
  gst_element_set_state (app->pipeline, GST_STATE_NULL);

  /* close app */
  NNS_custom_easy_unregister ("model1");
  NNS_custom_easy_unregister ("model2");
  gst_bus_remove_signal_watch (app->bus);
  gst_object_unref (app->bus);
  gst_object_unref (app->pipeline);

  g_free (app);
  g_free (sync_mode);

  return 0;
}
