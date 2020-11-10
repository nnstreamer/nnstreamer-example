/**
 * @file	NNStreamerMultiDevice.c
 * @date	10 July 2019
 * @brief	Multi-Device Exmaple using nnstremaer
 * @author	Jijoong Moon <jijoong.moon@samsung.com>
 * @bug		No known bugs
 *
 * Before running this example, model and labels should be in an internal storage of Android device.
 * /sdcard/nnstreamer/tflite_model/
 *
 * This example run simple pipeline through Three devices using tcpclientsrc & tcpserversink
 *
 * [ Device 1 ]
 * Get Camera Input and do preprocessing with input ( videoconvert & videoscale )
 *
 * [ Device 2 ]
 * Run Obejet Deteciton & if there is person, then cut it and send data to Devcie 3
 *
 * [ Device 3 ]
 * Run Pose Estimation
 */

#include <string.h>
#include <stdint.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <pthread.h>
#include <cairo/cairo.h>
#include "nnstreamer-ssd.h"

GST_DEBUG_CATEGORY_STATIC (debug_category);
#define GST_CAT_DEFAULT debug_category
#define TAG_NAME "NNStreamer"

#define POSE_SIZE 14
#define POSE_OUT_W 96
#define POSE_OUT_H 96
#define MEDIA_WIDTH 480
#define MEDIA_HEIGHT 480


/**
 * @brief These macros provide a way to store the native pointer to CustomData,
 * which might be 32 or 64 bits, into
 * a jlong, which is always 64 bits, without warnings.
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
  jobject app;                  /* Application instance, used to call its methods. A global reference is kept. */
  GstElement *pipeline;         /* The running pipeline */
  GMainContext *context;        /* GLib context used to run the main loop */
  GMainLoop *main_loop;         /* GLib main loop */
  gboolean initialized;         /* To avoid informing the UI multiple times about the initialization */
  GstElement *video_sink;       /* The video sink element which receives XOverlay commands */
  ANativeWindow *native_window; /* The Android native window where video will be rendered */
  int media_width;
  int media_height;
  int id;
  int portnum;
  char *ipaddr;
  int portnum_sub;
  char *ipaddr_sub;
} CustomData;

/**
 * @brief data structure for pose estimation callback
 */
typedef struct
{
  gboolean valid;
  guint x;
  guint y;
  gfloat prob;
} pose_s;

GMutex res_mutex;

/* These global variables cache values which are not changing during execution */
static pthread_t gst_app_thread;
static pthread_key_t current_jni_env;
static JavaVM *java_vm;
static jfieldID custom_data_field_id;
static jmethodID set_message_method_id;
static jmethodID on_gstreamer_initialized_method_id;
static gchar *pipeline_desc = NULL;

GList *detected_object = NULL;
pose_s estimated_pose[POSE_SIZE];

int person_x = 0, person_y = 0, person_width = 0, person_height = 0;

int new = 0;
GstElement *cropelement;

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
  JNIEnv *env = get_jni_env ();
  GST_DEBUG ("Setting message to: %s", message);
  jstring jmessage = (*env)->NewStringUTF (env, message);
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
  gchar *message_string;

  gst_message_parse_error (msg, &err, &debug_info);
  message_string =
      g_strdup_printf ("Error received from element %s: %s",
      GST_OBJECT_NAME (msg->src), err->message);
  g_clear_error (&err);
  g_free (debug_info);
  set_ui_message (message_string, data);
  g_free (message_string);
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
    set_ui_message (message, data);
    g_free (message);
  }
}

/**
 * @brief Notify UI about pipeline state changes
 */
static void
get_description (gchar ** desc, const gint option)
{
  *desc = pipeline_desc;
}

/**
 * @brief Check if all conditions are met to report GStreamer as initialized.
 * These conditions will change depending on the application
 */
static void
check_initialization_complete (CustomData * data)
{
  JNIEnv *env = get_jni_env ();
  gchar *desc = NULL;
  jstring jdesc;

  if (!data->initialized && data->native_window && data->main_loop) {
    GST_DEBUG
        ("Initialization complete, notifying application. native_window:%p main_loop:%p",
        data->native_window, data->main_loop);

    /* The main loop is running and we received a native window, inform the sink about it */
    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->video_sink),
        (guintptr) data->native_window);

    get_description (&desc, data->id);

    jdesc = (*env)->NewStringUTF (env, desc);

    (*env)->CallVoidMethod (env, data->app, on_gstreamer_initialized_method_id,
        jdesc);
    if ((*env)->ExceptionCheck (env)) {
      GST_ERROR ("Failed to call Java method");
      (*env)->ExceptionClear (env);
    }

    (*env)->DeleteLocalRef (env, jdesc);
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
  int area = 0;

  if (!data)
    return;

  /* set font props */
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, 20.0);

  max_objects = ssd_get_detected_objects (objects, max_objects);
  new++;
  for (i = 0; i < max_objects; ++i) {
    if (!ssd_get_label (objects[i].class_id, &label)) {
      GST_ERROR ("Failed to get label (class id %d)", objects[i].class_id);
      continue;
    }

    x = objects[i].x * data->media_width / MODEL_WIDTH;
    y = objects[i].y * data->media_height / MODEL_HEIGHT;
    width = objects[i].width * data->media_width / MODEL_WIDTH;
    height = objects[i].height * data->media_height / MODEL_HEIGHT;

    if (strcmp (label, "person") == 0 && new == 15) {
      if (area < width * height) {
        person_x = x;
        person_y = y;
        person_width = width;
        person_height = height;
        area = width * height;
      }
    }

    cairo_rectangle (cr, x, y, width, height);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_set_line_width (cr, 1.5);
    cairo_stroke (cr);
    cairo_fill_preserve (cr);


    cairo_move_to (cr, x + 5, y + 25);
    cairo_text_path (cr, label);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_set_line_width (cr, .3);
    cairo_stroke (cr);
    cairo_fill_preserve (cr);

  }
  if (new == 15)
    new = 0;
}

/**
 * @brief Callback to draw the overlay.
 */
static GstPadProbeReturn
cb_have_data (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  gint x, y;
  GstMapInfo map, newmap;
  GstMemory *mem;
  guint8 *ptr, *newptr, t;
  GstBuffer *buffer, *newbuffer;

  buffer = GST_PAD_PROBE_INFO_BUFFER (info);
  buffer = gst_buffer_make_writable (buffer);

  if (buffer == NULL)
    return GST_PAD_PROBE_OK;

  if (gst_buffer_map (buffer, &map, GST_MAP_WRITE)) {
    ptr = map.data;

    for (y = 0; y < 480; y++) {
      for (x = 0; x < 480; x++) {
        if (x < person_x || y < person_y) {
          ptr[y * 480 * 3 + x * 3] = 0;
          ptr[y * 480 * 3 + x * 3 + 1] = 0;
          ptr[y * 480 * 3 + x * 3 + 2] = 0;
        }
        if (x > person_x + person_width || y > person_y + person_height) {
          ptr[y * 480 * 3 + x * 3] = 0;
          ptr[y * 480 * 3 + x * 3 + 1] = 0;
          ptr[y * 480 * 3 + x * 3 + 2] = 0;
        }
      }
    }
    gst_buffer_unmap (buffer, &map);
  }

  GST_PAD_PROBE_INFO_DATA (info) = buffer;

  return GST_PAD_PROBE_OK;
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
new_pose_data_cb (GstElement * element, GstBuffer * buffer, gpointer user_data)
{

  GstMemory *mem_pose;
  GstMapInfo info_pose;
  gfloat *pose_data;
  guint index, i, j;
  guint maxX, maxY;
  gfloat max, cen;
  pose_s detected[POSE_SIZE];
  pose_s *p;
  const gfloat threshold_score = .5f;

  if (gst_buffer_n_memory (buffer) != 1) {
    GST_ERROR ("Invalid");
    return;
  }

  mem_pose = gst_buffer_get_memory (buffer, 0);
  gst_memory_map (mem_pose, &info_pose, GST_MAP_READ);
  pose_data = (gfloat *) info_pose.data;

  for (index = 0; index < POSE_SIZE; ++index) {
    maxX = maxY = 0;
    max = .0f;
    for (j = 0; j < POSE_OUT_H; ++j) {
      for (i = 0; i < POSE_OUT_W; ++i) {
        cen = pose_data[i * POSE_SIZE + j * POSE_OUT_W * POSE_SIZE + index];
        if (cen > max) {
          max = cen;
          maxX = i;
          maxY = j;
        }
      }
    }
    detected[index].valid = FALSE;
    detected[index].x = maxX;
    detected[index].y = maxY;
    detected[index].prob = max;
  }

  gst_memory_unmap (mem_pose, &info_pose);
  gst_memory_unref (mem_pose);

  g_mutex_lock (&res_mutex);

  for (i = 0; i < POSE_SIZE; ++i) {
    if (detected[i].prob > threshold_score)
      detected[i].valid = TRUE;
    estimated_pose[i].prob = detected[i].prob;
    estimated_pose[i].x = detected[i].x;
    estimated_pose[i].y = detected[i].y;
    estimated_pose[i].valid = detected[i].valid;
  }

  g_mutex_unlock (&res_mutex);
}

/**
 * @brief Draw Line using cairo
 */
static void
pose_draw_line (cairo_t * cr, pose_s * detected, guint start, guint end)
{
  gdouble xs, ys, xe, ye;

  if (detected[start].valid && detected[end].valid) {
    xs = detected[start].x * MEDIA_WIDTH / POSE_OUT_W;
    ys = detected[start].y * MEDIA_HEIGHT / POSE_OUT_H;

    xe = detected[end].x * MEDIA_WIDTH / POSE_OUT_W;
    ye = detected[end].y * MEDIA_HEIGHT / POSE_OUT_H;

    cairo_move_to (cr, xs, ys);
    cairo_line_to (cr, xe, ye);
    cairo_stroke (cr);
  }
}

/**
 * @brief Callback to draw the overlay.
 */
static void
draw_pose_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp,
    guint64 duration, gpointer user_data)
{

  pose_s detected[POSE_SIZE];
  gdouble x, y;
  int i;
  g_mutex_lock (&res_mutex);
  for (i = 0; i < POSE_SIZE; i++) {
    detected[i].prob = estimated_pose[i].prob;
    detected[i].x = estimated_pose[i].x;
    detected[i].y = estimated_pose[i].y;
    detected[i].valid = estimated_pose[i].valid;
  }
  g_mutex_unlock (&res_mutex);

  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
  cairo_set_line_width (cr, 3.0);

  /* line */
  pose_draw_line (cr, detected, 0, 1);
  pose_draw_line (cr, detected, 1, 2);
  pose_draw_line (cr, detected, 2, 3);
  pose_draw_line (cr, detected, 3, 4);
  pose_draw_line (cr, detected, 1, 5);
  pose_draw_line (cr, detected, 5, 6);
  pose_draw_line (cr, detected, 6, 7);
  pose_draw_line (cr, detected, 1, 8);
  pose_draw_line (cr, detected, 8, 9);
  pose_draw_line (cr, detected, 9, 10);
  pose_draw_line (cr, detected, 1, 11);
  pose_draw_line (cr, detected, 11, 12);
  pose_draw_line (cr, detected, 12, 13);

  for (i = 0; i < POSE_SIZE; ++i) {
    if (detected[i].valid) {
      x = detected[i].x * MEDIA_WIDTH / POSE_OUT_W;
      y = detected[i].y * MEDIA_HEIGHT / POSE_OUT_H;

      cairo_arc (cr, x, y, 5, 0, 2 * 3.14);
      cairo_fill (cr);
    }
  }
}

/**
 * @brief append text to display pipeline
 */
static gchar *
append_text (gchar * old, gchar * str)
{
  gchar *new_pipeline;

  new_pipeline = g_strconcat (old, str, NULL);

  g_free (str);
  g_free (old);

  return new_pipeline;
}

/**
 * @brief append description to display pipeline
 */
static gchar *
append_description (gchar * old, gchar * str, const gchar * color)
{
  gchar *wrap, *new_desc;

  wrap = g_strdup_printf ("<p style=\"color:%s;\">%s</p>", color, str);
  new_desc = g_strconcat (old, wrap, NULL);

  g_free (wrap);
  g_free (str);
  g_free (old);

  return new_desc;
}

/**
 * @brief set description
 * 0: Default ( Camera )
 * 1: Face Detection
 * 2: Hand Detection
 * 3: Pose Estimation
 */
static void
set_description (const gint option, gchar * pipe)
{
  gchar *str_desc, *extra, *color;

  str_desc = g_strdup
      ("<html><meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"><body>");

  switch (option) {
    case 0:
      color = g_strdup ("#0000FF");
      break;
    case 1:
      color = g_strdup ("#008000");
      break;
    case 2:
      color = g_strdup ("#FF0000");
      break;
    default:
      color = g_strdup ("#262626");
  }

  extra = g_strdup (pipe);
  str_desc = append_description (str_desc, extra, color);
  extra = g_strdup ("</body></html>");
  str_desc = append_text (str_desc, extra);

  if (pipeline_desc)
    g_free (pipeline_desc);
  pipeline_desc = str_desc;
}


/**
 * @brief Main method for the native code. This is executed on its own thread.
 */
static void *
app_function (void *userdata)
{
  JavaVMAttachArgs args;
  GstBus *bus;
  GstElement *element;
  CustomData *data = (CustomData *) userdata;
  GSource *bus_source;
  GError *error = NULL;
  gchar *str_pipeline, *model_path;
  GstRegistry *registry;
  registry = gst_registry_get ();
  GList *list = gst_registry_get_plugin_list (registry);
  GList *l;
  GstPad *pad;
  g_list_free (list);

  GST_DEBUG ("Creating pipeline in CustomData at %p", data);

  /* Create our own GLib Main Context and make it the default one */
  data->context = g_main_context_new ();
  g_main_context_push_thread_default (data->context);

  if (!ssd_get_model_path (&model_path)) {
    __android_log_print (ANDROID_LOG_ERROR, TAG_NAME,
        "Failed to get SSD model path");
    return NULL;
  }



  switch (data->id) {
    case 0:
      str_pipeline =
          g_strdup_printf
          ("ahc2src camera-index=1 ! videoconvert ! video/x-raw, format=RGB, width=640, height=480, framerate=30/1 ! videoflip method=counterclockwise ! "
          "tee name=t "
          "t. ! queue ! videocrop top=80 bottom=80 ! jpegenc qos=true ! tcpserversink host=%s port=%d sync=false "
          "t. ! videoconvert ! glimagesink sync=false", data->ipaddr,
          data->portnum);
      break;
    case 1:
      str_pipeline =
          g_strdup_printf
          ("tcpclientsrc host=%s port=%d ! image/jpeg, framerate=30/1 ! "
          "jpegparse ! jpegdec ! videoconvert ! video/x-raw, format=RGB, "
          "width=480, height=480, framerate=30/1 ! tee name=t t. ! queue ! "
          "videoconvert ! cairooverlay name=res_overlay ! glimagesink sync=false "
          "t. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw, width=300, height=300, format=RGB "
          "! tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,add:-127,div:127.5 qos=true ! "
          "tensor_filter framework=tensorflow-lite model=/sdcard/nnstreamer/tflite_model/ssd_mobilenet_v2_coco.tflite qos=true ! "
          "tensor_sink name=res_sink sync=false "
          "t. ! queue ! videoconvert ! videocrop name=crop ! jpegenc qos=true ! tcpserversink host=%s port=%d sync=false ",
          data->ipaddr, data->portnum, data->ipaddr_sub, data->portnum_sub);
      break;
    case 2:
      str_pipeline =
          g_strdup_printf
          ("tcpclientsrc host=%s port=%d ! image/jpeg, framerate=30/1 ! jpegparse ! jpegdec ! videoconvert ! "
          "tee name=t t. ! queue ! videoconvert ! cairooverlay name=res_overlay ! glimagesink sync=false "
          "t. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw, width=192, height=192, format=RGB ! "
          "tensor_converter ! tensor_transform mode=typecast option=float32 qos=true ! "
          "tensor_filter framework=tensorflow-lite model=/sdcard/nnstreamer/tflite_model/detect_pose.tflite qos=true ! "
          "tensor_sink name=res_sink sync=false ", data->ipaddr, data->portnum);
      break;
    default:
      break;

  }

  set_description (data->id, str_pipeline);
  /* Build pipeline */
  data->pipeline = gst_parse_launch (str_pipeline, &error);
  g_free (str_pipeline);
  if (error) {
    gchar *message =
        g_strdup_printf ("Unable to build pipeline: %s", error->message);
    g_clear_error (&error);
    set_ui_message (message, data);
    g_free (message);
    return NULL;
  }

  if (data->id == 1) {
    element = gst_bin_get_by_name (GST_BIN (data->pipeline), "res_sink");
    g_signal_connect (element, "new-data", G_CALLBACK (new_data_cb), data);
    gst_object_unref (element);

    element = gst_bin_get_by_name (GST_BIN (data->pipeline), "res_overlay");
    g_signal_connect (element, "draw", G_CALLBACK (draw_overlay_cb), data);
    gst_object_unref (element);

    cropelement = gst_bin_get_by_name (GST_BIN (data->pipeline), "crop");
    pad = gst_element_get_static_pad (cropelement, "src");
    gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
        (GstPadProbeCallback) cb_have_data, NULL, NULL);
    gst_object_unref (pad);
  }
  if (data->id == 2) {
    element = gst_bin_get_by_name (GST_BIN (data->pipeline), "res_sink");
    g_signal_connect (element, "new-data", G_CALLBACK (new_pose_data_cb), data);
    gst_object_unref (element);

    element = gst_bin_get_by_name (GST_BIN (data->pipeline), "res_overlay");
    g_signal_connect (element, "draw", G_CALLBACK (draw_pose_overlay_cb), data);
    gst_object_unref (element);
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
 * @brief Instruct the native code to create its internal data structure, pipeline and thread
 */
static void
gst_native_init (JNIEnv * env, jobject thiz, jobject context, jint media_w,
    jint media_h)
{
  CustomData *data = g_new0 (CustomData, 1);
  SET_CUSTOM_DATA (env, thiz, custom_data_field_id, data);
  GST_DEBUG_CATEGORY_INIT (debug_category, "NNStreamer", 0,
      "Android NNStreamer MultiDevice Demo");
  gst_debug_set_threshold_for_name ("NNStreamer", GST_LEVEL_DEBUG);
  GST_DEBUG ("Created CustomData at %p", data);
  data->app = (*env)->NewGlobalRef (env, thiz);
  GST_DEBUG ("Created GlobalRef for app object at %p", data->app);

  jclass context_class;
  jmethodID package_code_id;
  jobject dir;

  context_class = (*env)->GetObjectClass (env, context);
  package_code_id =
      (*env)->GetMethodID (env, context_class, "getPackageCodePath",
      "()Ljava/lang/String;");
  dir = (*env)->CallObjectMethod (env, context, package_code_id);
  const gchar *str;
  gchar *replace = "lib/arm64";
  jboolean isCopy;
  str = (*env)->GetStringUTFChars (env, (jstring) dir, &isCopy);

  GString *path = g_string_new (str);
  int str_len = (int) path->len - 8;
  path = g_string_truncate (path, str_len);
  path = g_string_append (path, replace);
  __android_log_print (ANDROID_LOG_INFO, "GStreamer", "----- %s", path->str);
  setenv ("GST_PLUGIN_PATH", path->str, 1);
  setenv ("LD_LIBRARY_PATH", path->str, 1);
  data->media_width = (int) media_w;
  data->media_height = (int) media_h;

  ssd_init ();

}

/**
 * @brief Quit the main loop, remove the native thread and free resources
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
  GST_DEBUG ("Done finalizing");

  ssd_free ();

}

/**
 * @brief Set crop
 */
static gboolean
set_crop (GstElement * crop)
{
  return (GST_STATE (GST_ELEMENT (crop)) == GST_STATE_PLAYING);
}

/**
 * @brief Set pipeline to PLAYING state
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
 * @brief Static class initializer: retrieve method and field IDs
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
      "(Ljava/lang/String;)V");


  if (!custom_data_field_id || !set_message_method_id
      || !on_gstreamer_initialized_method_id) {
    __android_log_print (ANDROID_LOG_ERROR, "NNStreamer",
        "The calling class does not implement all necessary interface methods");
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

/**
 * @brief Native Surface init
 */
static void
gst_native_surface_init (JNIEnv * env, jobject thiz, jobject surface)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
  if (!data)
    return;
  ANativeWindow *new_native_window = ANativeWindow_fromSurface (env, surface);
  GST_DEBUG ("Received surface %p (native window %p)", surface,
      new_native_window);
  if (data->native_window) {
    if (data->native_window == new_native_window) {
      GST_DEBUG ("New native window is the same as the previous one %p",
          data->native_window);
      ANativeWindow_release (new_native_window);
      if (data->video_sink) {
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
 * @brief Native Surface finalize
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
 * @brief Native Surface stop
 */
static void
gst_native_stop (JNIEnv * env, jobject thiz)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);

  if (!data)
    return;

  if (data->main_loop) {
    g_main_loop_quit (data->main_loop);

    pthread_join (gst_app_thread, NULL);
  }
}

/**
 * @brief Native Surface start
 */
static void
gst_native_start (JNIEnv * env, jobject thiz, jstring ipaddr,
    jstring ipaddr_sub, jint id, jint port, jint port_sub)
{
  CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
  if (!data)
    return;
  gst_native_stop (env, thiz);


  data->id = id;
  data->portnum = port;
  data->ipaddr = (*env)->GetStringUTFChars (env, ipaddr, NULL);

  if (id == 1) {
    data->portnum_sub = port_sub;
    data->ipaddr_sub = (*env)->GetStringUTFChars (env, ipaddr_sub, NULL);
  }

  pthread_create (&gst_app_thread, NULL, &app_function, data);
}

/**
 * @brief List of implemented native methods
 */
static JNINativeMethod native_methods[] = {
  {"nativeInit", "(Landroid/content/Context;II)V", (void *) gst_native_init},
  {"nativeFinalize", "()V", (void *) gst_native_finalize},
  {"nativeStart", "(Ljava/lang/String;Ljava/lang/String;III)V",
      (void *) gst_native_start},
  {"nativeStop", "()V", (void *) gst_native_stop},
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
    __android_log_print (ANDROID_LOG_ERROR, "NNStreamer",
        "Could not retrieve JNIEnv");
    return 0;
  }
  jclass klass = (*env)->FindClass (env,
      "org/freedesktop/nnstreamer/nnstreamermultidevice/NNStreamerMultiDevice");
  (*env)->RegisterNatives (env, klass, native_methods,
      G_N_ELEMENTS (native_methods));

  pthread_key_create (&current_jni_env, detach_current_thread);

  return JNI_VERSION_1_4;
}
