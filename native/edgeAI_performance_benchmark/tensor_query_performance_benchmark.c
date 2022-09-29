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
 * @brief Print usage info
 */
static void
_usage (void)
{
  g_message ("\nusage: \n"
  "    --server    Run server pipeline. (default) \n"
  "    --client    Run client pipeline. \n"
  "    --topic Set MQTT-hybrid topic. \n"
  "    --srvhost   Set query server source host address. \n"
  "    --srvport   Set query server source port. \n"
  "    --clienthost  Set query server sink host address. \n"
  "    --clientport  Set query server sink port. \n"
  "    --brokerhost  Set mqtt host address. \n"
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
  gchar *srv_host, *client_host, *topic = NULL, *dest_host = NULL, *connect_type = NULL;
  guint16 srv_port = 5001, dest_port = 1883, repeat = 1, timeout = 10, width=640, height=480, framerate = 60;
  gint opt;
  struct option long_options[] = {
      { "server", no_argument, NULL, 's' },
      { "client", no_argument, NULL, 'c' },
      { "topic", required_argument, NULL, 'o' },
      { "srvhost", required_argument,  NULL, 'u' },
      { "srvport", required_argument,  NULL, 'b' },
      { "clienthost", required_argument,  NULL, 'k' },
      { "clientport", required_argument,  NULL, 'n' },
      { "desthost", required_argument,  NULL, 'm' },
      { "destport", required_argument,  NULL, 'd' },
      { "repeat", required_argument,  NULL, 'r' },
      { "timeout", required_argument,  NULL, 't' },
      { "help", required_argument,  NULL, 'h' },
      { "width", required_argument,  NULL, 'w' },
      { "height", required_argument,  NULL, 'a' },
      { "connecttype", required_argument,  NULL, 'p' },
      { "framerate", required_argument,  NULL, 'f' },
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

  srv_host = g_strdup ("localhost");
  client_host = g_strdup ("localhost");
  topic = g_strdup ("");
  dest_host = g_strdup ("");
  connect_type = g_strdup ("TCP");

  while ((opt = getopt_long (argc, argv, optstring, long_options, NULL)) != -1) {
    switch (opt) {
      case 's':
        is_server = TRUE;
        break;
      case 'c':
        is_server = FALSE;
        break;
      case 'o':
        g_free (topic);
        topic = g_strdup_printf ("topic=%s", optarg);
        break;
      case 'u':
        g_free (srv_host);
        srv_host = g_strdup (optarg);
        break;
      case 'b':
        srv_port = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 'k':
        g_free (client_host);
        client_host = g_strdup (optarg);
        break;
      case 'm':
        g_free (dest_host);
        dest_host = g_strdup (optarg);
        break;
      case 'd':
        dest_port = (guint16) g_ascii_strtoll (optarg, NULL, 10);
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
      case 'p':
        g_free (connect_type);
        connect_type = g_strdup (optarg);
        break;
      case 'f':
        framerate = (guint16) g_ascii_strtoll (optarg, NULL, 10);
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

  g_print ("srv host: %s, srv port: %u, client host: %s\n", srv_host, srv_port, client_host);
  g_print ("topic: %s, repeat: %u \n\n", topic, repeat);

  /* Create pipeline */
  if (0 == g_strcmp0 (connect_type, "TCP")) {
    if (is_server) {
      str_pipeline =
          g_strdup_printf
          ("tensor_query_serversrc host=%s port=%u ! other/tensors,num_tensors=1,dimensions=3:%u:%u:1,types=uint8,framerate=%u/1,format=static ! "
          "tensor_filter framework=custom-easy model=custom_scale ! "
          "tensor_query_serversink",
            srv_host, srv_port, width, height, framerate);
    } else {
      str_pipeline =
          g_strdup_printf
          ("videotestsrc is-live=true ! videoconvert ! videoscale ! video/x-raw,width=%u,height=%u,format=RGB,framerate=%u/1 ! "
              "tensor_converter ! tensor_query_client host=%s port=0 dest-host=%s dest-port=%u ! "
              "tensor_sink name=sinkx sync=true", width, height, framerate, client_host, srv_host, srv_port);
    }
  } else {
    if (is_server) {
      str_pipeline =
          g_strdup_printf
          ("tensor_query_serversrc host=%s port=%u dest-host=%s dest=port=%u %s connect-type=%s ! other/tensors,num_tensors=1,dimensions=3:%u:%u:1,types=uint8,framerate=%u/1,format=static ! "
          "tensor_filter framework=custom-easy model=custom_scale ! "
          "tensor_query_serversink connect-type=%s",
            srv_host, srv_port, dest_host, dest_port, topic, connect_type, width, height, framerate, connect_type);
    } else {
      str_pipeline =
          g_strdup_printf
          ("videotestsrc is-live=true ! videoconvert ! videoscale ! video/x-raw,width=%u,height=%u,format=RGB,framerate=%u/1 ! "
              "tensor_converter ! tensor_query_client connect-type=%s host=%s port=0 dest-host=%s dest-port=%u %s ! "
              "tensor_sink name=sinkx sync=true", width, height, framerate, connect_type, client_host, dest_host, dest_port, topic);
    }
  }

  g_print ("%s\n", str_pipeline);

  NNS_custom_easy_register ("custom_scale", ce_custom_scale, NULL, &info_in, &info_out);

  pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);

  /** Shut down the application after timeout. */
  element = gst_bin_get_by_name (GST_BIN (pipeline), "sinkx");
  g_signal_connect (element, "new-data", (GCallback) _new_data_cb, NULL);
  gst_object_unref (element);
  received = 0;

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_message ("Start pipeline");

  g_usleep ((timeout + 1) * 1000 * 1000);

  g_message ("Received data cnt: %u", received);
  gst_element_set_state (pipeline, GST_STATE_PAUSED);
  g_usleep (200 * 1000);

  gst_element_set_state (pipeline, GST_STATE_READY);
  g_usleep (200 * 1000);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_usleep (200 * 1000);

  g_message ("Stop pipeline");
  NNS_custom_easy_unregister ("custom_scale_10x10");
  gst_object_unref (pipeline);
  pipeline = NULL;
  g_free (srv_host);
  g_free (client_host);
  g_free (dest_host);
  g_free (topic);
  g_free (in_dim);
  g_free (connect_type);

  return 0;
}
