/**
 * @file    sum_and_add.c
 * @date    4 Jul 2024
 * @brief   Tizen ML Service API example with RPK
 * @see     https://github.com/nnstreamer/nnstreamer
 *          https://github.com/nnstreamer/api
 * @author  Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 * @bug     No known bugs except for NYI items.
 */

#include "sum_and_add.h"
#include <glib.h>
#include <nnstreamer.h>
#include <ml-api-service.h>
#include <pthread.h>

/** @brief UI and ml-service related objects */
typedef struct appdata {
  Evas_Object *win;
  Evas_Object *conform;
  Evas_Object *head_label;
  Evas_Object *info_label;
  Evas_Object *desc_label;
  Evas_Object *input_label;
  Evas_Object *result_label;

  int is_running;
  pthread_t request_loop;

  ml_service_h service_handle;
  ml_tensors_info_h input_info;
  ml_tensors_info_h output_info;
  ml_tensors_data_h input_data_h;

  gchar *input_node_name;
  gchar *output_node_name;

} appdata_s;

Ecore_Pipe *data_output_pipe;

/** @brief Invoke ml_service evey 1 sec */
static void *
_invoke_request_loop (void *user_data)
{
  appdata_s *ad = (appdata_s *) user_data;
  while (1) {
    if (ad->is_running == 1) {
      ml_service_request (ad->service_handle, ad->input_node_name, ad->input_data_h);
    }

    g_usleep (1000 * 1000); /* request every 1 sec */
  }
}

/** @brief Return informative string of given tensors_info */
static char *
tensors_info_to_string (ml_tensors_info_h info)
{
  unsigned int count;
  GString *ret_str = g_string_new (NULL);

  ml_tensors_info_get_count (info, &count);
  g_string_append_printf (ret_str, "  - tensor count: %u\n<br>", count);

  for (guint idx = 0; idx < count; idx++){
    g_string_append_printf (ret_str, "  - tensor[%u]\n<br>", idx);
    gchar *name;
    ml_tensors_info_get_tensor_name (info, idx, &name);
    g_string_append_printf (ret_str, "    - name: %s\n<br>", name);
    g_free (name);

    ml_tensor_type_e type;
    ml_tensors_info_get_tensor_type (info, idx, &type);
    g_string_append_printf (ret_str, "    - type: %d\n<br>", type);

    ml_tensor_dimension dimension;
    ml_tensors_info_get_tensor_dimension (info, idx, dimension);
    for (int j = 0; j < ML_TENSOR_RANK_LIMIT; j++) {
      g_string_append_printf (ret_str, "    - dimension[%d]: %d\n<br>", j, dimension[j]);
      if (dimension[j] == 0)
        break;
    }

    size_t size;
    ml_tensors_info_get_tensor_size (info, idx, &size);
    g_string_append_printf (ret_str, "    - size: %zu byte\n<br>", size);
  }

  return g_string_free (ret_str, FALSE);
}

/** @brief event callback */
static void
_event_cb (ml_service_event_e event, ml_information_h event_data, void *user_data)
{
  ml_tensors_data_h new_data = NULL;

  if (event != ML_SERVICE_EVENT_NEW_DATA)
    return;

  // get tensors-data from event data.
  ml_information_get (event_data, "data", &new_data);

  // get the float result
  float *result;
  size_t result_size;
  int ret = ml_tensors_data_get_tensor_data (new_data, 0U, (void *) &result, &result_size);
  if (ret != ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_tensors_data_get_tensor_data failed");
    return;
  }

  // do something useful with the result.
  ecore_pipe_write (data_output_pipe, (void *) result, result_size);
}

/** @brief base code for basic UI app */
static void
win_delete_request_cb (void *data, Evas_Object *obj, void *event_info)
{
  ui_app_exit ();
}

/** @brief base code for basic UI app */
static void
win_back_cb (void *data, Evas_Object *obj, void *event_info)
{
  appdata_s *ad = data;
  elm_win_lower (ad->win);
}

/** @brief base code for basic UI app */
static void
update_gui (void *data, void *buf, unsigned int size)
{
  appdata_s *ad = data;
  float *result = (float *) buf;

  gchar *tt = g_strdup_printf (
    "<font_size=20>Result of [sum_and_add] is %.1f</font_size>", *result);

  elm_object_text_set (ad->result_label, tt);
  g_free (tt);
}

/** @brief Creates essential UI objects */
static void
create_base_gui (appdata_s *ad)
{
  ad->win = elm_win_util_standard_add (PACKAGE, PACKAGE);
  elm_win_autodel_set (ad->win, EINA_TRUE);

  if (elm_win_wm_rotation_supported_get (ad->win)) {
    int rots[4] = { 0, 90, 180, 270 };
    elm_win_wm_rotation_available_rotations_set (ad->win, (const int *) (&rots), 4);
  }
  evas_object_smart_callback_add (ad->win, "delete,request", win_delete_request_cb, NULL);
  eext_object_event_callback_add (ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

  ad->conform = elm_conformant_add (ad->win);
  elm_win_indicator_mode_set (ad->win, ELM_WIN_INDICATOR_SHOW);
  elm_win_indicator_opacity_set (ad->win, ELM_WIN_INDICATOR_OPAQUE);
  evas_object_size_hint_weight_set (ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_win_resize_object_add (ad->win, ad->conform);
  evas_object_show (ad->conform);

  ad->head_label = elm_label_add (ad->conform);
  elm_object_text_set (ad->head_label, "<align=center>Hello Tizen ML Service API</align>");
  evas_object_move (ad->head_label, 120, 80);
  evas_object_resize (ad->head_label, 600, 80);
  evas_object_show  (ad->head_label);

  ad->info_label = elm_label_add (ad->conform);
  elm_object_text_set (ad->info_label, "");
  evas_object_move (ad->info_label, 30, 200);
  evas_object_resize (ad->info_label, 400, 400);
  evas_object_show  (ad->info_label);

  ad->desc_label = elm_label_add (ad->conform);
  elm_object_text_set (ad->desc_label, "");
  evas_object_move (ad->desc_label, 300, 200);
  evas_object_resize (ad->desc_label, 600, 100);
  evas_object_show  (ad->desc_label);

  ad->input_label = elm_label_add (ad->conform);
  elm_object_text_set (ad->input_label, "");
  evas_object_move (ad->input_label, 300, 300);
  evas_object_resize (ad->input_label, 600, 100);
  evas_object_show  (ad->input_label);

  ad->result_label = elm_label_add (ad->conform);
  elm_object_text_set (ad->result_label, "");
  evas_object_move (ad->result_label, 300, 350);
  evas_object_resize (ad->result_label, 600, 100);
  evas_object_show  (ad->result_label);

  evas_object_show (ad->win);
}

/** @brief Destroy the ml_service and related elements */
void
terminate_ml_service (appdata_s *ad)
{
  ad->is_running = 0;
  g_usleep (1000 * 1000);
  pthread_join (ad->request_loop, NULL);
  ml_tensors_info_destroy (ad->input_info);
  ml_tensors_info_destroy (ad->output_info);
  ml_service_destroy (ad->service_handle);
}

/** @brief Initialize the ml_service path */
int
init_ml_service (appdata_s *ad)
{
  int status = -1;
  gchar *conf_file_path = NULL;
  ml_information_list_h res_info_list;

  // get conf file from rpk using key for the file
  // const char *conf_key = "sum_and_add_resource_conf"; // use single conf
  const char *conf_key = "sum_and_add_resource_pipeline_conf"; // use pipeline conf
  status = ml_service_resource_get (conf_key, &res_info_list);
  unsigned int info_length = 0U;
  if (status!= ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to get resource data");
    return status;
  }

  // the conf file is the first item of `res_info_list`
  ml_information_list_length (res_info_list, &info_length);
  for (int i = 0; i < info_length; i++) {
    ml_information_h info;
    status = ml_information_list_get (res_info_list, i, &info);
    if (status!= ML_ERROR_NONE) {
      dlog_print (DLOG_ERROR, LOG_TAG, "ml_information_list_get failed");
    }
    char *path = NULL;
    status = ml_information_get (info, "path", (void **) &path);
    if (status!= ML_ERROR_NONE) {
      dlog_print (DLOG_ERROR, LOG_TAG, "ml_information_get failed");
    }
    conf_file_path = g_strdup (path);
  }

  ml_information_list_destroy (res_info_list);

  if (!g_file_test (conf_file_path, (G_FILE_TEST_EXISTS))) {
    dlog_print (DLOG_ERROR, LOG_TAG, "conf file does not exist!");
    return -1;
  }

  // make ml_service
  status = ml_service_new (conf_file_path, &ad->service_handle);
  if (status!= ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_service_new failed");
    return status;
  }

  // get input_node_name and output_node_name from conf file (it's running pipeline NOT single)
  status = ml_service_get_information (ad->service_handle, "input_node_name", &ad->input_node_name);
  if (status!= ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "No input_node_name given. It's single!");
    ad->input_node_name = NULL;
  }
  status = ml_service_get_information (ad->service_handle, "output_node_name", &ad->output_node_name);
  if (status!= ML_ERROR_NONE) {
    dlog_print (DLOG_INFO, LOG_TAG, "No output_node_name given. It's single!");
    output_node_name = NULL;
  }

  // get input information
  status = ml_service_get_input_information (ad->service_handle, ad->input_node_name, &ad->input_info);
  if (status!= ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_service_get_input_information failed");
    return status;
  }

  // get output information
  status = ml_service_get_output_information (ad->service_handle, ad->output_node_name, &ad->output_info);
  if (status!= ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_service_get_output_information failed");
    return status;
  }

  // set new_data callback
  status = ml_service_set_event_cb (ad->service_handle, _event_cb, NULL);
  if (status!= ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_service_set_event_cb failed");
    return status;
  }

  // show tensors_info
  gchar *input_info_str = tensors_info_to_string (ad->input_info);
  gchar *output_info_str = tensors_info_to_string (ad->output_info);
  gchar *info_value_text = g_strdup_printf ("<font_size=16>Input tensors info:<br>%s<br>output tensors info:<br>%s</font_size>", input_info_str, output_info_str);
  elm_object_text_set (ad->info_label, info_value_text);
  g_free (input_info_str);
  g_free (output_info_str);

  // show information of the serivce
  gchar *description = NULL;
  status = ml_service_get_information (ad->service_handle, "description", &description);
  gchar *info_desc_text = g_strdup_printf ("<font_size=20>Description:<br>  %s</font_size>", description);
  elm_object_text_set (ad->desc_label, info_desc_text);
  g_free (description);

  // set input data
  ml_tensors_data_create (ad->input_info, &ad->input_data_h);
  float input_arr[4] = { 1.0f, 2.0f, 3.0f, 4.0f };

  // set 0-th tensor data with given buffer
  status = ml_tensors_data_set_tensor_data (ad->input_data_h, 0U, (uint8_t *) input_arr, 4 * sizeof (float));
  if (status != ML_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "ml_tensors_data_set_tensor_data failed");
    return status;
  }

  // show input tensor value
  gchar *input_tensor_value_text = g_strdup_printf ("<font_size=20>Input tensor = float { %.1f, %.1f, %.1f, %.1f }</font_size>", input_arr[0], input_arr[1], input_arr[2], input_arr[3]);
  elm_object_text_set (ad->input_label, input_tensor_value_text);
  g_free (input_tensor_value_text);

  // set loop
  ad->is_running = 0;
  status = pthread_create (&ad->request_loop, NULL, _invoke_request_loop, ad);
  if (status != 0) {
    dlog_print (DLOG_ERROR, LOG_TAG, "pthread_create failed");
    return status;
  }

  return status;
}

/** @brief base code for basic UI app */
static bool
app_create (void *data)
{
  appdata_s *ad = data;

  data_output_pipe = ecore_pipe_add ((Ecore_Pipe_Cb) update_gui, ad);
  if (!data_output_pipe) {
    dlog_print (DLOG_ERROR, LOG_TAG, "Failed to create data output pipe");
  }

  create_base_gui (ad);
  init_ml_service (ad);

  return true;
}

/** @brief base code for basic UI app */
static void
app_control (app_control_h app_control, void *data)
{
  return;
}

/** @brief base code for basic UI app */
static void
app_pause (void *data)
{
  appdata_s *ad = data;
  ad->is_running = 0;
}

/** @brief base code for basic UI app */
static void
app_resume (void *data)
{
  appdata_s *ad = data;
  ad->is_running = 1;
}

/** @brief base code for basic UI app */
static void
app_terminate (void *data)
{
  appdata_s *ad = data;
  ecore_pipe_del (data_output_pipe);
  terminate_ml_service (ad);
}

/** @brief base code for basic UI app */
int
main(int argc, char *argv[])
{
  appdata_s ad = {0,};
  int ret = 0;

  ui_app_lifecycle_callback_s event_callback = {0,};

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;

  ret = ui_app_main (argc, argv, &event_callback, &ad);
  if (ret != APP_ERROR_NONE) {
    dlog_print (DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
  }

  return ret;
}
