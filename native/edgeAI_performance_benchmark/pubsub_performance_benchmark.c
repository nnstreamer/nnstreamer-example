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

static GMainLoop *loop; /**< main event loop */
guint received;

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
  gboolean is_pub = TRUE, is_zmq = FALSE;
  GstElement *pipeline, *element;
  gchar *host = g_strdup (""), *topic = NULL;
  guint16 port = 1883, repeat = 1, timeout = 10, width = 640, height = 480;
  gint opt;
  struct option long_options[] = {
      { "zmq", no_argument, NULL, 'z' },
      { "mqtt", no_argument, NULL, 'm' },
      { "pub", no_argument, NULL, 'p' },
      { "sub", no_argument, NULL, 's' },
      { "topic", required_argument, NULL, 'o' },
      { "host", required_argument,  NULL, 'u' },
      { "port", required_argument,  NULL, 'b' },
      { "repeat", required_argument,  NULL, 'r' },
      { "timeout", required_argument,  NULL, 't' },
      { "help", required_argument,  NULL, 'h' },
      { "width", required_argument,  NULL, 'w' },
      { "height", required_argument,  NULL, 'a' },
      { 0, 0, 0, 0}
  };
  gchar *optstring = "z:m:p:s:o:u:b:r:t:h:w:a";

  /* init gstreamer */
  gst_init (&argc, &argv);

  topic = g_strdup ("pubsub/benchmark");

  while ((opt = getopt_long (argc, argv, optstring, long_options, NULL)) != -1) {
    switch (opt) {
      case 'z':
        is_zmq = TRUE;
        break;
      case 'm':
        is_zmq = FALSE;
        break;
      case 'p':
        is_pub = TRUE;
        break;
      case 's':
        is_pub = FALSE;
        break;
      case 'o':
        topic = g_strdup_printf ("topic=%s", optarg);
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
      default:
        _usage ();
        return 0;
    }
  }

  g_print ("host: %s, port: %u\n", host, port);
  g_print ("topic: %s, repeat: %u \n\n", topic, repeat);

  /* Create main loop and pipeline */
  loop = g_main_loop_new (NULL, FALSE);
  if (is_zmq) {
    gchar *endpoint = g_strdup ("");
    if (0 != g_ascii_strcasecmp (host, "")) {
      g_free (endpoint);
      endpoint = g_strdup_printf ("endpoint=tcp://%s:5556", host);
    }
    if (is_pub) {
      str_pipeline =
          g_strdup_printf
          ("videotestsrc ! videoconvert ! videoscale ! video/x-raw,width=%u,height=%u,format=RGB,framerate=60/1 ! tensor_converter ! zmqsink %s", width, height, endpoint);
    } else {
      str_pipeline =
          g_strdup_printf
          ("zmqsrc %s ! tensor_sink name=sinkx sync=true", endpoint);
    }
    g_free (endpoint);
  } else {
    gchar *mqtthost = g_strdup ("");
    if (0 != g_ascii_strcasecmp (host, "")) {
      g_free (mqtthost);
      mqtthost = g_strdup_printf ("host=%s", host);
    }
    if (is_pub) {
      str_pipeline =
          g_strdup_printf
          ("videotestsrc ! videoconvert ! videoscale ! video/x-raw,width=%u,height=%u,format=RGB,framerate=60/1 ! tensor_converter ! mqttsink pub-topic=%s %s sync=true", width, height, topic, mqtthost);
    } else {
      str_pipeline =
           g_strdup_printf
           ("mqttsrc sub-topic=%s %s ! tensor_sink name=sinkx sync=true", topic, mqtthost);
    }
    g_free (mqtthost);
  }

  g_print ("%s\n", str_pipeline);
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
  g_free (host);
  g_free (topic);
  g_free (in_dim);

  return 0;
}
