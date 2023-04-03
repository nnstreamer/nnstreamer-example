/**
 * @file	nnstreamer_example_inference_offloading_leaf_rt.c
 * @date	16 Mar 2023
 * @brief	Leaf program requesting inference to edge server.
 * @see	https://github.com/nnstreamer/nnstreamer
 * @author	 <gichan2.jang@samsung.com>
 * @bug		No known bugs.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include "nnstreamer-edge.h"
#include <getopt.h>
#include <string.h>


unsigned int received;

/**
 * @brief Data struct for options.
 */
typedef struct
{
  char *host;
  unsigned int port;
  char *topic;
  char *dest_host;
  unsigned int dest_port;
  nns_edge_connect_type_e conn_type;
  nns_edge_node_type_e node_type;
} opt_data_s;

/**
 * @brief Edge event callback.
 */
static int
_query_client_event_cb (nns_edge_event_h event_h, void *user_data)
{
  nns_edge_event_e event = NNS_EDGE_EVENT_UNKNOWN;
  nns_edge_data_h data_h;
  void *data;
  nns_size_t data_len;
  unsigned int count;
  int ret;

  ret = nns_edge_event_get_type (event_h, &event);
  if (NNS_EDGE_ERROR_NONE != ret)
    return ret;

  switch (event) {
    case NNS_EDGE_EVENT_NEW_DATA_RECEIVED:
      received++;

      nns_edge_event_parse_new_data (event_h, &data_h);

      nns_edge_data_get_count (data_h, &count);
      nns_edge_data_get (data_h, 0, &data, &data_len);
      nns_edge_data_destroy (data_h);
      printf ("[DEBUG] New data received. the number of mem: %u, size: %u \n",
          count, (unsigned int) data_len);
      break;
    default:
      break;
  }

  return NNS_EDGE_ERROR_NONE;
}


/**
 * @brief Get nnstreamer-edge connection type
 */
static nns_edge_connect_type_e
_get_conn_type (char *arg)
{
  nns_edge_connect_type_e conn_type;

  if (strcmp (arg, "TCP") == 0)
    conn_type = NNS_EDGE_CONNECT_TYPE_TCP;
  else if (strcmp (arg, "HYBRID") == 0)
    conn_type = NNS_EDGE_CONNECT_TYPE_HYBRID;
  else if (strcmp (arg, "MQTT") == 0)
    conn_type = NNS_EDGE_CONNECT_TYPE_MQTT;
  else if (strcmp (arg, "AITT") == 0)
    conn_type = NNS_EDGE_CONNECT_TYPE_AITT;
  else
    conn_type = NNS_EDGE_CONNECT_TYPE_UNKNOWN;

  return conn_type;
}

/**
 * @brief Get nnstreamer-edge node type
 */
static nns_edge_node_type_e
_get_node_type (char *arg)
{
  nns_edge_node_type_e node_type;

  if (strcmp (arg, "QUERY") == 0)
    node_type = NNS_EDGE_NODE_TYPE_QUERY_CLIENT;
  else if (strcmp (arg, "PUB") == 0)
    node_type = NNS_EDGE_NODE_TYPE_PUB;
  else
    node_type = NNS_EDGE_NODE_TYPE_UNKNOWN;

  return node_type;
}

/**
 * @brief Function for getting options
 */
static void
_get_option (int argc, char **argv, opt_data_s *opt_data)
{
  int opt;
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
  char *optstring = "h:p:t:c:n:b:d:";

  opt_data->host = strdup ("localhost");
  opt_data->port = 0;
  opt_data->topic = NULL;
  opt_data->dest_host = strdup ("localhost");
  opt_data->dest_port = 5001;
  opt_data->conn_type = NNS_EDGE_CONNECT_TYPE_UNKNOWN;
  opt_data->node_type = NNS_EDGE_NODE_TYPE_UNKNOWN;

  while ((opt = getopt_long (argc, argv, optstring, long_options, NULL)) != -1) {
    switch (opt) {
      case 'h':
        free (opt_data->host);
        opt_data->host = strdup (optarg);
        break;
      case 'p':
        opt_data->port = (uint) strtoll (optarg, NULL, 10);
        break;
      case 't':
        opt_data->topic = strdup (optarg);
        break;
      case 'c':
        opt_data->conn_type = _get_conn_type (optarg);
        break;
      case 'b':
      free (opt_data->dest_host);
        opt_data->dest_host = strdup (optarg);
        break;
      case 'd':
        opt_data->dest_port = (uint) strtoll (optarg, NULL, 10);
        break;
      case 'n':
        opt_data->node_type = _get_node_type (optarg);
        break;
      default:
        break;
    }
  }
}

/**
 * @brief Prepare edge data to send
 */
static int
_prepare_edge_data (nns_edge_data_h *data_h)
{
  nns_size_t data_len;
  void *data = NULL;
  int ret = NNS_EDGE_ERROR_NONE;

  data_len = 3 * 224 * 224 * sizeof (char);
  data = malloc (data_len);
  if (!data) {
    printf ("Failed to allocate camera data.\n");
    return NNS_EDGE_ERROR_OUT_OF_MEMORY;
  }

  ret = nns_edge_data_create (data_h);
  if (NNS_EDGE_ERROR_NONE != ret) {
    printf ("Failed to create an edge data.\n");
    free (data);
    return ret;
  }

  ret = nns_edge_data_add (*data_h, data, data_len, free);
  if (NNS_EDGE_ERROR_NONE != ret) {
    printf ("Failed to add camera data to the edge data.\n");
    free (data);
  }

  return ret;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  nns_edge_h client_h;
  nns_edge_data_h data_h;
  int ret = NNS_EDGE_ERROR_NONE;
  unsigned int i, retry = 0;
  opt_data_s opt_data;

  printf ("============== Start app.. ============== \n\n");

  _get_option (argc, argv, &opt_data);

  printf ("[INFO] desthost: %s, destport: %u, topic: %s \n",
      opt_data.dest_host, opt_data.dest_port, opt_data.topic);

  received = 0;
  ret = nns_edge_create_handle ("TEMP_ID", opt_data.conn_type,
      opt_data.node_type, &client_h);
  nns_edge_set_event_callback (client_h, _query_client_event_cb, NULL);

  nns_edge_set_info (client_h, "HOST", opt_data.host);
  if (opt_data.topic)
    nns_edge_set_info (client_h, "TOPIC", opt_data.topic);

  if (NNS_EDGE_NODE_TYPE_PUB == opt_data.node_type) {
    if (NNS_EDGE_CONNECT_TYPE_TCP == opt_data.conn_type) {
      char port[6];
      sprintf (port, "%u", opt_data.port);
      nns_edge_set_info (client_h, "PORT", port);
    } else {
      char port[6];
      sprintf (port, "%u", opt_data.dest_port);
      nns_edge_set_info (client_h, "DEST_HOST", opt_data.dest_host);
      nns_edge_set_info (client_h, "DEST_PORT", port);
    }
  }
  ret = nns_edge_start (client_h);
  if (NNS_EDGE_ERROR_NONE != ret) {
    printf ("Failed to start query client.\n");
    goto done;
  }

  sleep (1);

  if (NNS_EDGE_NODE_TYPE_QUERY_CLIENT == opt_data.node_type) {
    ret = nns_edge_connect (client_h, opt_data.dest_host, opt_data.dest_port);
    if (NNS_EDGE_ERROR_NONE != ret) {
      printf ("Failed to connect to query server.\n");
      goto done;
    }
  }

  sleep (1);

  ret = _prepare_edge_data (&data_h);
  if (NNS_EDGE_ERROR_NONE != ret) {
    printf ("Failed to prepare to nns edge data.\n");
    goto done;
  }

  received = 0;
  for (i = 0; i < 50U; i++) {
    nns_edge_send (client_h, data_h);
    usleep (100000);
  }

  nns_edge_data_destroy (data_h);

  /* Wait for responding data (20 seconds) */
  do {
    usleep (100000);
    if (received > 0)
      break;
  } while (NNS_EDGE_NODE_TYPE_QUERY_CLIENT == opt_data.node_type &&
        retry++ < 100U);

  printf ("[DEBUG] Total received the number of data: %u\n", received);

done:
  nns_edge_release_handle (client_h);
  nns_edge_data_destroy (data_h);
  free (opt_data.dest_host);
  free (opt_data.host);
  if (opt_data.topic)
    free (opt_data.topic);

  return ret;
}
