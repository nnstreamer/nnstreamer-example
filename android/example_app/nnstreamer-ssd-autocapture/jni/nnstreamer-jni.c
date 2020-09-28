/**
 * @file	nnstreamer-jni.cpp
 * @date	1 April 2019
 * @brief	Tensor stream example with TF-Lite model
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @bug		No known bugs
 */

#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "nnstreamer-jni.h"

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
  gint media_width;             /**< The video width */
  gint media_height;            /**< The video height */
  gint pipeline_id;             /**< The pipeline ID */
  gint pipeline_option;         /**< The pipeline option (selected model) */
} CustomData;

/* These global variables cache values which are not changing during execution */
static pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
static JavaVM *java_vm;
static jfieldID custom_data_field_id;
static jmethodID set_message_method_id;
static jmethodID on_gstreamer_initialized_method_id;

/* list of registered pipelines */
static GSList *g_pipelines = NULL;

/**
 * @brief Get data of pipeline ID.
 */
static NNSPipelineInfo *
get_pipeline_info (gint id)
{
  GSList *list;
  NNSPipelineInfo *info;

  list = g_pipelines;
  while (list) {
    info = (NNSPipelineInfo *) list->data;

    if (id == info->id)
      return info;

    list = list->next;
  }

  return NULL;
}

/**
 * @brief Init callback of pipeline data.
 */
static void
init_pipeline_info (void)
{
  GSList *list;
  NNSPipelineInfo *info;

  list = g_pipelines;
  while (list) {
    info = (NNSPipelineInfo *) list->data;

    if (info->init)
      info->init ();

    list = list->next;
  }
}

/**
 * @brief Free callback of pipeline data.
 */
static void
free_pipeline_info (void)
{
  GSList *list;
  NNSPipelineInfo *info;

  list = g_pipelines;
  while (list) {
    info = (NNSPipelineInfo *) list->data;

    if (info->free)
      info->free ();

    list = list->next;
  }
}

/**
 * @brief Register pipeline.
 */
gboolean
nns_register_pipeline (NNSPipelineInfo * info)
{
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (info->id > 0, FALSE);
  g_return_val_if_fail (info->name && info->description, FALSE);
  g_return_val_if_fail (info->launch_pipeline != NULL, FALSE);

  if (get_pipeline_info (info->id) != NULL) {
    nns_loge ("Duplicated pipeline ID %d", info->id);
    return FALSE;
  }

  g_pipelines = g_slist_append (g_pipelines, info);
  return TRUE;
}

/**
 * @brief Register this thread with the VM
 */
static JNIEnv *
attach_current_thread (void)
{
  JNIEnv *env;
  JavaVMAttachArgs args;

  nns_logd ("Attaching thread %p", g_thread_self ());
  args.version = JNI_VERSION_1_4;
  args.name = NULL;
  args.group = NULL;

  if ((*java_vm)->AttachCurrentThread (java_vm, &env, &args) < 0) {
    nns_loge ("Failed to attach current thread");
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
  nns_logd ("Detaching thread %p", g_thread_self ());
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
set_ui_message (CustomData * data, const gchar * message)
{
  JNIEnv *env;
  jstring jmessage;

  nns_logd ("Setting message to: %s", message);

  env = get_jni_env ();
  jmessage = (*env)->NewStringUTF (env, message);

  (*env)->CallVoidMethod (env, data->app, set_message_method_id, jmessage);

  if ((*env)->ExceptionCheck (env)) {
    nns_loge ("Failed to call Java method");
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

  set_ui_message (data, message);
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

    nns_logd ("%s", message);
#if 0 /* Skip to notify state changes */
    set_ui_message (data, message);
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
    NNSPipelineInfo *info;
    gchar *name, *desc;
    jstring jname, jdesc;

    nns_logd
        ("Initialization complete, notifying application. native_window:%p main_loop:%p",
        data->native_window, data->main_loop);

    /* The main loop is running and we received a native window, inform the sink about it */
    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->video_sink),
        (guintptr) data->native_window);

    /* Set pipeline description */
    info = get_pipeline_info (data->pipeline_id);

    name = desc = NULL;
    if (info->get_name) {
      info->get_name(&name, data->pipeline_option);
    }

    if (info->get_description) {
      info->get_description(&desc, data->pipeline_option);
    }

    jname = (*env)->NewStringUTF (env, (name != NULL) ? name : info->name);
    jdesc = (*env)->NewStringUTF (env, (desc != NULL) ? desc : info->description);

    (*env)->CallVoidMethod (env, data->app, on_gstreamer_initialized_method_id,
        jname, jdesc);
    if ((*env)->ExceptionCheck (env)) {
      GST_ERROR ("Failed to call Java method");
      (*env)->ExceptionClear (env);
    }

    (*env)->DeleteLocalRef (env, jname);
    (*env)->DeleteLocalRef (env, jdesc);

    data->initialized = TRUE;
  }
}

/**
 * @brief Main method for the native code. This is executed on its own thread.
 */
static void *
run_pipeline (void *userdata)
{
  CustomData *data = (CustomData *) userdata;
  GstBus *bus;
  GstElement *element;
  GSource *bus_source;
  NNSPipelineInfo *info;
  gchar *message;
  GstStateChangeReturn state;

  nns_logd ("Creating pipeline in CustomData at %p", data);

  info = get_pipeline_info (data->pipeline_id);
  if (info == NULL) {
    message = g_strdup_printf ("Unknown pipeline [%d]", data->pipeline_id);

    set_ui_message (data, message);
    free (message);
    return NULL;
  }

  if (!info->prepare_pipeline (data->pipeline_option)) {
    message = g_strdup_printf ("Cannot start pipeline [%d]", data->pipeline_id);

    set_ui_message (data, message);
    free (message);
    return NULL;
  }

  /* Create our own GLib Main Context and make it the default one */
  data->context = g_main_context_new ();
  g_main_context_push_thread_default (data->context);

  if (!info->launch_pipeline (&data->pipeline, data->pipeline_option)) {
    message =
        g_strdup_printf ("Failed to build pipeline [%d]", data->pipeline_id);

    set_ui_message (data, message);
    g_free (message);
    return NULL;
  }

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
  nns_logd ("Entering main loop... (CustomData:%p)", data);
  data->main_loop = g_main_loop_new (data->context, FALSE);
  check_initialization_complete (data);
  g_main_loop_run (data->main_loop);

  nns_logd ("Exited main loop");
  g_main_loop_unref (data->main_loop);
  data->main_loop = NULL;

  /* Free resources */
  gst_element_set_state (data->pipeline, GST_STATE_NULL);

  do {
    g_usleep (100 * 1000);
    state = gst_element_get_state (data->pipeline, NULL, NULL, GST_SECOND);
  } while (state != GST_STATE_NULL);

  g_main_context_pop_thread_default (data->context);
  g_main_context_unref (data->context);

  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->video_sink),
      (guintptr) NULL);
  gst_object_unref (data->video_sink);
  data->video_sink = NULL;

  gst_object_unref (data->pipeline);
  data->pipeline = NULL;

  data->initialized = FALSE;
  return NULL;
}

/**
 * @brief Stop the pipeline.
 */
static void
gst_native_stop (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  if (data->main_loop) {
    nns_logd ("Quitting main loop...");
    g_main_loop_quit (data->main_loop);

    nns_logd ("Waiting for thread to finish...");
    pthread_join (gst_app_thread, NULL);
  }
}

/**
 * @brief Start a pipeline with given index.
 */
static void
gst_native_start (JNIEnv * env, jobject thiz, jint id, jint option)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  nns_logi ("Try to start pipeline %d with option %d", id, option);

  gst_native_stop (env, thiz);

  data->pipeline_id = id;
  data->pipeline_option = option;

  pthread_create (&gst_app_thread, NULL, &run_pipeline, data);
}

/**
 * @brief Instruct the native code to create its internal data structure, pipeline and thread.
 */
static void
gst_native_init (JNIEnv * env, jobject thiz, jint media_w, jint media_h)
{
  CustomData *data = g_new0 (CustomData, 1);

  g_assert (data);
  SET_CUSTOM_DATA (env, thiz, custom_data_field_id, data);

  GST_DEBUG_CATEGORY_INIT (debug_category, TAG_NAME, 0,
      "Android nnstreamer example");
  gst_debug_set_threshold_for_name (TAG_NAME, GST_LEVEL_DEBUG);

  nns_logd ("Created CustomData at %p", data);

  data->app = (*env)->NewGlobalRef (env, thiz);
  nns_logd ("Created GlobalRef for app object at %p", data->app);

  /* set media resolution */
  data->media_width = (gint) media_w;
  data->media_height = (gint) media_h;

  /* register ahc2src */
  GST_PLUGIN_STATIC_REGISTER (ahc2src);

  /* register nnstreamer plugins */
  GST_PLUGIN_STATIC_REGISTER (nnstreamer);

  /* filter tensorflow-lite sub-plugin */
  init_filter_tflite ();

  /* register pipeline */
  nns_ex_register_pipeline ();

  /* initialize pipelines */
  init_pipeline_info ();
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

  gst_native_stop (env, thiz);

  nns_logd ("Deleting GlobalRef for app object at %p", data->app);
  (*env)->DeleteGlobalRef (env, data->app);
  nns_logd ("Freeing CustomData at %p", data);

  g_free (data);
  SET_CUSTOM_DATA (env, thiz, custom_data_field_id, NULL);

  /* filter tensorflow-lite sub-plugin */
  fini_filter_tflite ();

  /* free pipelines */
  free_pipeline_info ();

  g_slist_free (g_pipelines);
  g_pipelines = NULL;

  nns_logi ("Done finalizing");
}

/**
 * @brief Set pipeline to PLAYING state.
 */
static void
gst_native_play (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data || !data->pipeline)
    return;

  nns_logi ("Setting state to PLAYING");
  gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
}

/**
 * @brief Set pipeline to PAUSED state
 */
static void
gst_native_pause (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data || !data->pipeline)
    return;

  nns_logi ("Setting state to PAUSED");
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
      (*env)->GetMethodID (env, klass, "onGStreamerInitialized",
      "(Ljava/lang/String;Ljava/lang/String;)V");

  if (!custom_data_field_id || !set_message_method_id ||
      !on_gstreamer_initialized_method_id) {
    /**
     * We emit this message through the Android log instead of the GStreamer log
     * because the later has not been initialized yet.
     */
    nns_logd
        ("The calling class does not implement all necessary interface methods");
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

  nns_logd ("Received surface %p (native window %p)", surface,
      new_native_window);

  if (data->native_window) {
    if (data->native_window == new_native_window) {
      nns_logd ("New native window is the same as the previous one %p",
          data->native_window);
      if (data->video_sink) {
        gst_video_overlay_expose (GST_VIDEO_OVERLAY (data->video_sink));
        gst_video_overlay_expose (GST_VIDEO_OVERLAY (data->video_sink));
      }
      return;
    }

    nns_logd ("Released previous native window %p", data->native_window);
    ANativeWindow_release (data->native_window);
    data->initialized = FALSE;
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

  nns_logd ("Releasing Native Window %p", data->native_window);

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
 * @brief Get pipeline name
 */
static jstring
gst_native_get_name (JNIEnv * env, jobject thiz, jint id, jint option)
{
  NNSPipelineInfo *info;
  gchar *title = NULL;
  jstring result;

  info = get_pipeline_info (id);
  if (info) {
    info->get_name (&title, option);
  }

  result = (*env)->NewStringUTF (env, (title) ? title : "Unknown");
  return result;
}

/**
 * @brief Get pipeline description
 */
static jstring
gst_native_get_description (JNIEnv * env, jobject thiz, jint id, jint option)
{
  NNSPipelineInfo *info;
  gchar *desc = NULL;
  jstring result;

  info = get_pipeline_info (id);
  if (info) {
    info->get_description (&desc, option);
  }

  result = (*env)->NewStringUTF (env, (desc) ? desc : "Unknown");
  return result;
}

/**
 * @brief List of implemented native methods
 */
static JNINativeMethod native_methods[] = {
  {"nativeInit", "(II)V", (void *) gst_native_init},
  {"nativeFinalize", "()V", (void *) gst_native_finalize},
  {"nativeStart", "(II)V", (void *) gst_native_start},
  {"nativeStop", "()V", (void *) gst_native_stop},
  {"nativePlay", "()V", (void *) gst_native_play},
  {"nativePause", "()V", (void *) gst_native_pause},
  {"nativeSurfaceInit", "(Ljava/lang/Object;)V",
      (void *) gst_native_surface_init},
  {"nativeSurfaceFinalize", "()V", (void *) gst_native_surface_finalize},
  {"nativeGetName", "(II)Ljava/lang/String;", (void *) gst_native_get_name},
  {"nativeGetDescription", "(II)Ljava/lang/String;",
      (void *) gst_native_get_description},
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
    nns_loge ("Could not retrieve JNIEnv");
    return 0;
  }

  jclass klass = (*env)->FindClass (env,
      "org/freedesktop/gstreamer/nnstreamer/NNStreamerActivity");
  (*env)->RegisterNatives (env, klass, native_methods,
      G_N_ELEMENTS (native_methods));

  pthread_key_create (&current_jni_env, detach_current_thread);

  return JNI_VERSION_1_4;
}
