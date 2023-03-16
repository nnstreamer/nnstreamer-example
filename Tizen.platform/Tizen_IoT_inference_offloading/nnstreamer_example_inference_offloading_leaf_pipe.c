/**
 * @file	nnstreamer_example_inference_offloading_leaf_pipe.c
 * @date	16 Mar 2023
 * @brief	Leaf program requesting inference to edge server.
 * @see	https://github.com/nnstreamer/nnstreamer
 * @author	 <gichan2.jang@samsung.com>
 * @bug		No known bugs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>
#include <getopt.h>

guint received; /**< received buffer count */

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
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  gchar *str_pipeline;
  guint16 timeout = 5;
  GstElement *element;
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  gint opt;
  gchar *topic, *host, *client_host;
  guint port = 5001;
  struct option long_options[] = {
      { "host", required_argument,  NULL, 'h' },
      { "port", required_argument,  NULL, 'p' },
      { "client_host", required_argument,  NULL, 'c' },
      { 0, 0, 0, 0}
  };
  gchar *optstring = "t:h:p:c:";

  /* init gstreamer */
  gst_init (&argc, &argv);

  host = g_strdup ("localhost");
  client_host = strdup ("localhost");
  topic = g_strdup ("");

  while ((opt = getopt_long (argc, argv, optstring, long_options, NULL)) != -1) {
    switch (opt) {
      case 'h':
        g_free (host);
        host = g_strdup (optarg);
        break;
      case 'p':
        port = (guint) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 'c':
        g_free (client_host);
        client_host = g_strdup (optarg);
        break;
      default:
        return 0;
    }
  }

  /* main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_message ("Host: %s, port: %u", host, port);
  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("videotestsrc is-live=true ! videoconvert ! videoscale ! video/x-raw,width=224,height=224,format=RGB,framerate=10/1 ! "
      "tensor_query_client host=%s port=0 dest-host=%s dest-port=%u ! tensor_sink name=sinkx sync=true", client_host, host, port);


  pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);

  element = gst_bin_get_by_name (GST_BIN (pipeline), "sinkx");
  g_signal_connect (element, "new-data", (GCallback) _new_data_cb, NULL);
  gst_object_unref (element);

  /* start pipeline */
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

  if (loop) {
    g_main_loop_unref (loop);
    loop = NULL;
  }

  if (pipeline) {
    gst_object_unref (pipeline);
    pipeline = NULL;
  }  return 0;

  g_free (topic);
  g_free (host);
  g_free (client_host);
}
