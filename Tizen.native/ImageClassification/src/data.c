/**
 * @file data.c
 * @date 13 November 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include "main.h"
#include "data.h"
#include <stdio.h>
#include <unistd.h>
#include <camera.h>
#include <storage.h>
#include <image_util.h>
#include <tbm_surface.h>

/**
 * @brief the UI objects structure
 */
typedef struct _camdata
{
  camera_h g_camera;            /* Camera handle */
  Evas_Object *cam_display;
  Evas_Object *cam_display_box;
  Evas_Object *display;
  Evas_Object *zoom_bt;
  Evas_Object *brightness_bt;
  Evas_Object *get_label_bt;
} camdata;

static camdata cam_data;

static int is_checking;
static int result_label;

static transformation_h t_handle;
G_LOCK_DEFINE_STATIC (callback_lock);

/**
 * @brief set the input and tensor data to get the result.
 */
static int
find_label (uint8_t * input_buf)
{
  ml_tensors_data_h data;
  unsigned int data_size = CH * CAM_WIDTH * CAM_HEIGHT;

  memcpy (inArray, input_buf, sizeof (uint8_t) * data_size);

  /* insert input data to appsrc */
  ml_tensors_data_create (info, &data);
  ml_tensors_data_set_tensor_data (data, 0, inArray, data_size);
  ml_pipeline_src_input_data (srchandle, data,
      ML_PIPELINE_BUF_POLICY_AUTO_FREE);

  return 0;
}

/**
 * @brief when the image transform is finished, this callback runs.
 */
static void
transform_completed_cb (media_packet_h * dst, int error_code, void *user_data)
{
  media_packet_h src = (media_packet_h) user_data;
  uint8_t *input_buf;

  media_packet_get_buffer_data_ptr (*dst, (void **) &input_buf);

  find_label (input_buf);

  media_packet_destroy (*dst);
  media_packet_destroy (src);
}

/**
 * @brief when the preview frame is sent, this callback runs.
 */
static void
preview_frame_cb (media_packet_h pkt, void *user_data)
{
  G_LOCK (callback_lock);

  if (is_checking) {
    G_UNLOCK (callback_lock);

    media_packet_destroy (pkt);
    return;
  }

  is_checking = 1;

  camera_unset_media_packet_preview_cb (cam_data.g_camera);

  image_util_transform_create (&t_handle);
  image_util_transform_set_colorspace (t_handle, IMAGE_UTIL_COLORSPACE_RGB888);
  image_util_transform_run (t_handle, pkt, transform_completed_cb, pkt);
  G_UNLOCK (callback_lock);
  return;
}

/**
 * @brief Maps the given camera state to its string representation.
 *
 * @param state  The camera state that should be mapped to its literal
 *               representation
 *
 * @return The string representation of the given camera state
 */
static const char *
_camera_state_to_string (camera_state_e state)
{
  switch (state) {
    case CAMERA_STATE_NONE:
      return "CAMERA_STATE_NONE";

    case CAMERA_STATE_CREATED:
      return "CAMERA_STATE_CREATED";

    case CAMERA_STATE_PREVIEW:
      return "CAMERA_STATE_PREVIEW";

    case CAMERA_STATE_CAPTURING:
      return "CAMERA_STATE_CAPTURING";

    case CAMERA_STATE_CAPTURED:
      return "CAMERA_STATE_CAPTURED";

    default:
      return "Unknown";
  }
}

/**
 * @brief Print the result of image classification.
 * @details Called when the "Run Image Classification" button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() signature defined in the
 *          Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. This argument is
 *                    not used in this case.
 * @param obj         A handle to the object on which the event occurred. In
 *                    this case it's a pointer to the button object. This
 *                    argument is not used in this case.
 * @param event_info  A pointer to a data which is totally dependent on the
 *                    smart object's implementation and semantic for the given
 *                    event. This argument is not used in this case.
 */
static void
__camera_cb_image_classification (void *data, Evas_Object * obj,
    void *event_info)
{
  PRINT_MSG ("%s is detected", labels[result_label]);
}

/**
 * @brief Sets the camera brightness.
 * @details Called when the "Brightness" button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() signature defined in the
 *          Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. This argument is
 *                    not used in this case.
 * @param obj         A handle to the object on which the event occurred. In
 *                    this case it's a pointer to the button object. This
 *                    argument is not used in this case.
 * @param event_info  A pointer to a data which is totally dependent on the
 *                    smart object's implementation and semantic for the given
 *                    event. This argument is not used in this case.
 */
static void
__camera_cb_bright (void *data, Evas_Object * obj, void *event_info)
{
  /**
   * Get the minimal and maximal supported value for the camera brightness
   * attribute.
   */
  int min, max;

  int error_code = camera_attr_get_brightness_range (cam_data.g_camera, &min,
      &max);
  if (CAMERA_ERROR_NONE != error_code) {
    dlog_print (DLOG_ERROR, LOG_TAG, "camera_attr_get_brightness_range",
        error_code);
    PRINT_MSG ("Could not get brightness range.");
  }

  /* Get the current value of the camera brightness attribute. */
  int brightness_level;

  error_code =
      camera_attr_get_brightness (cam_data.g_camera, &brightness_level);
  if (CAMERA_ERROR_NONE != error_code) {
    dlog_print (DLOG_ERROR, LOG_TAG, "camera_attr_get_brightness", error_code);
    PRINT_MSG ("Could not get current brightness value.");
  }

  /* Set new value of the camera brightness attribute */
  brightness_level = brightness_level == max ? min : ++brightness_level;
  error_code = camera_attr_set_brightness (cam_data.g_camera, brightness_level);
  if (CAMERA_ERROR_NONE != error_code) {
    if (CAMERA_ERROR_NOT_SUPPORTED != error_code) {
      dlog_print (DLOG_ERROR, LOG_TAG, "camera_attr_set_brightness",
          error_code);
      PRINT_MSG ("Could not set new brightness value.");
    } else
      dlog_print (DLOG_INFO, LOG_TAG,
          "Camera brightness is not supported on this device.");
  } else
    PRINT_MSG ("Brightness value set to %d", brightness_level);
}

/**
 * @brief Sets the camera zoom.
 * @details Called when the "Zoom" button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() signature defined in the
 *          Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. This argument is
 *                    not used in this case.
 * @param obj         A handle to the object on which the event occurred. In
 *                    this case it's a pointer to the button object. This
 *                    argument is not used in this case.
 * @param event_info  A pointer to a data which is totally dependent on the
 *                    smart object's implementation and semantic for the given
 *                    event. This argument is not used in this case.
 */
static void
__camera_cb_zoom (void *data, Evas_Object * obj, void *event_info)
{
  /* Get the minimal and maximal supported value for the camera zoom attribute. */
  int min, max;

  int error_code = camera_attr_get_zoom_range (cam_data.g_camera, &min, &max);
  if (CAMERA_ERROR_NONE != error_code) {
    dlog_print (DLOG_ERROR, LOG_TAG, "camera_attr_get_zoom_range", error_code);
    PRINT_MSG ("Could not get zoom range.");
  }

  /* Get the current value of the camera zoom attribute. */
  int zoom;

  error_code = camera_attr_get_zoom (cam_data.g_camera, &zoom);
  if (CAMERA_ERROR_NONE != error_code) {
    dlog_print (DLOG_ERROR, LOG_TAG, "camera_attr_get_zoom", error_code);
    PRINT_MSG ("Could not get current zoom value.");
  }

  /* Set new value of the camera zoom attribute */
  zoom = zoom == max ? min : ++zoom;
  error_code = camera_attr_set_zoom (cam_data.g_camera, zoom);
  if (CAMERA_ERROR_NONE != error_code) {
    if (CAMERA_ERROR_NOT_SUPPORTED != error_code) {
      dlog_print (DLOG_ERROR, LOG_TAG, "camera_attr_set_zoom", error_code);
      PRINT_MSG ("Could not set new zoom value.");
    } else {
      dlog_print (DLOG_INFO, LOG_TAG,
          "Camera zoom is not supported on this device.");
      PRINT_MSG ("Camera zoom is not supported on this device.");
    }
  } else {
    PRINT_MSG ("Zoom value set to %d", zoom);
  }
}

/**
 * @brief get label with the output of tensor_sink.
 */
void
get_label_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  size_t data_size;
  uint8_t *output;
  int label_num = -1, max = -1;

  ml_tensors_data_get_tensor_data (data, 0, (void **) &output, &data_size);

  for (int i = 0; i < 1001; i++) {
    if (output[i] > 0 && output[i] > max) {
      max = output[i];
      label_num = i;
    }
  }

  G_LOCK (callback_lock);
  is_checking = 0;
  result_label = label_num;
  image_util_transform_destroy (t_handle);

  camera_set_media_packet_preview_cb (cam_data.g_camera, preview_frame_cb, NULL);
  G_UNLOCK (callback_lock);
}

/**
 * @brief Called when the camera preview display is being resized.
 * @details It resizes the camera preview to fit the camera preview display.
 * @remarks This function matches the Evas_Object_Event_Cb() signature defined
 *          in the Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. In this case it is
 *                    used for passing the camera preview object. This argument
 *                    is not used in this case.
 * @param e           The canvas pointer on which the event occurred
 * @param obj         A pointer to the object on which the event occurred. In
 *                    this case it is the camera preview display.
 * @param event_info  In case of the EVAS_CALLBACK_RESIZE event, this parameter
 *                    is NULL. This argument is not used in this case.
 */
void
_post_render_cb (void *data, Evas * e, Evas_Object * obj, void *event_info)
{
  Evas_Object **cam_data_image = (Evas_Object **) data;

  /* Get the size of the parent box. */
  int x = 0, y = 0, w = 0, h = 0;
  evas_object_geometry_get (obj, &x, &y, &w, &h);

  /* Set the size of the image object. */
  evas_object_resize (*cam_data_image, w, h);
  evas_object_move (*cam_data_image, 0, y);
}

/**
 * @brief Creates the main view of the application.
 *
 */
void
create_buttons_in_main_window (void)
{
  /**
   * Create the window with camera preview and buttons for manipulating the
   * camera and taking the photo.
   */
  cam_data.display = _create_new_cd_display ("NNStreamer", NULL);

  /* Create a box for the camera preview. */
  cam_data.cam_display_box = elm_box_add (cam_data.display);
  elm_box_horizontal_set (cam_data.cam_display_box, EINA_FALSE);
  evas_object_size_hint_align_set (cam_data.cam_display_box, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  evas_object_size_hint_weight_set (cam_data.cam_display_box, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end (cam_data.display, cam_data.cam_display_box);
  evas_object_show (cam_data.cam_display_box);

  Evas *evas = evas_object_evas_get (cam_data.cam_display_box);
  cam_data.cam_display = evas_object_image_add (evas);
  evas_object_event_callback_add (cam_data.cam_display_box,
      EVAS_CALLBACK_RESIZE, _post_render_cb, &(cam_data.cam_display));

  /* Create buttons for the Camera. */
  cam_data.zoom_bt = _new_button (cam_data.display, "Zoom", __camera_cb_zoom);
  cam_data.brightness_bt = _new_button (cam_data.display, "Brightness",
      __camera_cb_bright);
  cam_data.get_label_bt = _new_button (cam_data.display,
      "Run Image Classification", __camera_cb_image_classification);

  int error_code = CAMERA_ERROR_NONE;

  elm_object_disabled_set (cam_data.zoom_bt, EINA_TRUE);
  elm_object_disabled_set (cam_data.brightness_bt, EINA_TRUE);
  elm_object_disabled_set (cam_data.get_label_bt, EINA_TRUE);

  /* Create the camera handle for the main camera of the device. */
  error_code = camera_create (CAMERA_DEVICE_CAMERA0, &(cam_data.g_camera));
  if (CAMERA_ERROR_NONE != error_code) {
    dlog_print (DLOG_ERROR, LOG_TAG, "camera_create", error_code);
    PRINT_MSG ("Could not create a handle to the camera.");
    return;
  }

  /* Check the camera state after creating the handle. */
  camera_state_e state;
  error_code = camera_get_state (cam_data.g_camera, &state);
  if (CAMERA_ERROR_NONE != error_code || CAMERA_STATE_CREATED != state) {
    dlog_print (DLOG_ERROR, LOG_TAG,
        "camera_get_state() failed! Error code = %d, state = %s", error_code,
        _camera_state_to_string (state));
    return;
  }

  /* Set the display for the camera preview. */
  error_code = camera_set_display (cam_data.g_camera, CAMERA_DISPLAY_TYPE_EVAS,
      GET_DISPLAY (cam_data.cam_display));
  if (CAMERA_ERROR_NONE != error_code) {
    dlog_print (DLOG_ERROR, LOG_TAG, "camera_set_display", error_code);
    PRINT_MSG ("Could not set the camera display.");
    return;
  }

  error_code = camera_set_preview_resolution (cam_data.g_camera, CAM_WIDTH,
      CAM_HEIGHT);
  if (CAMERA_ERROR_NONE != error_code) {
    dlog_print (DLOG_ERROR, LOG_TAG, "camera_set_preview_resolution",
        error_code);
    PRINT_MSG ("Could not set the camera preview resolution.");
  }

  /* Show the camera preview UI element. */
  evas_object_size_hint_weight_set (cam_data.display, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_size_hint_weight_set (cam_data.cam_display_box,
      EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_show (cam_data.cam_display_box);

  /* Start the camera preview. */
  camera_set_media_packet_preview_cb (cam_data.g_camera, preview_frame_cb,
      NULL);
  error_code = camera_start_preview (cam_data.g_camera);
  if (CAMERA_ERROR_NONE != error_code) {
    dlog_print (DLOG_ERROR, LOG_TAG, "camera_start_preview", error_code);
    PRINT_MSG ("Could not start the camera preview.");
    return;
  }

  /* Enable other camera buttons. */
  elm_object_disabled_set (cam_data.zoom_bt, EINA_FALSE);
  elm_object_disabled_set (cam_data.brightness_bt, EINA_FALSE);
  elm_object_disabled_set (cam_data.get_label_bt, EINA_FALSE);
}
