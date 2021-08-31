/**
 * @file	example_query_object_detection.c
 * @date	31 Aug 2021
 * @brief	tensor query object detection example using nnstreamer C-API
 * @author	Gichan Jang <gichan2.jnag@samsung.com>
 * @bug		No known bugs.
 */

#include <unistd.h>
#include <glib.h>
#include <nnstreamer.h>

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  gchar *str_pipeline = NULL;
  gboolean is_server = FALSE;
  ml_pipeline_h pipe = NULL;
  gchar *src_host, *sink_host;
  guint16 src_port = 3001, sink_port = 3000;

  if (argc != 2 && argc != 6) {
    g_print ("Please specify either the server or the client.\n");
    g_print ("e.g) $./nnstreamer_example_query_object_detection {server, client} ...\n\n");
    g_print ("Optional) If you want to give an address option:\n");
    g_print ("$./nnstreamer_example_query_object_detection server 'src-host', 'src-port', 'sink-host' and 'sink-port'\n");
    g_print ("$./nnstreamer_example_query_object_detection client 'src-host', 'src-port', 'sink-host' and 'sink-port'\n");
    return 0;
  }

  if (g_strcmp0 ("server", argv[1]) == 0) {
    is_server = TRUE;
  } else if (g_strcmp0 ("client", argv[1]) == 0) {
    is_server = FALSE;
  } else {
    g_print ("Failed to parse your input. Expected:  {server, client}, Your input: %s \n", argv[1]);
    return 0;
  }

  if (argc == 2) {
    g_print ("Run pipeline as default option.\n");
    src_host = g_strdup ("127.0.0.1");
    sink_host = g_strdup ("127.0.0.1");
  } else {
    g_print ("Run pipeline as given option.%s %s\n",argv[2], argv[3]);
    src_host = g_strdup (argv[2]);
    src_port = (guint16) g_ascii_strtoll (argv[3], NULL, 10);
    sink_host = g_strdup (argv[4]);
    sink_port = (guint16) g_ascii_strtoll (argv[5], NULL, 10);
  }
  g_print ("src host: %s, src port: %u, sink host: %s, sink port: %u\n", src_host, src_port, sink_host, sink_port);

  /* init pipeline */
  if (is_server) {
    str_pipeline =
        g_strdup_printf
        ("tensor_query_serversrc host=%s port=%u ! tensor_filter framework=tensorflow-lite model=./tflite_model/ssd_mobilenet_v2_coco.tflite ! "
          "tensor_decoder mode=bounding_boxes option1=mobilenet-ssd option2=./tflite_model/coco_labels_list.txt option3=./tflite_model/box_priors.txt option4=640:480 option5=300:300 ! "
          "tensor_converter ! other/tensors,num_tensors=1,dimensions=4:640:480:1,types=uint8 ! tensor_query_serversink host=%s port=%u sync=false async=false",
          src_host, src_port, sink_host, sink_port);
  } else {
    str_pipeline =
        g_strdup_printf
        ("compositor name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! ximagesink "
          "videotestsrc ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=10/1 ! tee name=t "
            "t. ! queue ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter ! "
            "tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! queue leaky=2 max-size-buffers=2 ! "
            "tensor_query_client src-host=%s src-port=%u sink-host=%s sink-port=%u ! "
            "tensor_decoder mode=direct_video ! videoconvert ! video/x-raw,width=640,height=480,format=RGBA ! mix.sink_0 "
            "t. ! queue ! mix.sink_1", src_host, src_port, sink_host, sink_port);
  }
  g_print ("%s\n", str_pipeline);
  ml_pipeline_construct (str_pipeline, NULL, NULL, &pipe);

  ml_pipeline_start (pipe);

  sleep (10);

  ml_pipeline_stop (pipe);
  ml_pipeline_destroy (pipe);
  g_free (src_host);
  g_free (sink_host);
  g_free (str_pipeline);

  return 0;
}
