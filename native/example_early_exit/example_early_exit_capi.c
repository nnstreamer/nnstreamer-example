/**
 * @file	example_early_exit_capi.c
 * @date	16 Dec 2020
 * @brief	Early exit network example with multiple module using nnstreamer C-API
 * @author	Gichan Jang <gichan2.jnag@samsung.com>
 * @bug		No known bugs.
 */

#include <unistd.h>
#include <glib.h>
#include <nnstreamer.h>
#include <nnstreamer/tensor_filter_custom_easy.h>

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
CE_pass_cb (const ml_tensors_data_h in, ml_tensors_data_h out, void *user_data)
{
  void *raw_data = NULL;
  size_t data_size;

  ml_tensors_data_get_tensor_data (in, 0, &raw_data, &data_size);
  ml_tensors_data_set_tensor_data (out, 0, raw_data, data_size);

  return 0;
}

/**
 * @brief Callback function to increase the brightness
 */
static int
CE_increase_br_cb (const ml_tensors_data_h in, ml_tensors_data_h out,
    void *user_data)
{
  void *in_data = NULL, *out_data = NULL;
  size_t in_size, out_size;
  guint i;

  ml_tensors_data_get_tensor_data (in, 0, &in_data, &in_size);
  ml_tensors_data_get_tensor_data (out, 0, &out_data, &out_size);
  for (i = 0; i < in_size; i++) {
    ((guint8 *) out_data)[i] =
        ((guint8 *) in_data)[i] + 40 > 255 ? 255 : ((guint8 *) in_data)[i] + 40;
  }
  return 0;
}

/**
 * @brief Callback function to calculate average tensor value
 */
static int
CE_get_avg_cb (const ml_tensors_data_h in, ml_tensors_data_h out,
    void *user_data)
{
  void *in_data = NULL, *out_data = NULL;
  size_t in_size, out_size;
  guint i;
  gdouble sum = 0.0;

  ml_tensors_data_get_tensor_data (in, 0, &in_data, &in_size);
  ml_tensors_data_get_tensor_data (out, 0, &out_data, &out_size);

  for (i = 0; i < in_size; i++) {
    sum += ((guint8 *) in_data)[i];
  }
  ((gdouble *) out_data)[0] = sum / in_size;

  return 0;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  gchar *str_pipeline;
  gchar **EEMs = NULL;
  ml_custom_easy_filter_h custom[9];
  ml_tensors_info_h in_info, out_info, avg_info;
  ml_tensor_dimension video_dim = { 3, 640, 480, 1 };
  ml_tensor_dimension avg_dim = { 1, 1, 1, 1 };
  ml_pipeline_h pipe;

  /* setting tensor_filter custom-easy */
  ml_tensors_info_create (&in_info);
  ml_tensors_info_set_count (in_info, 1);
  ml_tensors_info_set_tensor_type (in_info, 0, ML_TENSOR_TYPE_UINT8);
  ml_tensors_info_set_tensor_dimension (in_info, 0, video_dim);

  ml_tensors_info_create (&out_info);
  ml_tensors_info_set_count (out_info, 1);
  ml_tensors_info_set_tensor_type (out_info, 0, ML_TENSOR_TYPE_UINT8);
  ml_tensors_info_set_tensor_dimension (out_info, 0, video_dim);

  ml_tensors_info_create (&avg_info);
  ml_tensors_info_set_count (avg_info, 1);
  ml_tensors_info_set_tensor_type (avg_info, 0, ML_TENSOR_TYPE_FLOAT64);
  ml_tensors_info_set_tensor_dimension (avg_info, 0, avg_dim);

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
  ml_pipeline_custom_easy_filter_register ("CE_passthrough_main", in_info,
      out_info, CE_pass_cb, NULL, &custom[0]);
  ml_pipeline_custom_easy_filter_register ("CE_increase_brightness_0", in_info,
      out_info, CE_increase_br_cb, NULL, &custom[1]);
  ml_pipeline_custom_easy_filter_register ("CE_increase_brightness_1", in_info,
      out_info, CE_increase_br_cb, NULL, &custom[2]);
  ml_pipeline_custom_easy_filter_register ("CE_increase_brightness_2", in_info,
      out_info, CE_increase_br_cb, NULL, &custom[3]);
  ml_pipeline_custom_easy_filter_register ("CE_increase_brightness_3", in_info,
      out_info, CE_increase_br_cb, NULL, &custom[4]);
  ml_pipeline_custom_easy_filter_register ("CE_get_average_0", in_info,
      avg_info, CE_get_avg_cb, NULL, &custom[5]);
  ml_pipeline_custom_easy_filter_register ("CE_get_average_1", in_info,
      avg_info, CE_get_avg_cb, NULL, &custom[6]);
  ml_pipeline_custom_easy_filter_register ("CE_get_average_2", in_info,
      avg_info, CE_get_avg_cb, NULL, &custom[7]);
  ml_pipeline_custom_easy_filter_register ("CE_get_average_3", in_info,
      avg_info, CE_get_avg_cb, NULL, &custom[8]);

  ml_pipeline_construct (str_pipeline, NULL, NULL, &pipe);

  ml_pipeline_start (pipe);

  sleep (10);

  ml_pipeline_stop (pipe);
  ml_pipeline_destroy (pipe);

  /* Unregister custom easy filter */
  ml_pipeline_custom_easy_filter_unregister (custom[0]);
  ml_pipeline_custom_easy_filter_unregister (custom[1]);
  ml_pipeline_custom_easy_filter_unregister (custom[2]);
  ml_pipeline_custom_easy_filter_unregister (custom[3]);
  ml_pipeline_custom_easy_filter_unregister (custom[4]);
  ml_pipeline_custom_easy_filter_unregister (custom[5]);
  ml_pipeline_custom_easy_filter_unregister (custom[6]);
  ml_pipeline_custom_easy_filter_unregister (custom[7]);
  ml_pipeline_custom_easy_filter_unregister (custom[8]);

  ml_tensors_info_destroy (in_info);
  ml_tensors_info_destroy (out_info);
  ml_tensors_info_destroy (avg_info);
  g_free (str_pipeline);

  return 0;
}
