/**
 * @file	nnstreamer_example_training_offloading.c
 * @date	20 July 2023
 * @brief	training offloading example
 * @author	Hyunil Park <hyunil46.park@samsung.com>
 * @bug		No known bugs.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gst/gst.h>
#include <string.h>

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
 * @brief Macro to check error case.
 */
#define _check_cond_err(cond) \
  do { \
    if (!(cond)) { \
      _print_log ("app failed! [line : %d]", __LINE__); \
      goto error; \
    } \
  } while (0)

/**
 * @brief Data structure for app.
 */
typedef struct
{
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;

/**
 * @brief Free resources in app data.
 */
static void
_free_app_data (void)
{
  if (g_app.loop) {
    g_main_loop_unref (g_app.loop);
    g_app.loop = NULL;
  }

  if (g_app.bus) {
    gst_object_unref (g_app.bus);
    g_app.bus = NULL;
  }

  if (g_app.pipeline) {
    gst_object_unref (g_app.pipeline);
    g_app.pipeline = NULL;
  }
}

/**
 * @brief Bus callback for message.
 */
static gboolean
bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      _print_log ("received eos message");
      g_main_loop_quit ((GMainLoop *) data);
      break;
    case GST_MESSAGE_ERROR:
      _print_log ("received error message");
      g_main_loop_quit ((GMainLoop *) data);
      break;
    default:
      break;
  }

  return TRUE;
}

static const gchar *stream_role = NULL;
static const gchar *filename = NULL;
static const gchar *json = NULL;
static const gchar *framework = NULL;
static const gchar *model_config = NULL;
static const gchar *model_save_path = NULL;
static const gchar *capsfilter = NULL;
static const gchar *dest_host = NULL;
static gint dest_port = -1;
static gint epochs = 1;
static gint start_sample_index = 0;
static gint stop_sample_index = 999;
static gint num_training_sample = 500;
static gint num_validation_sample = 500;
static gint num_inputs = 1;
static gint num_labels = 1;

static GOptionEntry entries[] = {
  {"stream-role", 0, 0, G_OPTION_ARG_STRING, &stream_role,
      "stream role between peers"},
  {"filename", 0, 0, G_OPTION_ARG_STRING, &filename,
      "filename to be read by datareposrc"},
  {"json", 0, 0, G_OPTION_ARG_STRING, &json,
      "stream meta info to be read by datareposrc"},
  {"framework", 0, 0, G_OPTION_ARG_STRING, &framework,
      "Neural network framework to be used for model training"},
  {"model-config", 0, 0, G_OPTION_ARG_STRING, &model_config,
      "model configuration file is used to configure the model"},
  {"model-save-path", 0, 0, G_OPTION_ARG_STRING, &model_save_path,
      "Path to save the trained model"},
  {"input-caps", 0, 0, G_OPTION_ARG_STRING, &capsfilter,
      "input caps of tensor_trainer"},
  {"dest-host", 0, 0, G_OPTION_ARG_STRING, &dest_host,
      "destination host of MQTT"},
  {"dest-port", 0, 0, G_OPTION_ARG_INT, &dest_port,
      "destination port of MQTT"},
  {"epochs", 0, 0, G_OPTION_ARG_INT, &epochs, "Repetition of range of samples"},
  {"start-sample-index", 0, 0, G_OPTION_ARG_INT, &start_sample_index,
      "Set start index of range of samples"},
  {"stop-sample-index", 0, 0, G_OPTION_ARG_INT, &stop_sample_index,
      "Set stop index of range of samples"},
  {"num-training-sample", 0, 0, G_OPTION_ARG_INT, &num_training_sample,
      "set how many samples are taken for training model"},
  {"num-validation-sample", 0, 0, G_OPTION_ARG_INT, &num_validation_sample,
      "set how many samples are taken validation model"},
  {"num-inputs", 0, 0, G_OPTION_ARG_INT, &num_inputs,
      "set how many inputs are received"},
  {"num-labels", 0, 0, G_OPTION_ARG_INT, &num_labels,
      "set how many labels are received"},
  {NULL}
};

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  GOptionContext *context;
  GError *err = NULL;

  context = g_option_context_new (" - training offloading option");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gst_init_get_option_group ());
  if (!g_option_context_parse (context, &argc, &argv, &err)) {
    g_printerr ("Error initializing: %s\n", err->message);
    g_error_free (err);
    goto error;
  }
  gchar *str_pipeline = NULL;

  _print_log ("start app..");

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  _check_cond_err (stream_role != NULL);
  if (!g_strcmp0 (stream_role, "sender")) {
    _check_cond_err (filename != NULL);
    _check_cond_err (json != NULL);
    _check_cond_err (dest_host != NULL);
    _check_cond_err (dest_port != -1);
    str_pipeline =
        g_strdup_printf
        ("datareposrc location=%s json=%s epochs=%d start-sample-index=%d stop-sample-index=%d ! "
        "edgesink port=0 connect-type=HYBRID topic=tempTopic dest-host=%s dest-port=%d",
        filename, json, epochs, start_sample_index, stop_sample_index,
        dest_host, dest_port);
  } else if (!g_strcmp0 (stream_role, "receiver")) {
    _check_cond_err (dest_host != NULL);
    _check_cond_err (dest_port != -1);
    _check_cond_err (dest_port != -1);
    _check_cond_err (capsfilter != NULL);
    _check_cond_err (model_config != NULL);
    _check_cond_err (model_save_path != NULL);
    str_pipeline =
        g_strdup_printf
        ("edgesrc dest-host=%s dest-port=%d connect-type=HYBRID topic=tempTopic port=0 ! queue ! %s ! "
        "tensor_trainer framework=%s model-config=%s model-save-path=%s num-inputs=%d num-labels=%d "
        "num-training-samples=%d num-validation-samples=%d epochs=%d ! tensor_sink",
        dest_host, dest_port, capsfilter, framework, model_config,
        model_save_path, num_inputs, num_labels, num_training_sample,
        num_validation_sample, epochs);
  } else {
    g_critical ("Invaild stream role");
    goto error;
  }

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);
  gst_bus_add_watch (g_app.bus, bus_callback, g_app.loop);
  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  /* run main loop */
  g_main_loop_run (g_app.loop);

  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

error:
  _print_log ("close app..");
  g_critical
      ("command example for sender: ./nnstreamer_example_training_offloading --stream-role=sender "
      "--filename=mnist.data --json=mnist.json --epochs=1 --start-sample-index=0 --stop-sample-index=999 "
      "--dest-host=127.0.0.1 --dest-port=1883\n");
  g_critical
      ("command example for receiver: ./nnstreamer_example_training_offloading --stream-role=receiver "
      "--dest-host=127.0.0.1 --dest-port=1883 "
      "--framework=nntrainer --model-config=mnist.ini --model-save-path=model.bin "
      "--num-training-sample=500 --num-validation-sample=500 --epochs=1 --num-inputs=1 --num-labels=1 "
      "--input-caps=other/tensors,format=static,num_tensors=2,framerate=0/1,dimensions=1:1:784:1.1:1:10:1,types=float32.float32");
  g_option_context_free (context);
  _free_app_data ();

  return 0;
}
