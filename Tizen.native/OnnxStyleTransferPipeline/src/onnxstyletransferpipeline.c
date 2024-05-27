/**
 * @file onnxstyletransferpipeline.h
 * @date 27 May 2024
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author Suyeon Kim <suyeon5.kim@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include "onnxstyletransferpipeline.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <nnstreamer.h>
#include <ml-api-service.h>
#include <ml-api-common.h>
#include <image_util.h>

/** 
 * @brief Appdata struct
 */
typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *grid;
	Evas_Object *inference_button;
	Evas_Object *exit_button;

	g_autofree gchar *model_config;
} appdata_s;

g_autofree gchar *image_filename = "cat.jpg";
g_autofree gchar *image_path;

Evas_Object *image;
Ecore_Pipe *output_pipe;

ml_tensors_info_h input_info;
ml_tensors_info_h output_info;

ml_service_h handle;

image_util_image_h decoded_image;
image_util_decode_h decode_h;

/**
 * @brief evas object smart callback
 */
static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
	ui_app_exit();
}

/**
 * @brief evas object smart callback
 */
static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	appdata_s *ad = data;
	/* Let window go to hide state. */
	elm_win_lower(ad->win);
}

/**
 * @brief Update the image
 * @param data The data passed from the callback registration function (not used
 * here)
 * @param buf The data passed from pipe
 * @param size The data size
 */
void _update_image_cb(void *data, void *buf, unsigned int size) {
	unsigned char *img_src = evas_object_image_data_get(image, EINA_TRUE);
	memcpy(img_src, buf, size);
	evas_object_image_data_update_add(image, 0, 0, 720, 720);
}

/**
 * @brief Inference callback function
 */
static void onnx_inference_cb(void *data, Evas_Object *obj, void *event_info) {

	int status;
	unsigned long width, height;
	unsigned char *dataBuffer = NULL;
	size_t bufferSize = 0;

	status = image_util_decode_set_input_path(decode_h, image_path);
	if (status != IMAGE_UTIL_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "image_util_decode_set_input_path failed");
	}

	status =
		image_util_decode_set_colorspace(decode_h, IMAGE_UTIL_COLORSPACE_RGB888);
	if (status != IMAGE_UTIL_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "image_util_decode_set_colorspace failed");
	}

	status = image_util_decode_run2(decode_h, &decoded_image);
	if (status != IMAGE_UTIL_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", status);

	status = image_util_get_image(decoded_image, &width, &height, NULL, &dataBuffer, &bufferSize);
	if (status != IMAGE_UTIL_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", status);

	// create tensors_data handle with input_info
	ml_tensors_data_h input_data = NULL;

	status = ml_tensors_data_create(input_info, &input_data);
	if (status != ML_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "ml_tensors_data_create %d", status);
	}

	status =
		ml_tensors_data_set_tensor_data(input_data, 0U, dataBuffer, bufferSize);
	if (status != ML_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "ml_tensors_data_set_tensor_data failed");
	}
	for (int i = 0; i < 5; i++) {
		g_usleep (50000U);

		status = ml_service_request (handle, "appsrc", input_data);
		if (status != ML_ERROR_NONE) {
			dlog_print(DLOG_ERROR, LOG_TAG, "ml_service_request failed");
		}
	}

	free (dataBuffer);
	ml_tensors_data_destroy (input_data);
	
}

/** 
 * @brief Creates essential UI objects.
 */
static void
create_base_gui(appdata_s *ad)
{
	ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
	elm_win_autodel_set(ad->win, EINA_TRUE);

	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}

	evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
	eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

	ad->conform = elm_conformant_add(ad->win);
	elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

	ad->grid = elm_grid_add (ad->conform);
	elm_grid_size_set(ad->grid, 720, 720);
	evas_object_size_hint_weight_set(ad->grid, EVAS_HINT_EXPAND,EVAS_HINT_EXPAND);
	elm_object_content_set(ad->conform, ad->grid);
	evas_object_show(ad->grid);

	image = evas_object_image_filled_add(ad->win);
	evas_object_image_file_set (image, image_path, NULL);
	elm_grid_pack(ad->grid, image, 180, 100, 360, 360);
	evas_object_show (image);

	ad->inference_button = elm_button_add (ad->grid);
	evas_object_size_hint_align_set (ad->inference_button, EVAS_HINT_FILL,
		EVAS_HINT_FILL);
	elm_object_text_set (ad->inference_button, "Inference with trained model");
	evas_object_show (ad->inference_button);
	evas_object_smart_callback_add (ad->inference_button, "clicked",
		onnx_inference_cb, NULL);
	elm_grid_pack (ad->grid, ad->inference_button, 180, 500, 360, 60);

	ad->exit_button = elm_button_add (ad->grid);
	evas_object_size_hint_align_set (ad->exit_button, EVAS_HINT_FILL,
		EVAS_HINT_FILL);
	elm_object_text_set (ad->exit_button, "Exit");
	evas_object_show (ad->exit_button);
	evas_object_smart_callback_add (ad->exit_button, "clicked",
		win_delete_request_cb, ad);
	elm_grid_pack (ad->grid, ad->exit_button, 180, 600, 360, 60);

	/* Show window after base gui is set up */
	evas_object_show(ad->win);
}

/** @brief new_data callback for this app */
static void
_new_data_cb (ml_service_event_e event, ml_information_h event_data, void *user_data)
{
	ml_tensors_data_h data = NULL;
	char *data_ptr;
	size_t data_size;
	int status;

	switch (event) {
		case ML_SERVICE_EVENT_NEW_DATA:
			status = ml_information_get (event_data, "data", &data);
			if (status != ML_ERROR_NONE) {
				dlog_print(DLOG_ERROR, LOG_TAG, "ml_information_get failed");
			}
			status = ml_tensors_data_get_tensor_data (data, 0U, (void **)&data_ptr, &data_size);
			if (status != ML_ERROR_NONE) {
				dlog_print(DLOG_ERROR, LOG_TAG, "ml_tensors_data_get_tensor_data failed");
			}
			ecore_pipe_write(output_pipe, data_ptr, data_size);
		break;
		default:
		break;
	}

}

/** 
 * @brief base code for basic UI app.
 */
static bool
app_create(void *data)
{
	appdata_s *ad = data;
	int status;
	g_autofree gchar *res_path = app_get_resource_path ();

	output_pipe = ecore_pipe_add ((Ecore_Pipe_Cb) _update_image_cb, NULL);

	create_base_gui(ad);

	ad->model_config = g_strdup_printf ("%s%s", res_path, "onnx_style_transfer.pipeline.appsrc.conf");

	status = ml_service_new (ad->model_config, &handle);
	if (status != ML_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "ml_service_create failed");
	}

	status = ml_service_set_event_cb (handle, _new_data_cb, NULL);
	if (status != ML_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "ml_service_set_callback failed");
	}

	status = ml_service_get_input_information (handle, "appsrc", &input_info);
	if (status != ML_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "ml_handle_get_input_information failed");
	}

	status = ml_service_get_output_information (handle, "tensor_sink", &output_info);
	if (status != ML_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "ml_service_get_output_information failed");
	}

	return true;
}

/**
 * @brief base code for basic UI app.
 */
static void
app_control(app_control_h app_control, void *data)
{
	/* Handle the launch request. */
}

/**
 * @brief base code for basic UI app.
 */
static void
app_pause(void *data)
{
	/* Take necessary actions when application becomes invisible. */
}

/**
 * @brief base code for basic UI app.
 */
static void
app_resume(void *data)
{
	/* Take necessary actions when application becomes visible. */
}

/**
 * @brief base code for basic UI app.
 */
static void
app_terminate(void *data)
{
	/* Release all resources. */

	image_util_decode_destroy(decode_h);
	ecore_pipe_del(output_pipe);

	ml_tensors_info_destroy (input_info);
	ml_tensors_info_destroy (output_info);
	ml_service_destroy(handle);
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/

	int ret;
	char *language;

	ret = app_event_get_language(event_info, &language);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_event_get_language() failed. Err = %d.", ret);
		return;
	}

	if (language != NULL) {
		elm_language_set(language);
		free(language);
	}
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_orient_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_DEVICE_ORIENTATION_CHANGED*/
	return;
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_BATTERY*/
}

/**
 * @brief base code for basic UI app.
 */
static void
ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_MEMORY*/
}

/**
 * @brief base code for basic UI app.
 */
int
main(int argc, char *argv[])
{
	appdata_s ad = {0,};
	
	int ret = 0;
	g_autofree gchar *img_path = app_get_resource_path ();

	ui_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

	image_path = g_strdup_printf ("%s%s", img_path, image_filename);
	dlog_print(DLOG_INFO, LOG_TAG, "The image path is '%s'.", image_path);

	image_util_decode_create(&decode_h);

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;

	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_DEVICE_ORIENTATION_CHANGED], APP_EVENT_DEVICE_ORIENTATION_CHANGED, ui_app_orient_changed, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, ui_app_region_changed, &ad);

	ret = ui_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);
	}

	return ret;
}
