/**
 * @file	pubsub_performance_benchmark.c
 * @date	06 Oct 2021
 * @brief	edgeAI performance benchmark - pub/sub (ZeroMQ, MQTT)
 * @author	Gichan Jang <gichan2.jnag@samsung.com>
 * @bug		No known bugs.
 */
/**
 * @note The ZeroMQ element used from `https://github.com/mjhowell/gst-zeromq`
 */

#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>
#include <getopt.h>

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

guint received;

/**
 * @brief Print usage info
 */
static void
_usage (void)
{
  g_message ("\nusage: \n"
  "    --zmq    Use ZeroMQ. \n"
  "    --mqtt   Use MQTT. (default) \n"
  "    --pub    Run publisher pipeline. (default) \n"
  "    --sub    Run subscriber pipeline. \n"
  "    --topic Set topic. \n"
  "    --host   Set host address. \n"
  "    --port   Set port. \n"
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
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  gchar *str_pipeline = NULL, *in_dim = NULL;
  GstElement *pipeline, *element;
  gboolean is_pub = TRUE;
  gchar *host = g_strdup ("127.0.0.1"), *topic = NULL, *connect_type = g_strdup ("TCP"), *dest_host = g_strdup ("");
  guint16 port = 1883, repeat = 1, timeout = 10, width = 640, height = 480, dest_port=1883;
  gint opt;
  struct option long_options[] = {
      { "pub", no_argument, NULL, 'p' },
      { "sub", no_argument, NULL, 's' },
      { "topic", required_argument, NULL, 'o' },
      { "host", required_argument,  NULL, 'u' },
      { "port", required_argument,  NULL, 'b' },
      { "desthost", required_argument,  NULL, 'm' },
      { "destport", required_argument,  NULL, 'd' },
      { "repeat", required_argument,  NULL, 'r' },
      { "timeout", required_argument,  NULL, 't' },
      { "help", required_argument,  NULL, 'h' },
      { "width", required_argument,  NULL, 'w' },
      { "height", required_argument,  NULL, 'a' },
      { "connecttype", required_argument,  NULL, 'c' },
      { 0, 0, 0, 0}
  };
  gchar *optstring = "z:m:p:s:o:u:b:r:t:h:w:a";

  /* init gstreamer */
  gst_init (&argc, &argv);

  topic = g_strdup ("pubsub/benchmark");

  while ((opt = getopt_long (argc, argv, optstring, long_options, NULL)) != -1) {
    switch (opt) {
      case 'p':
        is_pub = TRUE;
        break;
      case 's':
        is_pub = FALSE;
        break;
      case 'o':
        topic = g_strdup (optarg);
        break;
      case 'u':
        g_free (host);
        host = g_strdup (optarg);
        break;
      case 'b':
        port = (guint16) g_ascii_strtoll (optarg, NULL, 10);
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
      case 'c':
        g_free (connect_type);
        connect_type = g_strdup (optarg);
        break;
      case 'm':
        g_free (dest_host);
        dest_host = g_strdup (optarg);
        break;
      case 'd':
        dest_port = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      default:
        _usage ();
        return 0;
    }
  }

  g_print ("host: %s, port: %u\n", host, port);
  g_print ("topic: %s, repeat: %u \n\n", topic, repeat);

  /* Create pipeline */
  if (0 == g_strcmp0 (connect_type, "ZMQ")) {
    gchar *endpoint = g_strdup ("");
    if (0 != g_ascii_strcasecmp (host, "")) {
      g_free (endpoint);
      endpoint = g_strdup_printf ("endpoint=tcp://%s:5556", host);
    }
    if (is_pub) {
      str_pipeline =
          g_strdup_printf
          ("videotestsrc is-live=true ! videoconvert ! videoscale ! video/x-raw,width=%u,height=%u,format=RGB,framerate=60/1 ! tensor_converter ! zmqsink %s", width, height, endpoint);
    } else {
      str_pipeline =
          g_strdup_printf
          ("zmqsrc %s ! tensor_sink name=sinkx sync=true", endpoint);
    }
    g_free (endpoint);
  } else if (0 == g_strcmp0 (connect_type, "MQTT")) {
    gchar *mqtthost = g_strdup ("");
    if (0 != g_ascii_strcasecmp (host, "")) {
      g_free (mqtthost);
      mqtthost = g_strdup_printf ("host=%s", host);
    }
    if (is_pub) {
      str_pipeline =
          g_strdup_printf
          ("videotestsrc is-live=true ! videoconvert ! videoscale ! video/x-raw,width=%u,height=%u,format=RGB,framerate=60/1 ! tensor_converter ! mqttsink pub-topic=%s %s sync=true", width, height, topic, mqtthost);
    } else {
      str_pipeline =
           g_strdup_printf
           ("mqttsrc sub-topic=%s %s ! tensor_sink name=sinkx sync=true", topic, mqtthost);
    }
    g_free (mqtthost);
  } else { /** For edgesrc/sink */
    if (is_pub) {
      str_pipeline =
          g_strdup_printf
          ("videotestsrc is-live=true ! videoconvert ! videoscale ! video/x-raw,width=%u,height=%u,format=RGB,framerate=60/1 ! tensor_converter ! edgesink host=%s port=0  dest-host=%s dest-port=%u connect-type=%s topic=%s sync=true", width, height, host, dest_host, dest_port, connect_type, topic);
    } else {
      str_pipeline =
           g_strdup_printf
           ("edgesrc host=%s port=0 dest-host=%s dest-port=%u connect-type=%s topic=%s ! tensor_sink name=sinkx sync=true", host, dest_host, dest_port, connect_type, topic);
    }
  }

  g_print ("%s\n", str_pipeline);
  pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);

  element = gst_bin_get_by_name (GST_BIN (pipeline), "sinkx");
  g_signal_connect (element, "new-data", (GCallback) _new_data_cb, NULL);
  gst_object_unref (element);
  received = 0;

  /* start pipeline */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_usleep (timeout * 1000 * 1000);

  g_message ("Received data cnt: %u", received);

  gst_element_set_state (pipeline, GST_STATE_PAUSED);
  g_usleep (200 * 1000);

  gst_element_set_state (pipeline, GST_STATE_READY);
  g_usleep (200 * 1000);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_usleep (200 * 1000);

  gst_object_unref (pipeline);
  pipeline = NULL;
  g_free (host);
  g_free (dest_host);
  g_free (connect_type);
  g_free (topic);
  g_free (in_dim);

  return 0;
}
