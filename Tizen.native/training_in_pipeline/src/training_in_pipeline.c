/**
 * @file training_in_pipeline.c
 * @date 07 September 2023
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author Hyunil Park <hyunil46.park.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */
#include <glib.h>
#include <nnstreamer.h>
#include <ml-api-service.h>
#include <ml-api-common.h>
#include "training_in_pipeline.h"

/**
 * @brief Appdata struct
 */
typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *box;
	Evas_Object *title_label;
	Evas_Object *label1;
	Evas_Object *label2;
	Evas_Object *get_model_button;
	Evas_Object *start_button;
	Evas_Object *stop_button;
	Evas_Object *model_resist_button;
	Evas_Object *resource_resist_button;
	Evas_Object *get_resource_button;
	Evas_Object *exit_button;

	Ecore_Pipe *ecore_pipe; /* To use main loop */
	ml_pipeline_h ml_pipe;
	ml_pipeline_sink_h sink_handle;
	ml_pipeline_element_h elem_handle;
	double result_data[4];
	unsigned int epochs;
	gchar *model_save_path;
	gchar *model_load_path;
	gchar *init_model_path;
	unsigned version;
	gchar *model_name;
	gchar *resource_path[3];
} appdata_s;

/**
 * @brief get output from the tensor_sink and update label
 */
static void
get_output_from_tensor_sink(const ml_tensors_data_h data, const ml_tensors_info_h info, void *user_data)
{
	int ret = ML_ERROR_NONE;
	size_t data_size;
	double *output;
	appdata_s *ad = (appdata_s *) user_data;

	ret = ml_tensors_data_get_tensor_data(data, 0, (void **) &output, &data_size);
	if (ret != ML_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "ml_tensors_data_get_tensor_data failed");
		return;
	}

	for (int i = 0; i < 4; i++)
	{
		ad->result_data[i] = output[i];
	}

	dlog_print(DLOG_INFO, LOG_TAG,
			"[training_loss: %f, training_accuracy: %f, validation_loss: %f, validation_accuracy: %f]",
			ad->result_data[0], ad->result_data[1], ad->result_data[2], ad->result_data[3]);
	dlog_print(DLOG_INFO, LOG_TAG, "start >> ecore_pipe_write");
	ecore_pipe_write(ad->ecore_pipe, ad, (unsigned int)data_size);
	dlog_print(DLOG_INFO, LOG_TAG, "end >> ecore_pipe_write");
}

/**
 * @brief Destroy ml pipeline
 */
static void
destroy_ml_pipeline(appdata_s *ad)
{
	dlog_print(DLOG_INFO, LOG_TAG, "destroy_ml_pipeline 1");
	if (ad->sink_handle) {
		dlog_print(DLOG_INFO, LOG_TAG, "destroy_ml_pipeline 2");
		ml_pipeline_sink_unregister(ad->sink_handle);
	}
	if (ad->ml_pipe) {
		dlog_print(DLOG_INFO, LOG_TAG, "ml_pipeline_stop 3");
		ml_pipeline_stop(ad->ml_pipe);

		dlog_print(DLOG_INFO, LOG_TAG, "destroy_ml_pipeline 3");
		ml_pipeline_destroy(ad->ml_pipe);
	}
}

/**
 * @brief Create ml pipeline
 */
static void
create_ml_pipeline(appdata_s *ad)
{
	gchar *res_path = NULL;
	gchar *shared_path = NULL;
	gchar *pipeline_description = NULL;
	int ret = ML_ERROR_NONE;

	res_path = app_get_resource_path();
	shared_path = app_get_shared_data_path();
	ad->model_name = "new_mnist_nntrainer_model.bin";
	ad->model_save_path = g_strdup_printf("%s/%s", shared_path, ad->model_name);

	pipeline_description = g_strdup_printf(
			"datareposrc location=%s json=%s epochs=10 ! queue ! "
			"tensor_trainer name=tensor_trainer0 framework=nntrainer model-config=%s "
			"model-save-path=%s model-load-path=%s num-inputs=1 num-labels=1 "
			"num-training-samples=500 num-validation-samples=500 epochs=10 ! "
			"tensor_sink name=tensor_sink0 sync=true",
			ad->resource_path[0], ad->resource_path[1], ad->resource_path[2], ad->model_save_path, ad->model_load_path
			);

	ret = ml_pipeline_construct(pipeline_description, NULL, NULL, &ad->ml_pipe);
	if (ret != ML_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Failed to construct ml pipeline");
		destroy_ml_pipeline(ad);
		goto error;
	}
	dlog_print(DLOG_INFO, LOG_TAG, "ml pipeline");
	ret = ml_pipeline_sink_register(ad->ml_pipe, "tensor_sink0", get_output_from_tensor_sink, ad, &ad->sink_handle);
	if (ret != ML_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Failed to register tensor_sink");
		destroy_ml_pipeline(ad);
		goto error;
	}

error:
	g_free(res_path);
	g_free(shared_path);
	g_free(ad->resource_path[0]);
	g_free(ad->resource_path[1]);
	g_free(ad->resource_path[2]);
	g_free(pipeline_description);

}

/**
 * @brief evas object smart callback
 */
static void
model_resist_button_cb(void *data, Evas_Object *obj, void *event_info)
{
	int ret = ML_ERROR_NONE;
	appdata_s *ad = data;
	unsigned int version;
	gchar *text1, *text2;

	if (ad->ml_pipe)
		destroy_ml_pipeline(ad);

	char *desc1 = "MNIST model from nntrainer, total_samples:1000, sample_size: 3176, ";
	char *desc2 = "other/tensors, format=static, framerate=30/1, dimensions=1:1:784:1.1:1:10:1, types=float32.float32";
	char *description = g_strdup_printf("%s %s", desc1, desc2);

	dlog_print(DLOG_INFO, LOG_TAG, "call >> ml_service_model_register");
	dlog_print(DLOG_INFO, LOG_TAG, "%s", ad->init_model_path);
	/* model_name is used as the key in this app */
	if (ad->init_model_path) {
		text1 = g_strdup_printf("<align=center>Initial model resist: %s</align></br><align=center>path:%s</align>", ad->model_name, ad->init_model_path);
		ret = ml_service_model_register(ad->model_name, ad->init_model_path, true, description, &version);
	} else {
		text1 = g_strdup_printf("<align=center>Trained model resist: %s</align></br><align=center>path:%s</align>", ad->model_name, ad->model_save_path);
		ret = ml_service_model_register(ad->model_name, ad->model_save_path, true, description, &version);
	}
	elm_object_text_set(ad->label1, text1);		
	if (ret != ML_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Failed to register model ret(%d)", ret);
		return;
	}
	dlog_print(DLOG_INFO, LOG_TAG, "leave >> ml_service_model_register");
	text2 = g_strdup_printf("<align=center>version: %u</align></br><align=center>description: %s</align></br><align=center> %s</align>", version, desc1, desc2);
	elm_object_text_set(ad->label2, text2);

	g_free(text1);
	dlog_print(DLOG_INFO, LOG_TAG, "leave >> ml_service_model_register 2");
	g_free(text2);
	dlog_print(DLOG_INFO, LOG_TAG, "leave >> ml_service_model_register 3");
	g_free(description);
	g_free(ad->init_model_path);
	ad->init_model_path = NULL;
	g_free(ad->model_save_path);
	ad->model_save_path = NULL;
}

/**
 * @brief evas object smart callback
 */
static void
get_resource_button_cb(void *data, Evas_Object *obj, void *event_info)
{
	int ret = ML_ERROR_NONE;
	appdata_s *ad = data;
	unsigned int i, length;
	gchar *text;
	ml_information_list_h resources_h;
	ml_information_h info_h;
	gchar *path_to_resource;

	ret = ml_service_resource_get ("mnist_data", &resources_h);
 	if (ret != ML_ERROR_NONE) {
 	  dlog_print(DLOG_ERROR, LOG_TAG, "Faild to get resource ret(%d)", ret);
	  return;
 	}

 	ret = ml_information_list_length (resources_h, &length);
	if (ret != ML_ERROR_NONE) {
 	  dlog_print(DLOG_ERROR, LOG_TAG, "Faild to get resource length(%d)", ret);
	  return;
 	}
    for (i = 0; i < length; i++) {
 		ret = ml_information_list_get (resources_h, i, &info_h);
 		if (ret != ML_ERROR_NONE) {
 	  		dlog_print(DLOG_ERROR, LOG_TAG, "Faild to get resource list(%d)", ret);
	  		return;
 		}
 	    ret = ml_information_get (info_h, "path", (void **) &path_to_resource);
	    if (ret != ML_ERROR_NONE) {
 	  		dlog_print(DLOG_ERROR, LOG_TAG, "Faild to get resource path(%d)", ret);
	  		return;
 		}
		ad->resource_path[i] = g_strdup(path_to_resource);
 	}

	text = g_strdup_printf("<align=center>registed resource<br/>data: %s</br>json: %s</br>model config:%s<align=center>",
		ad->resource_path[0], ad->resource_path[1], ad->resource_path[2]);
	elm_object_text_set(ad->label1, text);
	elm_object_text_set(ad->label2, NULL);
	g_free(text);

	ret = ml_information_list_destroy (resources_h);
}

/**
 * @brief evas object smart callback
 */
static void
resource_resist_button_cb(void *data, Evas_Object *obj, void *event_info)
{
	int ret = ML_ERROR_NONE;
	appdata_s *ad = data;
	unsigned int i = 0;
	gchar *resources[3];
	gchar *text;
	gchar *res_path;

	res_path = app_get_resource_path();
	resources[0] = g_strdup_printf("%s/%s", res_path, "mnist.data");
	resources[1] = g_strdup_printf("%s/%s", res_path, "mnist.json");
	resources[2] = g_strdup_printf("%s/%s", res_path, "mnist.ini");


	for (i = 0; i < 3; i++) {
		ret = ml_service_resource_add ("mnist_data", resources[i], "MNIST resource data");
 		if (ret != ML_ERROR_NONE) {
      		dlog_print(DLOG_ERROR, LOG_TAG, "Faild to regist resource ret(%d)", ret);
			return;
    	}
	}

	text = g_strdup_printf("<align=center>register resource<br/>data: %s</br>json: %s</br>model config:%s<align=center>",
		resources[0], resources[1], resources[2]);
	elm_object_text_set(ad->label1, text);
	elm_object_text_set(ad->label2, NULL);
	g_free(text);
	g_free(resources[0]);
	g_free(resources[1]);
	g_free(resources[2]);
	g_free(res_path);
}

/**
 * @brief evas object smart callback
 */
static void
get_model_button_cb(void *data, Evas_Object *obj, void *event_info)
{
	int ret = ML_ERROR_NONE;
	gchar *text1, *text2;
	appdata_s *ad = data;
	gchar *version_str = NULL;
	gchar *path = NULL;
	ml_information_h model_info = NULL;
	ad->model_load_path = NULL;

	dlog_print(DLOG_INFO, LOG_TAG, "Get model path: %s", ad->model_name);

	ret = ml_service_model_get_activated(ad->model_name, &model_info);
	if (ret == ML_ERROR_NONE && model_info != NULL) {
		ret = ml_information_get(model_info, "path", (void **)&path);
		if (ret == ML_ERROR_NONE) {
			dlog_print(DLOG_INFO, LOG_TAG, "Get registered activate model path: %s", path);
			ad->model_load_path = g_strdup(path);
		} else {
			dlog_print(DLOG_INFO, LOG_TAG, "Failed to get registered model path");
		}
		ret = ml_information_get(model_info, "version", (void **)&version_str);
		if (ret == ML_ERROR_NONE) {
			dlog_print(DLOG_INFO, LOG_TAG, "Get registered activate model version: %s", version_str);
			ad->version = g_ascii_strtoll(version_str, NULL, 10);
		} else {
			dlog_print(DLOG_INFO, LOG_TAG, "Failed to get registered model version");
		}
	} else {
		ad->model_load_path = NULL;
		dlog_print(DLOG_INFO, LOG_TAG, "There is not registered model: %s", ad->model_name);
	}

	ret = ml_information_destroy(model_info);
	if (ret != ML_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "Faild to destroy model info, ret(%d)", ret);

	text1 = g_strdup_printf("<align=center> get activated model<br> %s </align>", ad->model_load_path);
	dlog_print(DLOG_INFO, LOG_TAG, "text1: %s", text1);
	elm_object_text_set(ad->label2, NULL);
	elm_object_text_set(ad->label1, text1);
	if (ad->model_load_path) {
		text2 = g_strdup_printf("<align=center> version %d </align>", ad->version);
		dlog_print(DLOG_INFO, LOG_TAG, "text2: %s", text2);
		elm_object_text_set(ad->label2, text2);
		g_free(text2);
	}
	g_free(text1);
}

/**
 * @brief evas object smart callback
 */
static void
start_button_cb(void *data, Evas_Object *obj, void *event_info)
{
	int ret = ML_ERROR_NONE;
	appdata_s *ad = data;
	gchar *text;
	text = g_strdup_printf("<align=center> Training model with the imported model %s", ad->model_name);
	elm_object_text_set(ad->label2, text);
	g_free(text);

	create_ml_pipeline(ad);

	ret = ml_pipeline_start(ad->ml_pipe);
	if (ret != ML_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Failed to start ml pipeline ret(%d)", ret);
		return;
	}
}

/**
 * @brief evas object smart callback
 */
static void
stop_button_cb(void *data, Evas_Object *obj, void *event_info)
{
	int ret = ML_ERROR_NONE;
	appdata_s *ad = data;

	ret = ml_pipeline_element_get_handle(ad->ml_pipe, "tensor_trainer0", &ad->elem_handle);
	if (ret != ML_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Failed to get tensor_trainer handle");
		return;
	}

	ret = ml_pipeline_element_set_property_bool(ad->elem_handle, "ready-to-complete", TRUE);
	if (ret != ML_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Failed to set 'ready-to-complete' property");
		return;
	}

	if (ad->elem_handle) {
		ret = ml_pipeline_element_release_handle(ad->elem_handle);
		if (ret != ML_ERROR_NONE) {
			dlog_print(DLOG_INFO, LOG_TAG, "Failed to release elem_handle");
			return;
		}
	}
}

/**
 * @brief base code for basic UI app.
 */
static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
	ui_app_exit();
}

/**
 * @brief base code for basic UI app.
 */
static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	appdata_s *ad = data;
	/* Let window go to hide state. */
	elm_win_lower(ad->win);
}

/**
 * @brief Called when data is written to the pipe
 */
static void
ecore_pipe_cb(void *data, void *buffer, unsigned int nbyte)
{
	appdata_s *ad = data;
	dlog_print(DLOG_INFO, LOG_TAG, "start >> ecore_pipe_cb");
	gchar *text = g_strdup_printf(
		"<align=center>%d epochs = [training_loss: %f, training_accuracy: %f, validation_loss: %f, validation_accuracy: %f]</align>",
		ad->epochs++, ad->result_data[0], ad->result_data[1], ad->result_data[2], ad->result_data[3]);
	elm_object_text_set(ad->label1, text);
	g_free(text);
	dlog_print(DLOG_INFO, LOG_TAG, "end >> ecore_pipe_cb");
}


/**
 * @brief Creates essential UI objects.
 */
static void
create_base_gui(appdata_s *ad)
{
	/* Window */
	ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
	elm_win_autodel_set(ad->win, EINA_TRUE);

	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}

	evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
	eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

	/* Conformant */
	ad->conform = elm_conformant_add(ad->win);
	elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

	ad->box = elm_box_add(ad->conform);
	evas_object_size_hint_weight_set(ad->box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_show(ad->box);
	elm_object_content_set(ad->conform, ad->box);

	/* Label */
	ad->title_label = elm_label_add(ad->box);
	elm_object_text_set(ad->title_label, "<align=center><font_size=30>MNIST training</font></align>");
	elm_object_style_set(ad->title_label, "maker");
	evas_object_size_hint_weight_set(ad->title_label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(ad->title_label, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_min_set(ad->title_label, 100, 100);
	evas_object_show(ad->title_label);
	elm_box_pack_end(ad->box, ad->title_label);

	ad->label1 = elm_label_add(ad->box);
	evas_object_size_hint_weight_set(ad->label1, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(ad->label1, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_min_set(ad->label1, 100, 100);
	evas_object_show(ad->label1);
	elm_box_pack_end(ad->box, ad->label1);

	ad->label2 = elm_label_add(ad->box);
	evas_object_size_hint_weight_set(ad->label2, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(ad->label2, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_min_set(ad->label2, 100, 100);
	evas_object_show(ad->label2);
	elm_box_pack_end(ad->box, ad->label2);

	/* Button */
	ad->model_resist_button = elm_button_add(ad->box);
	//evas_object_size_hint_weight_set(ad->label1, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(ad->model_resist_button, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_text_set(ad->model_resist_button, "1. Model resist ( 1. Initial model or 7. Trained model )");
	evas_object_show(ad->model_resist_button);
	evas_object_smart_callback_add(ad->model_resist_button, "clicked", model_resist_button_cb, ad);
	elm_box_pack_end(ad->box, ad->model_resist_button);

	ad->resource_resist_button = elm_button_add(ad->box);
	evas_object_size_hint_align_set(ad->resource_resist_button, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_text_set(ad->resource_resist_button, "2. Resource resist");
	evas_object_show(ad->resource_resist_button);
	evas_object_smart_callback_add(ad->resource_resist_button, "clicked", resource_resist_button_cb, ad);
	elm_box_pack_end(ad->box, ad->resource_resist_button);

	ad->get_model_button = elm_button_add(ad->box);
	evas_object_size_hint_align_set(ad->get_model_button, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_text_set(ad->get_model_button, "3. Get registerd model (8)");
	evas_object_show(ad->get_model_button);
	evas_object_smart_callback_add(ad->get_model_button, "clicked", get_model_button_cb, ad);
	elm_box_pack_end(ad->box, ad->get_model_button);

	ad->get_resource_button = elm_button_add(ad->box);
	evas_object_size_hint_align_set(ad->get_resource_button, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_text_set(ad->get_resource_button, "4. Get registred resource");
	evas_object_show(ad->get_resource_button);
	evas_object_smart_callback_add(ad->get_resource_button, "clicked", get_resource_button_cb, ad);
	elm_box_pack_end(ad->box, ad->get_resource_button);

	ad->start_button = elm_button_add(ad->box);
	evas_object_size_hint_align_set(ad->start_button, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_text_set(ad->start_button, "5. Start model training");
	evas_object_show(ad->start_button);
	evas_object_smart_callback_add(ad->start_button, "clicked", start_button_cb, ad);
	elm_box_pack_end(ad->box, ad->start_button);

	ad->stop_button = elm_button_add(ad->box);
	evas_object_size_hint_align_set(ad->stop_button, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_text_set(ad->stop_button, "6. Ready to complete");
	evas_object_show(ad->stop_button);
	evas_object_smart_callback_add(ad->stop_button, "clicked", stop_button_cb, ad);
	elm_box_pack_end(ad->box, ad->stop_button);

	ad->exit_button = elm_button_add(ad->box);
	evas_object_size_hint_align_set(ad->exit_button, EVAS_HINT_FILL, EVAS_HINT_FILL);
	elm_object_text_set(ad->exit_button, "Exit");
	evas_object_show(ad->exit_button);
	evas_object_smart_callback_add(ad->exit_button, "clicked", win_delete_request_cb, ad);
	elm_box_pack_end(ad->box, ad->exit_button);
	/* Show window after base gui is set up */
	evas_object_show(ad->win);

	/* Add ecore_pipe */
	ad->ecore_pipe = ecore_pipe_add((Ecore_Pipe_Cb)ecore_pipe_cb, ad);
	if (ad->ecore_pipe) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Failed to register callback");
	}
}

/**
 * @brief base code for basic UI app.
 */
static bool
app_create(void *data)
{
	appdata_s *ad = data;
	gchar *res_path = NULL;
	/* initial name for getting model */
	res_path = app_get_resource_path();
	ad->model_name = "init_mnist_nntrainer_model.bin";
	ad->init_model_path = g_strdup_printf("%s/%s", res_path, ad->model_name);

	create_base_gui(ad);

	g_free(res_path);
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
	appdata_s *ad = data;
	ml_pipeline_stop(ad->ml_pipe);
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
  appdata_s *ad = data;
  dlog_print(DLOG_INFO, LOG_TAG, "app_terminate...");
  destroy_ml_pipeline(ad);
  ecore_pipe_del(ad->ecore_pipe);
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

	ui_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

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
