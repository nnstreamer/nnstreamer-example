/**
 * @file	example_early_exit.c
 * @date	10 Dec 2020
 * @brief	Early exit network example with multiple module
 * @author	Gichan Jang <gichan2.jnag@samsung.com>
 * @bug		No known bugs.
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/app.h>
#include <nnstreamer/tensor_filter_custom_easy.h>

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
    gst_bus_remove_signal_watch (g_app.bus);
    gst_object_unref (g_app.bus);
    g_app.bus = NULL;
  }

  if (g_app.pipeline) {
    gst_object_unref (g_app.pipeline);
    g_app.pipeline = NULL;
  }
}

/**
 * @brief Function to print error message.
 */
static void
_parse_err_message (GstMessage * message)
{
  gchar *debug;
  GError *error;

  g_return_if_fail (message != NULL);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &error, &debug);
      break;

    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (message, &error, &debug);
      break;

    default:
      return;
  }

  gst_object_default_error (GST_MESSAGE_SRC (message), error, debug);
  g_error_free (error);
  g_free (debug);
}

/**
 * @brief Callback for message.
 */
static void
_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      _print_log ("received eos message");
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_ERROR:
      _print_log ("received error message");
      _parse_err_message (message);
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_WARNING:
      _print_log ("received warning message");
      _parse_err_message (message);
      break;

    case GST_MESSAGE_STREAM_START:
      _print_log ("received start message");
      break;

    default:
      break;
  }
}

/**
 * @brief Get early exit module description
 */
static gchar *
_get_early_exit_desc (const int idx, const gchar * main_model,
    const gchar * check_model)
{
  /* `extra_str` is added to help visual understanding. It is not an essential part. */
  gchar *extra_str =
      g_strdup_printf
      ("tensor_decoder mode=direct_video silent=FALSE ! videoconvert ! "
      "textoverlay text=\"Early exit module %d\" valignment=top halignment=left font-desc=\"Sans, 32\"",
      idx);

  gchar *EE_module =
      g_strdup_printf
      ("tensor_filter framework=custom-easy model=%s ! tensor_filter framework=custom-easy model=%s outputCombination=i0,o0 ! "
      "tensor_if name=tif_%d compared-value=A_VALUE compared-value-option=0:0:0:0,1 "
      "supplied-value=200 operator=GE then=TENSORPICK then-option=0 else=TENSORPICK else-option=0 "
      "tif_%d.src_0 ! %s ! join.sink_%d " "tif_%d.src_1 ", main_model,
      check_model, idx, idx, extra_str, idx, idx);

  g_free (extra_str);
  return EE_module;
}

/**
 * @brief Callback function to passthrough the input tensor
 */
static int
CE_pass_cb (void *data, const GstTensorFilterProperties * prop,
    const GstTensorMemory * in, GstTensorMemory * out)
{
  unsigned int t;
  for (t = 0; t < prop->output_meta.num_tensors; t++) {
    memcpy (out[t].data, in[t].data, in[t].size);
  }
  return 0;
}

/**
 * @brief Callback function to increase the brightness
 */
static int
CE_increase_br_cb (void *data, const GstTensorFilterProperties * prop,
    const GstTensorMemory * in, GstTensorMemory * out)
{
  unsigned int t, i;
  for (t = 0; t < prop->output_meta.num_tensors; t++) {
    for (i = 0; i < in[t].size; i++) {
      ((guint8 *) out[t].data)[i] =
          (((guint8 *) in[t].data)[i] + 40) >
          255 ? 255 : (((guint8 *) in[t].data)[i] + 40);
    }
  }
  return 0;
}

/**
 * @brief Callback function to calculate average tensor value
 */
static int
CE_get_avg_cb (void *data, const GstTensorFilterProperties * prop,
    const GstTensorMemory * in, GstTensorMemory * out)
{
  guint t, i, sum = 0, size = 0;

  for (t = 0; t < prop->output_meta.num_tensors; t++) {
    for (i = 0; i < in[t].size; i++) {
      sum += ((guint8 *) in[t].data)[i];
    }
    size += in[t].size;
  }
  ((gdouble *) out[0].data)[0] = sum / size;
  return 0;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  gchar *str_pipeline, **EEMs = NULL;
  gulong handle_id;

  /* setting tensor_filter custom-easy */
  const GstTensorsInfo info_in = {
    .num_tensors = 1U,
    .info = {{.name = NULL,.type = _NNS_UINT8,.dimension = {3, 640, 480, 1}}},
  };
  const GstTensorsInfo info_out = {
    .num_tensors = 1U,
    .info = {{.name = NULL,.type = _NNS_UINT8,.dimension = {3, 640, 480, 1}}},
  };
  const GstTensorsInfo avg_info = {
    .num_tensors = 1U,
    .info = {{.name = NULL,.type = _NNS_FLOAT64,.dimension = {1, 1, 1, 1}}},
  };

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* Get early exit module description */
  EEMs = (gchar **) g_malloc0 (sizeof (gchar *) * 4);
  EEMs[0] =
      _get_early_exit_desc (0, "CE_increase_brightness_0", "CE_get_average_0");
  EEMs[1] =
      _get_early_exit_desc (1, "CE_increase_brightness_1", "CE_get_average_1");
  EEMs[2] =
      _get_early_exit_desc (2, "CE_increase_brightness_2", "CE_get_average_2");
  EEMs[3] =
      _get_early_exit_desc (3, "CE_increase_brightness_3", "CE_get_average_3");

  /* init pipeline */
  /** If the average brightness value of the input video is more than 100 (The value ranges from 0 to 255.),
   * it does not pass through the next EE module and sends the results immediately.
   */
  str_pipeline =
      g_strdup_printf
      ("v4l2src ! videoscale ! videoconvert ! video/x-raw,format=RGB,width=640,height=480,framerate=30/1 ! tensor_converter ! "
      "%s ! %s ! %s ! %s ! tensor_filter framework=custom-easy model=CE_passthrough_main ! "
      "tensor_decoder mode=direct_video silent=FALSE ! videoconvert ! "
      "textoverlay text=\"Normal exit\" valignment=top halignment=left font-desc=\"Sans, 32\" ! join.sink_4 "
      "join name=join ! ximagesink sync=false async=false", EEMs[0], EEMs[1],
      EEMs[2], EEMs[3]);
  g_strfreev (EEMs);

  /* Register custom easy filter */
  NNS_custom_easy_register ("CE_passthrough_main", CE_pass_cb, NULL, &info_in,
      &info_out);
  NNS_custom_easy_register ("CE_increase_brightness_0", CE_increase_br_cb, NULL,
      &info_in, &info_out);
  NNS_custom_easy_register ("CE_increase_brightness_1", CE_increase_br_cb, NULL,
      &info_in, &info_out);
  NNS_custom_easy_register ("CE_increase_brightness_2", CE_increase_br_cb, NULL,
      &info_in, &info_out);
  NNS_custom_easy_register ("CE_increase_brightness_3", CE_increase_br_cb, NULL,
      &info_in, &info_out);
  NNS_custom_easy_register ("CE_get_average_0", CE_get_avg_cb, NULL, &info_in,
      &avg_info);
  NNS_custom_easy_register ("CE_get_average_1", CE_get_avg_cb, NULL, &info_in,
      &avg_info);
  NNS_custom_easy_register ("CE_get_average_2", CE_get_avg_cb, NULL, &info_in,
      &avg_info);
  NNS_custom_easy_register ("CE_get_average_3", CE_get_avg_cb, NULL, &info_in,
      &avg_info);

  _print_log ("%s\n", str_pipeline);

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  handle_id = g_signal_connect (g_app.bus, "message",
      (GCallback) _message_cb, NULL);
  _check_cond_err (handle_id > 0);

  /* start pipeline */
  gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);

  /* run main loop */
  g_main_loop_run (g_app.loop);

  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

/* Unregister custom easy filter */
  NNS_custom_easy_unregister ("CE_passthrough_main");
  NNS_custom_easy_unregister ("CE_increase_brightness_0");
  NNS_custom_easy_unregister ("CE_increase_brightness_1");
  NNS_custom_easy_unregister ("CE_increase_brightness_2");
  NNS_custom_easy_unregister ("CE_increase_brightness_3");
  NNS_custom_easy_unregister ("CE_get_average_0");
  NNS_custom_easy_unregister ("CE_get_average_1");
  NNS_custom_easy_unregister ("CE_get_average_2");
  NNS_custom_easy_unregister ("CE_get_average_3");

error:
  _print_log ("close app..");
  _free_app_data ();
  return 0;
}
