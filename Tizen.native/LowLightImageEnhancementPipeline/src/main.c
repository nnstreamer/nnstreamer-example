/**
 * @file	main.c
 * @date	08 Feb 2022
 * @brief	example with TF-Lite model for low light image enhancement
 * @see		https://github.com/nnstreamer/nnstreamer
 * @author	Yelin Jeong <yelini.jeong@samsung.com>
 * @bug		This example runs well in emulator, but on the Tizen device it
 * 			takes more than 10 seconds.
 */

#include "main.h"
#include <app.h>
#include <dlog.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <image_util.h>
#include <nnstreamer-single.h>
#include <nnstreamer.h>
#include <tizen_error.h>

#define BUFLEN 256
#define IMG_HEIGHT 400
#define IMG_WIDTH 600

const char *sample_image_filename = "0.jpg";
char image_file_path[BUFLEN];

Evas_Object *image;
Evas_Object *win;
Evas_Object *button;
Evas_Object *bg;

gchar *pipeline;
gchar *model;

ml_pipeline_src_h src_handle;
ml_pipeline_h pipeline_handle;
ml_pipeline_sink_h sink_handle;
ml_tensors_info_h info;
ml_tensors_data_h input;

image_util_decode_h decode_h = NULL;
Ecore_Pipe *data_output_pipe;

bool is_invoked;

/**
 * @brief base code generated from tizen-studio
 */
static void win_delete_request_cb(void *data, Evas_Object *obj,
                                  void *event_info) {
  ui_app_exit();
}

/**
 * @brief base code generated from tizen-studio
 */
static void win_back_cb(void *data, Evas_Object *obj, void *event_info) {
  /* Let window go to hide state. */
  elm_win_lower(win);
}

/**
 * @brief Image util callback function
 */
static void image_util_cb(void *data, Evas_Object *obj, void *event_info) {
  if (is_invoked)
    return;
  is_invoked = true;

  unsigned char *img_source = NULL;
  unsigned long width, height;
  unsigned long long size_decode;

  int error_code = image_util_decode_set_input_path(decode_h, image_file_path);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "image_util_decode_set_input_path %d",
               error_code);
    return;
  }

  error_code = image_util_decode_set_output_buffer(decode_h, &img_source);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "image_util_decode_set_output_buffer %d",
               error_code);
    return;
  }

  error_code =
      image_util_decode_set_colorspace(decode_h, IMAGE_UTIL_COLORSPACE_RGB888);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "image_util_decode_set_colorspace %d",
               error_code);
    return;
  }

  error_code = image_util_decode_run(decode_h, &width, &height, &size_decode);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "image_util_decode_run %d", error_code);
    return;
  }

  ml_tensor_dimension in_dim = {3, 600, 400, 1};

  ml_tensors_info_create(&info);
  ml_tensors_info_set_count(info, 1);
  ml_tensors_info_set_tensor_type(info, 0, ML_TENSOR_TYPE_UINT8);
  ml_tensors_info_set_tensor_dimension(info, 0, in_dim);

  error_code = ml_tensors_data_create(info, &input);
  if (error_code != ML_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "ml_tensors_data_create %d", error_code);
  }
  error_code =
      ml_tensors_data_set_tensor_data(input, 0, img_source, 3 * 600 * 400);
  if (error_code != ML_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "ml_tensors_data_set_tensor_data %d",
               error_code);
  }

  error_code = ml_pipeline_src_input_data(src_handle, input,
                                          ML_PIPELINE_BUF_POLICY_DO_NOT_FREE);
  if (error_code != ML_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "ml_pipeline_src_input_data %d",
               error_code);
  }
}

/**
 * @brief New data callback function
 */
static void new_data_cb(const ml_tensors_data_h data,
                        const ml_tensors_info_h info, void *user_data) {
  char *data_ptr;
  size_t data_size;
  int error_code =
      ml_tensors_data_get_tensor_data(data, 0, (void **)&data_ptr, &data_size);

  if (error_code != ML_ERROR_NONE)
    dlog_print(DLOG_ERROR, LOG_TAG, "Failed to get tensor data.");

  ecore_pipe_write(data_output_pipe, data_ptr, data_size);
}

/**
 * @brief Creates essential objects
 */
static void create_base_gui() {
  /* Create Evas instance */
  win = elm_win_util_standard_add(PACKAGE, PACKAGE);
  elm_win_autodel_set(win, EINA_TRUE);

  evas_object_smart_callback_add(win, "delete,request", win_delete_request_cb,
                                 NULL);
  eext_object_event_callback_add(win, EEXT_CALLBACK_BACK, win_back_cb, NULL);

  /* Get the canvas */
  Evas *evas = evas_object_evas_get(win);

  bg = evas_object_rectangle_add(evas);
  evas_object_color_set(bg, 227, 245, 254, 255);
  evas_object_resize(bg, 720, 1280);
  evas_object_move(bg, 0, 0);
  evas_object_show(bg);

  image = evas_object_image_filled_add(evas);
  evas_object_image_file_set(image, image_file_path, NULL);
  evas_object_resize(image, IMG_WIDTH, IMG_HEIGHT);
  evas_object_move(image, 60, 250);
  evas_object_show(image);

  button = elm_button_add(win);
  elm_object_text_set(button, "Get Enhancement Result");
  evas_object_color_set(button, 227, 254, 254, 255);
  evas_object_resize(button, 600, 100);
  evas_object_move(button, 60, 700);
  evas_object_show(button);
  evas_object_smart_callback_add(button, "clicked", image_util_cb, NULL);

  evas_object_show(win);
}

/**
 * @brief Update the image
 * @param data The data passed from the callback registration function (not used
 * here)
 * @param buf The data passed from pipe
 * @param size The data size
 */
void update_gui(void *data, void *buf, unsigned int size) {
  unsigned char *img_src = evas_object_image_data_get(image, EINA_TRUE);
  memcpy(img_src, buf, size);
  evas_object_image_data_update_add(image, 0, 0, IMG_WIDTH, IMG_HEIGHT);
}

/**
 * @brief Hook to take necessary actions before main event loop starts.
 * Initialize UI resources and application's data.
 * If this function returns true, the main loop of application starts.
 * If this function returns false, the application is terminated.
 * @param user_data The data passed from the callback registration function
 * @return @c true on success (the main loop is started), @c false otherwise
 * (the app is terminated)
 */
static bool app_create(void *data) {
  is_invoked = false;
  create_base_gui();

  char *path = app_get_resource_path();
  model = g_build_filename(path, "lite-model_zero-dce_1.tflite", NULL);
  pipeline = g_strdup_printf(
      "appsrc name=appsrc ! "
      "other/tensor,type=uint8,dimension=3:600:400:1,framerate=0/1 ! "
      "tensor_transform mode=arithmetic "
      "option=typecast:float32,add:0,div:255.0 ! "
      "tensor_filter name=tfilter framework=tensorflow-lite model=%s ! "
      "tensor_transform mode=arithmetic option=mul:255.0,add:0.0 ! "
      "tensor_transform mode=clamp option=0:255 ! "
      "other/tensor,type=float32,dimension=3:600:400:1,framerate=0/1 ! "
      "tensor_transform mode=typecast option=uint8 ! "
      "other/tensor,type=uint8,dimension=3:600:400:1,framerate=0/1 ! "
      "tensor_decoder mode=direct_video ! videoconvert ! "
      "video/x-raw,width=600,height=400,format=BGRA,framerate=0/1 ! "
      "tensor_converter ! tensor_sink name=tensor_sink",
      model);

  int error_code =
      ml_pipeline_construct(pipeline, NULL, NULL, &pipeline_handle);
  if (error_code != ML_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "ml_pipeline_construct %d", error_code);
  }

  error_code =
      ml_pipeline_src_get_handle(pipeline_handle, "appsrc", &src_handle);
  if (error_code != ML_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "ml_pipeline_src_get_handle %d",
               error_code);
  }

  error_code = ml_pipeline_sink_register(pipeline_handle, "tensor_sink",
                                         new_data_cb, NULL, &sink_handle);
  if (error_code != ML_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "ml_pipeline_sink_register %d", error_code);
  }

  error_code = ml_pipeline_start(pipeline_handle);
  if (error_code != ML_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "ml_pipeline_start %d", error_code);
    ml_pipeline_destroy(pipeline_handle);
  }

  g_free(pipeline);
  g_free(model);
  return true;
}

/**
 * @brief This callback function is called when another application
 * sends a launch request to the application.
 *
 * @param app_control The launch request(not used here)
 * @param user_data The data passed from the callback registration function(not
 * used here)
 */
static void app_control(app_control_h app_control, void *data) {
  /* Handle the launch request. */
}

/**
 * @brief This callback function is called each time
 * the application is completely obscured by another application
 * and becomes invisible to the user.
 *
 * @param user_data The data passed from the callback registration function(not
 * used here)
 */
static void app_pause(void *data) {
  /* Take necessary actions when application becomes invisible. */
}

/**
 * @brief Verifies if permission to the privilege is granted
 * @details This callback function is called each time
 * the application becomes visible to the user.
 *
 * @param user_data The data passed from the callback registration function(not
 * used here)
 */
static void app_resume(void *data) {
  /* Take necessary actions when application becomes visible. */
}

/**
 * @brief This callback function is called once after the main loop of the
 * application exits.
 *
 * @param user_data The data passed from the callback registration function(not
 * used here)
 */
static void app_terminate(void *data) {
  ml_pipeline_destroy(pipeline_handle);
  ml_tensors_info_destroy(info);
  ml_tensors_info_destroy(input);

  ecore_pipe_del(data_output_pipe);
  image_util_decode_destroy(decode_h);
}

/**
 * @brief Main function of the application.
 */
int main(int argc, char *argv[]) {
  ui_app_lifecycle_callback_s event_callback = {
      0,
  };
  int ret = 0;

  ecore_init();

  /* Get the path to the sample image stored in the resources. */
  char *resource_path = app_get_resource_path();
  snprintf(image_file_path, BUFLEN, "%s%s", resource_path,
           sample_image_filename);

  image_util_decode_create(&decode_h);

  data_output_pipe = ecore_pipe_add(update_gui, NULL);

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;

  ret = ui_app_main(argc, argv, &event_callback, NULL);

  if (ret != APP_ERROR_NONE) {
    dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
  }
  return ret;
}
