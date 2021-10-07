/**
 * @file	tensor_query_performance_benchmark.c
 * @date	06 Oct 2021
 * @brief	edgeAI performance benchmark - query
 * @author	Gichan Jang <gichan2.jnag@samsung.com>
 * @bug		No known bugs.
 */

#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>
#include <getopt.h>
#include <nnstreamer_plugin_api.h>
#include <nnstreamer/tensor_filter_custom_easy.h>

static GMainLoop *loop; /**< main event loop */
guint received;

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
 * @brief Timer callback for client
 */
static gboolean
timeout_cb (gpointer user_data)
{
  g_message ("timeout_cb");
  if (loop) {
    g_main_loop_quit (loop);
    g_main_loop_unref (loop);
    loop = NULL;
  }
  return FALSE;
}

/**
 * @brief Print usage info
 */
static void
_usage (void)
{
  g_message ("\nusage: \n"
  "    --server    Run server pipeline. (default) \n"
  "    --client    Run client pipeline. \n"
  "    --operation Set MQTT-hybrid operation. \n"
  "    --srchost   Set query server source host address. \n"
  "    --srcport   Set query server source port. \n"
  "    --sinkhost  Set query server sink host address. \n"
  "    --sinkport  Set query server sink port. \n"
  "    --mqtthost  Set mqtt host address. \n"
  "    --repeat    Set the number of repetitions. \n"
  "    --timeout   Set the running time in Sec. \n"
  "    --width     Set the width of the video. \n"
  "    --height    Set the height of the video. \n");
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
_new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  /* print progress */
  received++;
  g_message ("receiving new data [%d]", received);
}

/**
 * @brief Function for custom-easy filter
 */
static int
ce_custom_scale (void *data, const GstTensorFilterProperties *prop,
    const GstTensorMemory *in, GstTensorMemory *out)
{
  unsigned int t;
  for (t = 0; t < prop->output_meta.num_tensors; t++) {
    if (prop->input_meta.num_tensors <= t)
      memset (out[t].data, 0, out[t].size);
    else
      memcpy (out[t].data, in[t].data, MIN (in[t].size, out[t].size));
  }
  return 0;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  gchar *str_pipeline = NULL, *in_dim = NULL;
  gboolean is_server = TRUE;
  GstElement *pipeline, *element;
  gchar *src_host, *sink_host, *operation = NULL, *mqtt_host = NULL;
  guint16 src_port = 5001, sink_port = 5000, repeat = 1, timeout = 10, width=640, height=480;
  gint opt;
  struct option long_options[] = {
      { "server", no_argument, NULL, 's' },
      { "client", no_argument, NULL, 'c' },
      { "operation", required_argument, NULL, 'o' },
      { "srchost", required_argument,  NULL, 'u' },
      { "srcport", required_argument,  NULL, 'b' },
      { "sinkhost", required_argument,  NULL, 'k' },
      { "sinkport", required_argument,  NULL, 'n' },
      { "mqtthost", required_argument,  NULL, 'm' },
      { "repeat", required_argument,  NULL, 'r' },
      { "timeout", required_argument,  NULL, 't' },
      { "help", required_argument,  NULL, 'h' },
      { "width", required_argument,  NULL, 'w' },
      { "height", required_argument,  NULL, 'a' },
      { 0, 0, 0, 0}
  };
  gchar *optstring = "s:c:o:u:b:k:n:m:r:t:h:w:a";
  /* setting tensor_filter custom-easy */
  GstTensorsInfo info_in;
  const GstTensorsInfo info_out = {
    .num_tensors = 1U,
    .info = {{.name = NULL,.type = _NNS_UINT8,.dimension = {10, 10, 1, 1}}},
  };

  /* init gstreamer */
  gst_init (&argc, &argv);

  src_host = g_strdup ("localhost");
  sink_host = g_strdup ("localhost");
  operation = g_strdup ("");
  mqtt_host = g_strdup ("");

  while ((opt = getopt_long (argc, argv, optstring, long_options, NULL)) != -1) {
    switch (opt) {
      case 's':
        is_server = TRUE;
        break;
      case 'c':
        is_server = FALSE;
        break;
      case 'o':
        g_free (operation);
        operation = g_strdup_printf ("operation=%s", optarg);
        break;
      case 'u':
        g_free (src_host);
        src_host = g_strdup (optarg);
        break;
      case 'b':
        src_port = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 'k':
        g_free (sink_host);
        sink_host = g_strdup (optarg);
        break;
      case 'n':
        sink_port = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 'm':
        g_free (mqtt_host);
        mqtt_host = g_strdup_printf ("mqtt-host=%s", optarg);
        break;
      case 'r':
        repeat = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 't':
        timeout = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 'w':
        width = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 'a':
        height = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      default:
        _usage ();
        return 0;
    }
  }

  gst_tensors_info_init (&info_in);
  info_in.num_tensors = 1U;
  info_in.info[0].name = NULL;
  info_in.info[0].type = _NNS_UINT8;
  in_dim = g_strdup_printf ("3:%u:%u:1", width, height);
  gst_tensor_parse_dimension (in_dim, info_in.info[0].dimension);

  g_print ("src host: %s, src port: %u, sink host: %s, sink port: %u\n", src_host, src_port, sink_host, sink_port);
  g_print ("operation: %s, repeat: %u \n\n", operation, repeat);

  /* Create main loop and pipeline */
  loop = g_main_loop_new (NULL, FALSE);
  if (is_server) {
    str_pipeline =
        g_strdup_printf
        ("tensor_query_serversrc host=%s port=%u %s %s ! other/tensors,num_tensors=1,dimensions=3:%u:%u:1,types=uint8,framerate=60/1,format=static ! "
         "tensor_filter framework=custom-easy model=custom_scale ! "
         "tensor_query_serversink host=%s port=%u",
          src_host, src_port, operation, mqtt_host, width, height, sink_host, sink_port);
  } else {
    str_pipeline =
        g_strdup_printf
        ("videotestsrc ! videoconvert ! videoscale ! video/x-raw,width=%u,height=%u,format=RGB,framerate=60/1 ! "
            "tensor_converter ! tensor_query_client src-host=%s src-port=%u sink-host=%s sink-port=%u %s %s ! "
            "tensor_sink name=sinkx sync=true", width, height, src_host, src_port, sink_host, sink_port, operation, mqtt_host);
  }
  g_print ("%s\n", str_pipeline);

  NNS_custom_easy_register ("custom_scale", ce_custom_scale, NULL, &info_in, &info_out);

  pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);

  /** Shut down the application after timeout. */
  g_timeout_add_seconds (timeout, timeout_cb, NULL);

  element = gst_bin_get_by_name (GST_BIN (pipeline), "sinkx");
  g_signal_connect (element, "new-data", (GCallback) _new_data_cb, NULL);
  gst_object_unref (element);
  received = 0;

  /* start pipeline */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* run main loop */
  g_main_loop_run (loop);
  g_usleep (200 * 1000);

  g_message ("Received data cnt: %u", received);
  gst_element_set_state (pipeline, GST_STATE_PAUSED);
  g_usleep (200 * 1000);

  gst_element_set_state (pipeline, GST_STATE_READY);
  g_usleep (200 * 1000);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_usleep (200 * 1000);

  gst_object_unref (pipeline);
  pipeline = NULL;
  g_free (src_host);
  g_free (sink_host);
  g_free (mqtt_host);
  g_free (operation);
  g_free (in_dim);
  NNS_custom_easy_unregister ("custom_scale_10x10");

  return 0;
}
