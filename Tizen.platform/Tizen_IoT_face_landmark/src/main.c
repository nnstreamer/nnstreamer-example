/**
 * @file main.c
 * @date 8 April 2022
 * @brief nnstreamer example application for face landmark on tizen platform
 * @see  https://github.com/nnsuite/nnstreamer
 * @author Hyunil Park <hyunil46.park@samsung.com>
 * @bug No known bugs
 *
 * title: Face landmark
 *
 * model link : https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_landmark/face_landmark.tflite
 *
 * model type : Convolutional Neural Network
 *
 * input: floate32[1,192,192,3]
 *        Captured frame should contain a single face placed in the center of the image. 
 *        there should be a margin around the face calculated ad 25% of face size.
 *
 * output: floate32[1,1,1,1404]
 *         Facial surface represented as 468 3D landmarks flattened into a 1D tensor:(x1,y1,z1),(x2,y2,z2)...
 *         x- and y-coordinates follow the image pixel coordinates; z-coordinates are relative to the
 *         face center of mass and are scaled proportionally to the face width.
 *         The points have been manually selected in accordance with the supposed applications,
 *         such as expressive AR effects, virtual accessory and apparel try-on and makeup.
 * 
 * output: floate32[1,1,1,1]
 *         Face flag indicating the likelihood of the face being present in the input image.
 *         Used in tracking mode to detect that the face was lost and the face detector should be applied to
 *         obtain a new face position.
*/
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <math.h>
#include <appcore-efl.h>
#include <Elementary.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Ecore_Wl2.h>
#include <tizen-extension-client-protocol.h>
#define PACKAGE "face-landmarks"
#define FACE_FLAG_THRESHOLD 120
#define NUM_OF_LANDMARK 468

/**
 * @brief tflite model info structure
 */
typedef struct
{
	gchar *model_path;
}tflite_info_s;

/**
 * @brief coordinates structure
 */
typedef struct
{
	float x;
	float y;
	float z;
}point_s;

/**
 * @brief app data structure
 */
typedef struct
{
	Evas_Object *win;
	gint parent_id;
	GstBus *bus;
	GstElement *pipeline;
	tflite_info_s tflite_info;
	point_s point[NUM_OF_LANDMARK];
	float face_flag;
}app_data_s;

/**
 * @brief app data variable
 */
static app_data_s ad;

/**
 * @brief create window
 */
static Evas_Object *create_win(char *name)
{
	Evas_Object *win = NULL;
	win  = elm_win_add(NULL, name, ELM_WIN_SPLASH);
	if (win) {
		elm_win_borderless_set(win, EINA_TRUE);
		elm_win_fullscreen_set(win, EINA_TRUE);
		elm_win_autodel_set(win, EINA_TRUE);
		elm_win_alpha_set(win, EINA_TRUE);
		elm_win_activate(win);
		evas_object_show(win);
	}
	return win;
}

/**
 * @brief wayland registry listener callback
 */
static void global(void *data, struct wl_registry *registry,
	uint32_t name, const char *interface, uint32_t version)
{
	struct tizen_surface **tz_surface = NULL;

	if (!data) {
		g_print("NULL data \n");
		return;
	}

	tz_surface = (struct tizen_surface **)data;

	if (!interface) {
		g_print("NULL interface \n");
		return;
	}

	if (strcmp(interface, "tizen_surface") == 0) {
		g_print("binding tizen surface for wayland \n");

		*tz_surface = wl_registry_bind(registry, name, &tizen_surface_interface, version);
		if (*tz_surface == NULL)
			g_print("failed to bind \n");

		g_print("done");
	}
	return;
}

/**
 * @brief  wayland registry listener callback
 */
static void global_remove(void *data, struct wl_registry *wl_registry, uint32_t name)
{
	g_print("enter \n");
	return;
}

/**
 * @brief wayland registry listener
 */
static const struct wl_registry_listener wl_registry_listener = {
	global,
	global_remove
};

/**
 * @brief tizen resource listener callback
 */
static void parent_id_getter(void *data, struct tizen_resource *tizen_resource, uint32_t id)
{
	if (!data) {
		g_print("NULL data \n");
		return;
	}

	*((unsigned int *)data) = id;

	g_print("[CLIENT] got parent_id [%u] from server \n", id);

	return;
}

/**
 * @brief tizen resource listener
 */
static const struct tizen_resource_listener tz_resource_listener = {
	parent_id_getter
};

/**
 * @brief get window id
 */
static void get_window_id(void *handle, int *parent_id)
{
	Ecore_Wl2_Window *wl2_window = NULL;
	Ecore_Wl2_Display *wl2_display = NULL;
	struct wl_display *display = NULL;
	struct wl_surface *surface = NULL;
	struct wl_registry *registry = NULL;
	struct tizen_surface *tz_surface = NULL;
	struct tizen_resource *tz_resource = NULL;

	if (!handle || !parent_id) {
		g_print("NULL parameter %p %p \n", handle, parent_id);
		return;
	}
	wl2_window = ecore_evas_wayland2_window_get(ecore_evas_ecore_evas_get(evas_object_evas_get((Evas_Object *)handle)));
	if (!wl2_window) {
		g_print("failed to get wayland window\n");
		goto _DONE;
	}

	/* set video_has flag to a video application window */
	ecore_wl2_window_video_has(wl2_window, EINA_TRUE);

	surface = (struct wl_surface *)ecore_wl2_window_surface_get(wl2_window);
	if (!surface) {
		g_print("failed to get wayland surface \n");
		goto _DONE;
	}

	wl2_display = ecore_wl2_connected_display_get(NULL);
	if (!wl2_display) {
		g_print("failed to get wl2 display \n");
		goto _DONE;
	}

	display = (struct wl_display *)ecore_wl2_display_get(wl2_display);
	if (!display) {
		g_print("failed to get wayland display \n");
		goto _DONE;
	}

	registry = wl_display_get_registry(display);
	if (!registry) {
		g_print("failed to get wayland registry \n");
		goto _DONE;
	}

	wl_registry_add_listener(registry, &wl_registry_listener, &tz_surface);

	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (!tz_surface) {
		g_print("failed to get tizen surface \n");
		goto _DONE;
	}

	/* Get parent_id which is unique in a entire system. */
	tz_resource = tizen_surface_get_tizen_resource(tz_surface, surface);
	if (!tz_resource) {
		g_print("failed to get tizen resource \n");
		goto _DONE;
	}

	*parent_id = 0;

	tizen_resource_add_listener(tz_resource, &tz_resource_listener, parent_id);

	wl_display_roundtrip(display);

	if (*parent_id > 0) {
		g_print("parent id : %d \n", *parent_id);
	} else {
		g_print("failed to get parent id \n");
	}

_DONE:
	if (tz_surface) {
		tizen_surface_destroy(tz_surface);
		tz_surface = NULL;
	}

	if (tz_resource) {
		tizen_resource_destroy(tz_resource);
		tz_resource = NULL;
	}

	if (registry) {
		wl_registry_destroy(registry);
		registry = NULL;
	}
}

/**
 * @brief message callback
 */
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
	gchar *debug;
	GError *error;

	switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_EOS:
			g_print("End of stream\n");
			break;
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &error, &debug);
			g_free(debug);

			g_print("Error: %s\n", error->message);
			g_error_free(error);
			break;
		default:
			break;
	}
	return TRUE;
}

/**
 * @brief check tensorflow model
 */
static gboolean check_tflite_model(tflite_info_s *tflite_info)
{
	g_return_val_if_fail(tflite_info != NULL, FALSE);

	if (access(tflite_info->model_path, F_OK) !=0) {
		g_print("Error: can not find model [%s]\n", tflite_info->model_path);
		return FALSE;
	}
	return TRUE;
}

/**
 * @brief update coordinates
 */
static void __update_coordinates(float *data, guint len, gpointer user_data)
{
	app_data_s *app_data = NULL;
	app_data = user_data;
	int count = 0;

	g_return_if_fail(app_data != NULL);
	g_return_if_fail(data != NULL);

	/** The points have been manually selected in accordance with
	    the supposed applications, such as expressive AR effects,
	    virtual accessory and apparel try-on and makeup. */
	for (int i = 0; i < len; i=i+3){
		app_data->point[count].x = data[i];
		app_data->point[count].y = data[i+1];
		app_data->point[count].z = data[i+2];
		count++;
	}
}

/**
 * @brief update face flag
 */
static void __update_face_flag(float *data, guint len, gpointer user_data)
{
	app_data_s *app_data = NULL;
	app_data = user_data;

	g_return_if_fail(app_data != NULL);
	g_return_if_fail(data != NULL);

	app_data->face_flag = data[0];
	g_print("face_flag [%f] \n", app_data->face_flag);
}

/**
 * @brief new-data callback of tensor_sink
 */
static void __new_data_cb (GstElement *element, GstBuffer *gstbuffer, gpointer user_data)
{
	app_data_s *app_data = NULL;
	GstMemory *mem = NULL;
	GstMapInfo info;
	guint i;

	app_data = user_data;
	if (!app_data) {		
		g_print("Error: app_data is NULL \n");
		return;
	}

	/* Repeat as time as model's number of output */
	for (i = 0; i < gst_buffer_n_memory(gstbuffer); i++) {
		mem = gst_buffer_peek_memory(gstbuffer, i);
		if (mem == NULL) {
			g_print("Faild peek memory \n");
			return;
		}
		if (gst_memory_map(mem, &info, GST_MAP_READ)) {
			int len = info.size/4;
			if (i) {
				__update_face_flag(info.data, len, app_data);
			} else {
				__update_coordinates(info.data, len, app_data);
			}
			gst_memory_unmap(mem, &info);
		}
	}	
}

/**
 * @brief draw callback of cairooverlay
 */
static void __draw_cb(GstElement *overlay, cairo_t *cr, guint64 timestamp,
	guint64 duration, gpointer user_data)
{
	app_data_s *app_data = NULL;
	app_data = user_data;
	g_return_if_fail(app_data != NULL);      
	
	if (app_data->face_flag > FACE_FLAG_THRESHOLD) {
		cairo_set_source_rgba(cr, 0, 1.0, 0, 1.0);
		for (int i = 0; i < NUM_OF_LANDMARK; i++) {
			float x = app_data->point[i].x;
			float y = app_data->point[i].y;
			cairo_arc(cr, x, y, 0.1, 0, 2*M_PI);
			cairo_stroke(cr);
		}
		cairo_fill(cr);
	}
}

/**
 * @brief app create
 */
static int app_create(void *data)
{
	gchar *pipeline_description = NULL;
	app_data_s *app_data = NULL;
	GstElement *tensor_sink = NULL;
	GstElement *cario_overlay = NULL;
	GstElement *videosink = NULL;
	Evas_Object *win = NULL;

	app_data = data;
	if (!app_data) {
		g_print("Error: app_data is NULL \n");
		return 0;
	}

	/*create window*/
	win = create_win(PACKAGE);
	if (!win) {
		g_print("Error: win is NULL \n");
	}
	app_data->win = win;

	/* get window id*/
	get_window_id(app_data->win, &app_data->parent_id);
	g_print("get window id: %d \n", app_data->parent_id);

	/* set model path */
	app_data->tflite_info.model_path = g_strdup("/opt/usr/home/owner/face_landmark.tflite");

	/* check tflite model file */
	if (check_tflite_model(&app_data->tflite_info) != TRUE)
		return FALSE;

	/* gstreamer init */
	gst_init(NULL, NULL);

	/* RPI4 with common profile */
	pipeline_description = g_strdup_printf
	("v4l2src ! videoconvert ! video/x-raw,width=320,height=240,format=RGB ! videocrop top=48 left=128 !videoscale ! video/x-raw,width=192,height=192,format=RGB ! tee name=t_raw  t_raw. ! queue "
	 "! videoconvert ! cairooverlay name=cairo_overlay ! tizenwlsink name=videosink t_raw. ! queue leaky=2 max-size-buffers=2 ! tensor_converter "
	 "! tensor_transform mode=typecast option=float32 ! tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=tensor_sink", app_data->tflite_info.model_path);

	/** TM1 ref target with mobile profile
	pipeline_description = g_strdup_printf
	("camerasrc camera-id = 1 ! video/x-raw,width=320,height=240,format=SN12 ! videoconvert ! video/x-raw,width=320,height=240,format=RGB ! videocrop top=48 left=128 ! videoscale ! video/x-raw,width=192,height=192,format=RGB ! videoflip video-direction=3 ! tee name=t_raw  t_raw. ! queue "
	 "! videoconvert ! cairooverlay name=cairo_overlay ! tizenwlsink name=videosink t_raw. ! queue leaky=2 max-size-buffers=2 ! tensor_converter "
	 "! tensor_transform mode=typecast option=float32 ! tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=tensor_sink", app_data->tflite_info.model_path);
	*/

	g_print("pipe : %s \n", pipeline_description);

	g_free(app_data->tflite_info.model_path);

	app_data->pipeline = gst_parse_launch(pipeline_description, NULL);
	if (!app_data->pipeline) {
		g_print("Error: Failed to launch \n");
		return 0;
	}
	g_free(pipeline_description);

	app_data->bus = gst_pipeline_get_bus(GST_PIPELINE(app_data->pipeline));
	if (!app_data->bus) {
		g_print("Error: Failed to get bus \n");
		return 0;
	}
	gst_bus_add_watch(app_data->bus, bus_call, NULL);
	gst_object_unref(app_data->bus);

	/* set display id */
	videosink = gst_bin_get_by_name(GST_BIN(app_data->pipeline), "videosink");
	gst_video_overlay_set_wl_window_wl_surface_id(GST_VIDEO_OVERLAY(videosink), app_data->parent_id);

	/* set display roi for setting good camera angle */
	g_object_set(GST_OBJECT(videosink), "display-geometry-method", 5,
			"display-roi-x", 0,"display-roi-y", 0, "display-roi-width", 256,"display-roi-height", 256, NULL);
	gst_object_unref(GST_OBJECT(videosink));

	/* add new-data callback to tensorsink */
	tensor_sink = gst_bin_get_by_name(GST_BIN(app_data->pipeline), "tensor_sink");
	g_signal_connect(GST_OBJECT(tensor_sink), "new-data", G_CALLBACK(__new_data_cb), app_data);
	gst_object_unref(GST_OBJECT(tensor_sink));

	/* add draw callback to cairooverlay */
	cario_overlay = gst_bin_get_by_name(GST_BIN(app_data->pipeline), "cairo_overlay");
	g_signal_connect(GST_OBJECT(cario_overlay), "draw", G_CALLBACK(__draw_cb), app_data);
	gst_object_unref(GST_OBJECT(cario_overlay));

	gst_element_set_state(app_data->pipeline, GST_STATE_PLAYING);

	return 0;
}

/**
 * @brief app terminate
 */
static int app_terminate(void *data)
{
	app_data_s *app_data = NULL;

	app_data = data;
	if (!app_data) {
		g_print("Error: app_data is NULL \n");
		return 0;
	}

	if (app_data->pipeline) {
		gst_element_set_state(app_data->pipeline, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(app_data->pipeline));

		if(app_data->bus)
			gst_bus_remove_watch(app_data->bus);
	}

	if (app_data->tflite_info.model_path) {
		g_free(app_data->tflite_info.model_path);
		app_data->tflite_info.model_path = NULL;
	}

	if (app_data->win) {
		evas_object_del(app_data->win);
	}

	return 0;
}

/**
 * @brief appcore struct
 */
struct appcore_ops ops = {
	.create = app_create,
	.terminate = app_terminate,
};

/**
 * @brief main function
 */
int main(int argc, char *argv[])
{
	memset(&ad, 0x0, sizeof(app_data_s));
	ops.data = &ad;

	return appcore_efl_main(PACKAGE, &argc, &argv, &ops);
}
