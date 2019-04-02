/**
 * @file	nnstreamer-jni.cpp
 * @date	1 April 2019
 * @brief	Tensor stream example with TF-Lite model for object detection
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @bug		No known bugs
 */

#include <string.h>
#include <stdint.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <cairo/cairo.h>
#include <pthread.h>
#include "nnstreamer-ssd.h"

#define TAG_NAME "NNStreamer-SSD"

GST_DEBUG_CATEGORY_STATIC (debug_category);
#define GST_CAT_DEFAULT debug_category

/**
 * These macros provide a way to store the native pointer to CustomData,
 * which might be 32 or 64 bits, into a jlong, which is always 64 bits, without warnings.
 */
#if GLIB_SIZEOF_VOID_P == 8
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(*env)->GetLongField (env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)data)
#else
# define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(jint)(*env)->GetLongField (env, thiz, fieldID)
# define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)(jint)data)
#endif

/**
 * @brief Structure to contain all our information, so we can pass it to callbacks
 */
typedef struct _CustomData
{
  jobject app;                  /**< Application instance, used to call its methods. A global reference is kept. */
  GstElement *pipeline;         /**< The running pipeline */
  GMainContext *context;        /**< GLib context used to run the main loop */
  GMainLoop *main_loop;         /**< GLib main loop */
  gboolean initialized;         /**< To avoid informing the UI multiple times about the initialization */
  GstElement *video_sink;       /**< The video sink element which receives XOverlay commands */
  ANativeWindow *native_window; /**< The Android native window where video will be rendered */
  int media_width;              /**< The video width */
  int media_height;             /**< The video height */
} CustomData;

/* These global variables cache values which are not changing during execution */
static pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
static JavaVM *java_vm;
static jfieldID custom_data_field_id;
static jmethodID set_message_method_id;
static jmethodID on_gstreamer_initialized_method_id;

/**
 * @brief Register this thread with the VM
 */
static JNIEnv *
attach_current_thread (void)
{
  JNIEnv *env;
  JavaVMAttachArgs args;

  GST_DEBUG ("Attaching thread %p", g_thread_self ());
  args.version = JNI_VERSION_1_4;
  args.name = NULL;
  args.group = NULL;

  if ((*java_vm)->AttachCurrentThread (java_vm, &env, &args) < 0) {
    GST_ERROR ("Failed to attach current thread");
    return NULL;
  }

  return env;
}

/**
 * @brief Unregister this thread from the VM
 */
static void
detach_current_thread (void *env)
{
  GST_DEBUG ("Detaching thread %p", g_thread_self ());
  (*java_vm)->DetachCurrentThread (java_vm);
}

/**
 * @brief Retrieve the JNI environment for this thread
 */
static JNIEnv *
get_jni_env (void)
{
  JNIEnv *env;

  if ((env = pthread_getspecific (current_jni_env)) == NULL) {
    env = attach_current_thread ();
    pthread_setspecific (current_jni_env, env);
  }

  return env;
}

/**
 * @brief Change the content of the UI's TextView
 */
static void
set_ui_message (const gchar * message, CustomData * data)
{
  JNIEnv *env;
  jstring jmessage;

  GST_DEBUG ("Setting message to: %s", message);

  env = get_jni_env ();
  jmessage = (*env)->NewStringUTF (env, message);

  (*env)->CallVoidMethod (env, data->app, set_message_method_id, jmessage);

  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to call Java method");
    (*env)->ExceptionClear (env);
  }

  (*env)->DeleteLocalRef (env, jmessage);
}

/**
 * @brief Retrieve errors from the bus and show them on the UI
 */
static void
error_cb (GstBus * bus, GstMessage * msg, CustomData * data)
{
  GError *err;
  gchar *debug_info;
  gchar *message;

  gst_message_parse_error (msg, &err, &debug_info);
  message =
      g_strdup_printf ("Error received from element %s: %s",
      GST_OBJECT_NAME (msg->src), err->message);
  g_clear_error (&err);
  g_free (debug_info);

  __android_log_print (ANDROID_LOG_DEBUG, TAG_NAME, "%s", message);
  set_ui_message (message, data);
  g_free (message);

  gst_element_set_state (data->pipeline, GST_STATE_NULL);
}

/**
 * @brief Notify UI about pipeline state changes
 */
static void
state_changed_cb (GstBus * bus, GstMessage * msg, CustomData * data)
{
  GstState old_state, new_state, pending_state;

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

  /* Only pay attention to messages coming from the pipeline, not its children */
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->pipeline)) {
    gchar *message = g_strdup_printf ("State changed to %s",
        gst_element_state_get_name (new_state));

    __android_log_print (ANDROID_LOG_DEBUG, TAG_NAME, "%s", message);
#if 0 /* Skip to notify state changes */
    set_ui_message (message, data);
#endif
    g_free (message);
  }
}

/**
 * @brief Check if all conditions are met to report GStreamer as initialized.
 * These conditions will change depending on the application.
 */
static void
check_initialization_complete (CustomData * data)
{
  JNIEnv *env = get_jni_env ();

  if (!data->initialized && data->native_window && data->main_loop) {
    GST_DEBUG
        ("Initialization complete, notifying application. native_window:%p main_loop:%p",
        data->native_window, data->main_loop);

    /* The main loop is running and we received a native window, inform the sink about it */
    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->video_sink),
        (guintptr) data->native_window);

    (*env)->CallVoidMethod (env, data->app, on_gstreamer_initialized_method_id);
    if ((*env)->ExceptionCheck (env)) {
      GST_ERROR ("Failed to call Java method");
      (*env)->ExceptionClear (env);
    }

    data->initialized = TRUE;
  }
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
new_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  GstMemory *mem_boxes, *mem_detections;
  GstMapInfo info_boxes, info_detections;
  gfloat *boxes, *detections;

  if (gst_buffer_n_memory (buffer) != 2) {
    GST_ERROR ("Invalid result, the number of memory blocks is different.");
    return;
  }

  /* boxes */
  mem_boxes = gst_buffer_get_memory (buffer, 0);
  gst_memory_map (mem_boxes, &info_boxes, GST_MAP_READ);
  boxes = (gfloat *) info_boxes.data;

  /* detections */
  mem_detections = gst_buffer_get_memory (buffer, 1);
  gst_memory_map (mem_detections, &info_detections, GST_MAP_READ);
  detections = (gfloat *) info_detections.data;

  ssd_update_result (detections, boxes);

  gst_memory_unmap (mem_boxes, &info_boxes);
  gst_memory_unmap (mem_detections, &info_detections);

  gst_memory_unref (mem_boxes);
  gst_memory_unref (mem_detections);
}

/**
 * @brief Callback to draw the overlay.
 */
static void
draw_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp,
    guint64 duration, gpointer user_data)
{
  CustomData *data = (CustomData *) user_data;
  guint max_objects = MAX_OBJECT_DETECTION;
  ssd_detected_object_s objects[MAX_OBJECT_DETECTION];
  guint i;
  gfloat x, y, width, height;
  gchar *label;

  if (!data)
      return;

  /* set font props */
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, 20.0);

  max_objects = ssd_get_detected_objects (objects, max_objects);

  for (i = 0; i < max_objects; ++i) {
    if (!ssd_get_label (objects[i].class_id, &label)) {
      GST_ERROR ("Failed to get label (class id %d)", objects[i].class_id);
      continue;
    }

    x = objects[i].x * data->media_width / MODEL_WIDTH;
    y = objects[i].y * data->media_height / MODEL_HEIGHT;
    width = objects[i].width * data->media_width / MODEL_WIDTH;
    height = objects[i].height * data->media_height / MODEL_HEIGHT;

    /* draw rectangle */
    cairo_rectangle (cr, x, y, width, height);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_set_line_width (cr, 1.5);
    cairo_stroke (cr);
    cairo_fill_preserve (cr);

    /* draw title */
    cairo_move_to (cr, x + 5, y + 25);
    cairo_text_path (cr, label);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_set_line_width (cr, .3);
    cairo_stroke (cr);
    cairo_fill_preserve (cr);
  }
}

/**
 * @brief Main method for the native code. This is executed on its own thread.
 */
static void *
app_function (void *userdata)
{
  GstBus *bus;
  GstElement *element;
  CustomData *data = (CustomData *) userdata;
  GSource *bus_source;
  GError *error = NULL;
  gchar *str_pipeline, *model_path;

  GST_DEBUG ("Creating pipeline in CustomData at %p", data);

  /* Create our own GLib Main Context and make it the default one */
  data->context = g_main_context_new ();
  g_main_context_push_thread_default (data->context);

  /* Build pipeline, video resolution 640x480, scaled 300x300 for SSD model */
  if (!ssd_get_model_path (&model_path)) {
    __android_log_print (ANDROID_LOG_ERROR, TAG_NAME,
        "Failed to get SSD model path");
    return NULL;
  }

  str_pipeline = g_strdup_printf
      ("ahc2src ! videoconvert ! video/x-raw,format=RGB,width=%d,height=%d,framerate=30/1 ! tee name=traw "
      "traw. ! queue ! videoconvert ! cairooverlay name=res_overlay ! autovideosink "
      "traw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw,format=RGB,width=%d,height=%d ! tensor_converter ! "
      "tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
      "tensor_filter framework=tensorflow-lite model=%s ! tensor_sink name=res_sink",
      data->media_width, data->media_height, MODEL_WIDTH, MODEL_HEIGHT,
      model_path);
  data->pipeline = gst_parse_launch (str_pipeline, &error);
  g_free (str_pipeline);

  if (error) {
    gchar *message =
        g_strdup_printf ("Unable to build pipeline: %s", error->message);
    g_clear_error (&error);

    __android_log_print (ANDROID_LOG_DEBUG, TAG_NAME, "%s", message);
    set_ui_message (message, data);
    g_free (message);
    return NULL;
  }

  /**
   * tensor_sink new-data signal
   * tensor_sink emits the signal when new buffer incomes to the sink pad.
   * The application can connect this signal with the callback.
   * Please be informed that, the memory blocks in the buffer object passed from tensor_sink is available only in this callback function.
   */
  element = gst_bin_get_by_name (GST_BIN (data->pipeline), "res_sink");
  g_signal_connect (element, "new-data", G_CALLBACK (new_data_cb), data);
  gst_object_unref (element);

  /* cairooverlay draw */
  element = gst_bin_get_by_name (GST_BIN (data->pipeline), "res_overlay");
  g_signal_connect (element, "draw", G_CALLBACK (draw_overlay_cb), data);
  gst_object_unref (element);

  /* Set the pipeline to READY, so it can already accept a window handle, if we have one */
  gst_element_set_state (data->pipeline, GST_STATE_READY);

  data->video_sink =
      gst_bin_get_by_interface (GST_BIN (data->pipeline),
      GST_TYPE_VIDEO_OVERLAY);
  if (!data->video_sink) {
    GST_ERROR ("Could not retrieve video sink");
    return NULL;
  }

  /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
  bus = gst_element_get_bus (data->pipeline);
  bus_source = gst_bus_create_watch (bus);
  g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func,
      NULL, NULL);
  g_source_attach (bus_source, data->context);
  g_source_unref (bus_source);
  g_signal_connect (G_OBJECT (bus), "message::error", (GCallback) error_cb,
      data);
  g_signal_connect (G_OBJECT (bus), "message::state-changed",
      (GCallback) state_changed_cb, data);
  gst_object_unref (bus);

  /* Create a GLib Main Loop and set it to run */
  GST_DEBUG ("Entering main loop... (CustomData:%p)", data);
  data->main_loop = g_main_loop_new (data->context, FALSE);
  check_initialization_complete (data);
  g_main_loop_run (data->main_loop);

  GST_DEBUG ("Exited main loop");
  g_main_loop_unref (data->main_loop);
  data->main_loop = NULL;

  /* Free resources */
  g_main_context_pop_thread_default (data->context);
  g_main_context_unref (data->context);
  gst_element_set_state (data->pipeline, GST_STATE_NULL);

  g_usleep (100 * 1000);
  gst_object_unref (data->video_sink);
  gst_object_unref (data->pipeline);

  return NULL;
}

/**
 * @brief Instruct the native code to create its internal data structure, pipeline and thread.
 */
static void
gst_native_init (JNIEnv * env, jobject thiz, jint media_w, jint media_h)
{
  CustomData *data = g_new0 (CustomData, 1);

  SET_CUSTOM_DATA (env, thiz, custom_data_field_id, data);

  GST_DEBUG_CATEGORY_INIT (debug_category, TAG_NAME, 0,
      "Android nnstreamer example with tf-lite SSD model");
  gst_debug_set_threshold_for_name (TAG_NAME, GST_LEVEL_DEBUG);

  GST_DEBUG ("Created CustomData at %p", data);

  data->app = (*env)->NewGlobalRef (env, thiz);
  GST_DEBUG ("Created GlobalRef for app object at %p", data->app);

  /* set media resolution */
  data->media_width = (int) media_w;
  data->media_height = (int) media_h;

  /* register ahc2src */
  GST_PLUGIN_STATIC_REGISTER (ahc2src);

  /* register nnstreamer plugins */
  GST_PLUGIN_STATIC_REGISTER (nnstreamer);

  /* filter tensorflow-lite sub-plugin */
  init_filter_tflite ();

  /* initialize SSD model info */
  ssd_init ();

  pthread_create (&gst_app_thread, NULL, &app_function, data);
}

/**
 * @brief Quit the main loop, remove the native thread and free resources.
 */
static void
gst_native_finalize (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  GST_DEBUG ("Quitting main loop...");
  g_main_loop_quit (data->main_loop);

  GST_DEBUG ("Waiting for thread to finish...");
  pthread_join (gst_app_thread, NULL);

  GST_DEBUG ("Deleting GlobalRef for app object at %p", data->app);
  (*env)->DeleteGlobalRef (env, data->app);
  GST_DEBUG ("Freeing CustomData at %p", data);

  g_free (data);
  SET_CUSTOM_DATA (env, thiz, custom_data_field_id, NULL);

  /* free SSD model info */
  ssd_free ();

  /* filter tensorflow-lite sub-plugin */
  fini_filter_tflite ();

  GST_DEBUG ("Done finalizing");
}

/**
 * @brief Set pipeline to PLAYING state.
 */
static void
gst_native_play (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  GST_DEBUG ("Setting state to PLAYING");
  gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
}

/**
 * @brief Set pipeline to PAUSED state
 */
static void
gst_native_pause (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  GST_DEBUG ("Setting state to PAUSED");
  gst_element_set_state (data->pipeline, GST_STATE_PAUSED);
}

/**
 * @brief Static class initializer: retrieve method and field IDs.
 */
static jboolean
gst_native_class_init (JNIEnv * env, jclass klass)
{
  custom_data_field_id =
      (*env)->GetFieldID (env, klass, "native_custom_data", "J");
  set_message_method_id =
      (*env)->GetMethodID (env, klass, "setMessage", "(Ljava/lang/String;)V");
  on_gstreamer_initialized_method_id =
      (*env)->GetMethodID (env, klass, "onGStreamerInitialized", "()V");

  if (!custom_data_field_id || !set_message_method_id ||
      !on_gstreamer_initialized_method_id) {
    /**
     * We emit this message through the Android log instead of the GStreamer log
     * because the later has not been initialized yet.
     */
    __android_log_print (ANDROID_LOG_ERROR, TAG_NAME,
        "The calling class does not implement all necessary interface methods");
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

/**
 * @brief Callback surfaceChanged
 */
static void
gst_native_surface_init (JNIEnv * env, jobject thiz, jobject surface)
{
  CustomData *data;
  ANativeWindow *new_native_window;

  data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  new_native_window = ANativeWindow_fromSurface (env, surface);

  GST_DEBUG ("Received surface %p (native window %p)", surface,
      new_native_window);

  if (data->native_window) {
    if (data->native_window == new_native_window) {
      GST_DEBUG ("New native window is the same as the previous one %p",
          data->native_window);
      if (data->video_sink) {
        gst_video_overlay_expose (GST_VIDEO_OVERLAY (data->video_sink));
        gst_video_overlay_expose (GST_VIDEO_OVERLAY (data->video_sink));
      }
      return;
    } else {
      GST_DEBUG ("Released previous native window %p", data->native_window);
      ANativeWindow_release (data->native_window);
      data->initialized = FALSE;
    }
  }

  data->native_window = new_native_window;
  check_initialization_complete (data);
}

/**
 * @brief Callback surfaceDestroyed
 */
static void
gst_native_surface_finalize (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  GST_DEBUG ("Releasing Native Window %p", data->native_window);

  if (data->video_sink) {
    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->video_sink),
        (guintptr) NULL);
    gst_element_set_state (data->pipeline, GST_STATE_READY);
  }

  ANativeWindow_release (data->native_window);
  data->native_window = NULL;
  data->initialized = FALSE;
}

/**
 * @brief List of implemented native methods
 */
static JNINativeMethod native_methods[] = {
  {"nativeInit", "(II)V", (void *) gst_native_init},
  {"nativeFinalize", "()V", (void *) gst_native_finalize},
  {"nativePlay", "()V", (void *) gst_native_play},
  {"nativePause", "()V", (void *) gst_native_pause},
  {"nativeSurfaceInit", "(Ljava/lang/Object;)V",
      (void *) gst_native_surface_init},
  {"nativeSurfaceFinalize", "()V", (void *) gst_native_surface_finalize},
  {"nativeClassInit", "()Z", (void *) gst_native_class_init}
};

/**
 * @brief Library initializer
 */
jint
JNI_OnLoad (JavaVM * vm, void *reserved)
{
  JNIEnv *env = NULL;

  java_vm = vm;

  if ((*vm)->GetEnv (vm, (void **) &env, JNI_VERSION_1_4) != JNI_OK) {
    __android_log_print (ANDROID_LOG_ERROR, TAG_NAME,
        "Could not retrieve JNIEnv");
    return 0;
  }

  jclass klass = (*env)->FindClass (env,
      "org/freedesktop/gstreamer/nnstreamer/ssd/NNStreamerSSD");
  (*env)->RegisterNatives (env, klass, native_methods,
      G_N_ELEMENTS (native_methods));

  pthread_key_create (&current_jni_env, detach_current_thread);

  return JNI_VERSION_1_4;
}
