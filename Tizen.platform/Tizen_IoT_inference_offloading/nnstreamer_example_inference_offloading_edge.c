/**
 * @file	nnstreamer_example_inference_offloading_edge.c
 * @date	16 Mar 2023
 * @brief	Edge server that processes client requests.
 * @see	https://github.com/nnstreamer/nnstreamer
 * @author	 <gichan2.jang@samsung.com>
 * @bug		No known bugs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>
#include <nnstreamer/tensor_filter_custom_easy.h>
#include <getopt.h>

unsigned int received;
/**
 * @brief Data struct for options.
 */
typedef struct
{
  gchar *host;
  gchar *topic;
  gchar *dest_host;
  gchar *dest_port;
  gchar *connect_type;
  guint16 port;
  gchar *node_type;
} opt_data_s;

/**
 * @brief Function for custom-easy filter
 */
static int
ce_custom_inference_offloading (void *data, const GstTensorFilterProperties *prop,
    const GstTensorMemory *in, GstTensorMemory *out)
{
  unsigned int t;

  g_message ("[[DEBUG]] tensor filter is called.. num: %u", received++);

  for (t = 0; t < prop->output_meta.num_tensors; t++) {
    if (prop->input_meta.num_tensors <= t)
      memset (out[t].data, 0, out[t].size);
    else
      memcpy (out[t].data, in[t].data, MIN (in[t].size, out[t].size));
  }
  return 0;
}

/**
 * @brief Function for getting options
 */
static void
_get_option (int argc, char **argv, opt_data_s *opt_data)
{
  gint opt;
  struct option long_options[] = {
      { "host", required_argument,  NULL, 'h' },
      { "port", required_argument,  NULL, 'p' },
      { "topic", required_argument,  NULL, 't' },
      { "connecttype", required_argument,  NULL, 'c' },
      { "desthost", required_argument,  NULL, 'b' },
      { "destport", required_argument,  NULL, 'd' },
      { "nodetype", required_argument,  NULL, 'n' },
      { 0, 0, 0, 0}
  };
  gchar *optstring = "h:p:t:c:b:d:n:";

  opt_data->host = g_strdup ("localhost");
  opt_data->topic = g_strdup ("");
  opt_data->port = 5001;
  opt_data->dest_host = g_strdup ("");
  opt_data->dest_port = g_strdup ("");
  opt_data->connect_type = g_strdup ("TCP");
  opt_data->node_type = g_strdup ("QUERY");

  while ((opt = getopt_long (argc, argv, optstring, long_options, NULL)) != -1) {
    switch (opt) {
      case 'h':
        g_free (opt_data->host);
        opt_data->host = g_strdup (optarg);
        break;
      case 'p':
        opt_data->port = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 't':
        g_free (opt_data->topic);
        opt_data->topic = g_strdup_printf ("topic=%s", optarg);
        break;
      case 'c':
        g_free (opt_data->connect_type);
        opt_data->connect_type = g_strdup (optarg);
        break;
      case 'b':
        g_free (opt_data->dest_host);
        opt_data->dest_host = g_strdup_printf ("dest-host=%s", optarg);
        break;
      case 'd':
        g_free (opt_data->dest_port);
        opt_data->dest_port = g_strdup_printf ("dest-port=%s", optarg);
        break;
      case 'n':
        g_free (opt_data->node_type);
        opt_data->node_type = g_strdup (optarg);
        break;
      default:
        break;
    }
  }
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  gchar *str_pipeline;
  guint16 timeout = 10;
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  opt_data_s opt_data;

  /* setting tensor_filter custom-easy */
  const GstTensorsInfo info_in = {
    .num_tensors = 1U,
    .info = {{.name = NULL,.type = _NNS_UINT8,.dimension = {3, 224, 224, 1}}},
  };
  const GstTensorsInfo info_out = {
    .num_tensors = 1U,
    .info = {{.name = NULL,.type = _NNS_UINT8,.dimension = {10, 10, 1, 1}}},
  };

  /* init gstreamer */
  gst_init (&argc, &argv);

  _get_option (argc, argv, &opt_data);

  /* main loop */
  loop = g_main_loop_new (NULL, FALSE);

  /* init pipeline */
  if (g_ascii_strcasecmp (opt_data.node_type, "QUERY") == 0) {
    str_pipeline =
      g_strdup_printf
      ("tensor_query_serversrc host=%s port=%u %s %s connect-type=%s %s ! video/x-raw,width=224,height=224,format=RGB,framerate=0/1 ! "
      "tensor_converter ! tensor_filter framework=custom-easy model=inferecne_offloading ! "
      "tensor_query_serversink connect-type=%s async=false", opt_data.host, opt_data.port, opt_data.dest_host, opt_data.dest_port, opt_data.connect_type, opt_data.topic, opt_data.connect_type);
  } else if (g_ascii_strcasecmp (opt_data.node_type, "SUB") == 0) {
    str_pipeline =
      g_strdup_printf
      ("edgesrc host=%s port=%u %s %s connect-type=%s %s !  video/x-raw,width=224,height=224,format=RGB,framerate=0/1 ! "
      "tensor_converter ! tensor_filter framework=custom-easy model=inferecne_offloading ! tensor_sink name=sinkx",
          opt_data.host, opt_data.port, opt_data.dest_host, opt_data.dest_port, opt_data.connect_type, opt_data.topic);
  } else {
    g_critical ("Invalid application node_type. Please choose between QUERY and SUB.");
    return 0;
  }

  g_message ("[INFO] Pipeline str: %s", str_pipeline);

  NNS_custom_easy_register ("inferecne_offloading", ce_custom_inference_offloading, NULL, &info_in, &info_out);

  pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);

  /* start pipeline */
  received = 0;
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_message ("Start pipeline");

  g_usleep ((timeout + 1) * 1000 * 1000);

  gst_element_set_state (pipeline, GST_STATE_PAUSED);
  g_usleep (200 * 1000);

  gst_element_set_state (pipeline, GST_STATE_READY);
  g_usleep (200 * 1000);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_usleep (200 * 1000);

  g_message ("Stop pipeline");
  NNS_custom_easy_unregister ("inferecne_offloading");

  if (loop) {
    g_main_loop_unref (loop);
    loop = NULL;
  }

  if (pipeline) {
    gst_object_unref (pipeline);
    pipeline = NULL;
  }

  g_free (opt_data.host);
  g_free (opt_data.dest_host);
  g_free (opt_data.dest_port);
  g_free (opt_data.topic);
  g_free (opt_data.connect_type);
  g_free (opt_data.node_type);

  return 0;
}
