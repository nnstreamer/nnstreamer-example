/**
 * @file data.c
 * @date 28 Feb 2020
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author Parichay Kapoor <pk.kapoor@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include "main.h"
#include "data.h"
#include <camera.h>

size_t IMG_WIDTH = 640;
size_t IMG_HEIGHT = 480;

#define BUFLEN 512

/**
 * @brief Display and Camera related objects
 */
typedef struct _camdata {
  camera_h g_camera; /* Camera handle */
  Evas_Object *cam_display;
  Evas_Object *cam_display_box;
  Evas_Object *display;
  Evas_Object *preview_bt;
  Evas_Object *zoom_bt;
  Evas_Object *brightness_bt;
  Evas_Object *photo_bt;
  bool cam_prev;
} camdata;
static camdata cam_data;

/**
 * @brief Maps the given camera state to its string representation.
 *
 * @param state  The camera state that should be mapped to its literal
 *               representation
 *
 * @return The string representation of the given camera state
 */
static const char *_camera_state_to_string(camera_state_e state)
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
 * @brief Starts the camera preview.
 * @details Called when the "Start preview" button is clicked.
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
static void __camera_cb_preview(void *data, Evas_Object *obj,
    void *event_info)
{
  int error_code = CAMERA_ERROR_NONE;

  if (!cam_data.cam_prev) {
    /* Show the camera preview UI element. */
    evas_object_size_hint_weight_set(cam_data.display, EVAS_HINT_EXPAND,
        EVAS_HINT_EXPAND);
    evas_object_size_hint_weight_set(cam_data.cam_display_box,
        EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(cam_data.cam_display_box);
    evas_object_show(cam_data.cam_display);

    /* Start the camera preview. */
    error_code = camera_start_preview(cam_data.g_camera);
    if (CAMERA_ERROR_NONE != error_code) {
      DLOG_PRINT_ERROR("camera_start_preview", error_code);
      return;
    }

    cam_data.cam_prev = true;

    elm_object_text_set(cam_data.preview_bt, "Stop preview");
  } else {
    /* Hide the camera preview UI element. */
    evas_object_size_hint_weight_set(cam_data.display, EVAS_HINT_EXPAND,
        EVAS_HINT_EXPAND);
    evas_object_size_hint_weight_set(cam_data.cam_display_box,
        EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_hide(cam_data.cam_display_box);

    /* Stop the camera preview. */
    error_code = camera_stop_preview(cam_data.g_camera);
    if (CAMERA_ERROR_NONE != error_code) {
      DLOG_PRINT_ERROR("camera_stop_preview", error_code);
      return;
    }

    cam_data.cam_prev = false;

    elm_object_text_set(cam_data.preview_bt, "Start preview");
  }
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
void _post_render_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
  Evas_Object **cam_data_image = (Evas_Object **) data;

  /* Get the size of the parent box. */
  int x = 0, y = 0, w = 0, h = 0;
  evas_object_geometry_get(obj, &x, &y, &w, &h);

  /* Set the size of the image object. */
  evas_object_resize(*cam_data_image, w, h);
  evas_object_move(*cam_data_image, 0, y);
}

/**
 * @brief Send the camera preview frame to the pipeline
 */
  static void
send_data_to_pipeline(camera_preview_data_s *frame)
{
  ml_tensors_data_h data;
  size_t data_size = frame->data.triple_plane.y_size + frame->data.triple_plane.u_size + frame->data.triple_plane.v_size;

  /* insert input data to appsrc */
  ml_tensors_data_create (info, &data);
  ml_tensors_data_set_tensor_data (data, 0, frame->data.triple_plane.y, data_size);
  ml_pipeline_src_input_data (srchandle, data, ML_PIPELINE_BUF_POLICY_AUTO_FREE);
}

/**
 * @brief Updates the display object based on the output of the pipeline
 */
  void
update_gui(void * data, void * buf, unsigned int size)
{
  int w, h;
  evas_object_image_size_get(cam_data.cam_display, &w, &h);
  unsigned char * img_src = evas_object_image_data_get(cam_data.cam_display, EINA_TRUE);
  memcpy(img_src, buf, w*h*4);
  evas_object_image_data_update_add(cam_data.cam_display, 0, 0, w, h);
}

/**
 * @brief Camera preview callback function to process the frame
 */
  static void
_camera_preview_cb(camera_preview_data_s *frame, void *user_data)
{
  send_data_to_pipeline(frame);
  //	 ecore_main_loop_thread_safe_call_async(send_data_to_pipeline, frame);
}

/**
 * @brief Creates the main view of the application.
 */
void create_buttons_in_main_window(void)
{
  /**
   * Create the window with camera preview and buttons for manipulating the
   * camera and taking the photo.
   */
  cam_data.display = _create_new_cd_display("Camera", NULL);

  /* Create a box for the camera preview. */
  cam_data.cam_display_box = elm_box_add(cam_data.display);
  elm_box_horizontal_set(cam_data.cam_display_box, EINA_FALSE);
  evas_object_size_hint_align_set(cam_data.cam_display_box, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(cam_data.cam_display_box, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_resize(cam_data.cam_display_box, IMG_WIDTH, IMG_HEIGHT);
  elm_box_pack_end(cam_data.display, cam_data.cam_display_box);

  Evas *evas = evas_object_evas_get(cam_data.cam_display_box);
  cam_data.cam_display = evas_object_image_filled_add(evas);

  /* Set a source file to fetch pixel data */

  unsigned char * img_src = malloc(IMG_WIDTH*IMG_HEIGHT*4);
  memset(img_src, 0, IMG_WIDTH*IMG_HEIGHT*4);
  evas_object_image_memfile_set (cam_data.cam_display, img_src, IMG_WIDTH*IMG_HEIGHT*4, NULL, NULL);
  free (img_src);

  evas_object_size_hint_weight_set(cam_data.cam_display, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_align_set(cam_data.cam_display, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_image_smooth_scale_set(cam_data.cam_display, EINA_FALSE);
  evas_object_image_colorspace_set(cam_data.cam_display, EVAS_COLORSPACE_ARGB8888);
  evas_object_image_size_set(cam_data.cam_display, IMG_WIDTH, IMG_HEIGHT);
  evas_object_image_alpha_set(cam_data.cam_display, EINA_TRUE);
  evas_object_image_fill_set(cam_data.cam_display, 0, 0, IMG_WIDTH, IMG_HEIGHT);
  evas_object_image_filled_set(cam_data.cam_display, EINA_TRUE);
  elm_box_pack_end(cam_data.cam_display_box, cam_data.cam_display);
  evas_object_show(cam_data.cam_display);

  /* Set the size and position of the image on the image object area */
  evas_object_resize(cam_data.cam_display, IMG_WIDTH, IMG_HEIGHT);
  evas_object_show(cam_data.cam_display);

  evas_object_event_callback_add(cam_data.cam_display_box,
      EVAS_CALLBACK_RESIZE, _post_render_cb, &(cam_data.cam_display));

  evas_object_resize(cam_data.cam_display_box, IMG_WIDTH, IMG_HEIGHT);
  evas_object_show(cam_data.cam_display_box);

  /* Create buttons for the Camera. */
  cam_data.preview_bt = _new_button(cam_data.display, "Start preview",
      __camera_cb_preview);

  /* Create the camera handle for the main camera of the device. */
  int error_code = camera_create(CAMERA_DEVICE_CAMERA0, &(cam_data.g_camera));
  if (CAMERA_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("camera_create", error_code);
    return;
  }

  /* Check the camera state after creating the handle. */
  camera_state_e state;
  error_code = camera_get_state(cam_data.g_camera, &state);
  if (CAMERA_ERROR_NONE != error_code || CAMERA_STATE_CREATED != state) {
    dlog_print(DLOG_ERROR, LOG_TAG,
        "camera_get_state() failed! Error code = %d, state = %s",
        error_code, _camera_state_to_string(state));
    return;
  }

  /**
   * Enable EXIF data storing during taking picture. This is required to edit
   * the orientation of the image.
   */
  error_code = camera_attr_enable_tag(cam_data.g_camera, true);
  if (CAMERA_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("camera_attr_enable_tag", error_code);
  }

  /**
   * Set the camera image orientation. Required (on Kiran device) to save the
   * image in regular orientation (without any rotation).
   */
  error_code = camera_attr_set_tag_orientation(cam_data.g_camera,
      CAMERA_ATTR_TAG_ORIENTATION_RIGHT_TOP);
  if (CAMERA_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("camera_attr_set_tag_orientation", error_code);
  }

  /* Set the picture quality attribute of the camera to maximum. */
  error_code = camera_attr_set_image_quality(cam_data.g_camera, 100);
  if (CAMERA_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("camera_attr_set_image_quality", error_code);
  }

  /**
   * Set the display for the camera preview.
   * However, set display type to NONE as we don't want to show camera image directly.
   * We only display the modified output image.
   */
  error_code = camera_set_display(cam_data.g_camera, CAMERA_DISPLAY_TYPE_NONE,
      GET_DISPLAY(cam_data.cam_display));
  if (CAMERA_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("camera_set_display", error_code);
    return;
  }

  error_code = camera_set_preview_resolution(cam_data.g_camera, 640,
      480);
  if (CAMERA_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("camera_set_preview_resolution", error_code);
  }

  /* Set the capture format for the camera. */
  error_code = camera_set_capture_format(cam_data.g_camera,
      CAMERA_PIXEL_FORMAT_JPEG);
  if (CAMERA_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("camera_set_capture_format", error_code);
  }

  /* Set camera preview cb. */
  error_code = camera_set_preview_cb(cam_data.g_camera,
      _camera_preview_cb, NULL);
  if (CAMERA_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("camera_set_preview_cb", error_code);
  }
}
