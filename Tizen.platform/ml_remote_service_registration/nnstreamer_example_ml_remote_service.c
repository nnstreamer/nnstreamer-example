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
#include <ml-api-service.h>
#include <nnstreamer-tizen-internal.h>
#include <getopt.h>

#define MAX_SENTENCE_LENGTH 256

/**
 * @brief Data struct for options.
 */
typedef struct
{
  gchar *host;
  gchar *topic;
  gchar *dest_host;
  gchar *connect_type;
  guint port;
  guint dest_port;
  gchar *node_type;
  gchar *service_type;
  gchar *service_key;
} opt_data_s;


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
      { "servicetype", required_argument,  NULL, 's' },
      { "servicekey", required_argument,  NULL, 'k' },
      { 0, 0, 0, 0}
  };
  gchar *optstring = "h:p:t:c:b:d:n:s:k:";

  opt_data->host = g_strdup ("localhost");
  opt_data->topic = g_strdup ("");
  opt_data->port = 0;
  opt_data->dest_host = g_strdup ("");
  opt_data->dest_port = 1883;
  opt_data->connect_type = g_strdup ("TCP");
  opt_data->node_type = g_strdup ("remote_sender");
  opt_data->service_type = g_strdup ("pipeline_raw");
  opt_data->service_key = g_strdup ("temp_key");

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
        opt_data->topic = g_strdup (optarg);
        break;
      case 'c':
        g_free (opt_data->connect_type);
        opt_data->connect_type = g_strdup (optarg);
        break;
      case 'b':
        g_free (opt_data->dest_host);
        opt_data->dest_host = g_strdup (optarg);
        break;
      case 'd':
        opt_data->dest_port = (guint16) g_ascii_strtoll (optarg, NULL, 10);
        break;
      case 'n':
        g_free (opt_data->node_type);
        opt_data->node_type = g_strdup (optarg);
        break;
      case 's':
        g_free (opt_data->service_type);
        opt_data->service_type = g_strdup (optarg);
        break;
      case 'k':
        g_free (opt_data->service_key);
        opt_data->service_key = g_strdup (optarg);
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
  opt_data_s opt_data;
  ml_service_h service_h = NULL;
  ml_option_h option_h = NULL;
  ml_option_h service_option_h = NULL;
  gboolean is_sender = FALSE;

  g_print ("***** Start remote registration sample app. *****\n");

  _get_option (argc, argv, &opt_data);

  ml_option_create (&option_h);
  ml_option_set (option_h, "connect-type", opt_data.connect_type, NULL);
  ml_option_set (option_h, "topic", opt_data.topic, NULL);
  ml_option_set (option_h, "host", opt_data.host, NULL);
  ml_option_set (option_h, "dest-host", opt_data.dest_host, NULL);
  ml_option_set (option_h, "port", &opt_data.port, NULL);
  ml_option_set (option_h, "dest-port", &opt_data.dest_port, NULL);
  ml_option_set (option_h, "node-type", opt_data.node_type, NULL);
  ml_remote_service_create (option_h, &service_h);
  if (g_strcmp0 (opt_data.node_type, "remote_sender") == 0)
    is_sender = TRUE;

  if (is_sender) {
    ml_option_create (&service_option_h);
    ml_option_set (service_option_h, "service-type", opt_data.service_type, NULL);
    ml_option_set (service_option_h, "service-key", opt_data.service_key, NULL);
    g_message ("service type:key, %s:%s", opt_data.service_type, opt_data.service_key);
  }

  while (1) {
    /* Get user input */
    gchar sentence[MAX_SENTENCE_LENGTH] = { 0, };
    gchar *pipeline_desc = NULL;
    gint ret = ML_ERROR_NONE;

    g_print ("\n==================================================\n");
    if (is_sender) {
      g_print ("Please enter the pipeline description: ");
    } else {
      g_print ("Enter the service key to get the pipeline description: ");
    }

    if (fgets (sentence, sizeof (sentence), stdin)) {
      sentence[strcspn(sentence, "\n")] = '\0';
      g_print ("User input : %s \n", sentence);
      if (g_strcmp0 (sentence, "exit") == 0) {
        g_print ("Exiting...\n");
        break;
      }
    }

    if (is_sender) {
      pipeline_desc = g_strdup (sentence);
      ret = ml_remote_service_register (service_h, service_option_h,
          pipeline_desc, strlen (pipeline_desc) + 1);
      if (ML_ERROR_NONE != ret) {
        g_warning("Failed to register pipeline description. Check the connection with the server.");
      } else {
        g_print ("\n ***** Pipeline description is registered. *****\n");
      }
    } else {
      ret = ml_service_get_pipeline (sentence, &pipeline_desc);
      if (ML_ERROR_NONE != ret) {
        g_warning("Failed to get pipeline description. Check the service key, given service key: %s.", sentence);
      } else {
        g_print ("Received pipeline description: %s \n",  pipeline_desc);
      }
    }
    g_free (pipeline_desc);
   }

  g_free (opt_data.host);
  g_free (opt_data.dest_host);
  g_free (opt_data.topic);
  g_free (opt_data.connect_type);
  g_free (opt_data.node_type);
  g_free (opt_data.service_type);
  g_free (opt_data.service_key);
  ml_option_destroy (option_h);
  ml_option_destroy (service_option_h);
  ml_service_destroy (service_h);

  return 0;
}
