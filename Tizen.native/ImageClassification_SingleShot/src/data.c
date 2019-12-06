/**
 * @file data.c
 * @date 06 December 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include "main.h"
#include "data.h"
#include "my_file_selector.h"
#include <image_util.h>
#include <storage.h>

#include <glib.h>
#include <nnstreamer.h>
#include <nnstreamer-single.h>

#define BUFLEN 256
#define IMG_HEIGHT  224
#define IMG_WIDTH   224
#define IMG_CHANNEL 3

static Evas_Object *image;
static media_packet_h media_packet = NULL;
static media_packet_h packet_h = NULL;
static char image_file_path[BUFLEN];
static const char *sample_image_filename = "sample.jpg";

static bool invoke_finished = false;
static image_util_decode_h decode_h = NULL;
static transformation_h handle = NULL;
static gchar **labels;
static ml_single_h single;

static ml_tensors_data_h input_data;
static int label_num = -1;

/**
 * @brief get label data from the file
 */
void
init_label_data()
{
  GError *err = NULL;
  gchar *contents = NULL;
  char *res_path = app_get_resource_path();
  gchar *label_path = g_strdup_printf("%s/labels.txt", res_path);

  if (!g_file_get_contents(label_path, &contents, NULL, &err)) {
    dlog_print(DLOG_ERROR, LOG_TAG, "Unable to read file %s with error %s.",
        label_path, err->message);
    g_clear_error(&err);
  } else {
    labels = g_strsplit(contents, "\n", -1);
  }
  g_free(label_path);
  g_free(contents);
  free(res_path);
}

/**
 * @brief Sets the file that will be used.
 * @details Called when the file is chosen in the file selecting pop-up
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in EFL API.
 *
 * @param file_path The path to the file
 * @param data Additional data passed to the function(not used here)
 */
void
file_selected(const char *file_path, void *data)
{
  snprintf(image_file_path, BUFLEN, "%s", file_path);
  dlog_print(DLOG_DEBUG, LOG_TAG, "[%s:%d] selected file path: %s",
      __FUNCTION__, __LINE__, image_file_path);

  /* Set the path to the image for the EFL image object created earlier, to display the photo. */
  elm_image_file_set(image, image_file_path, NULL);

  /* Check whether an error occurred during loading of the photo. */
  Evas_Load_Error error = evas_object_image_load_error_get(image);

  if (EVAS_LOAD_ERROR_NONE != error) {
    dlog_print(DLOG_ERROR, LOG_TAG,
        "elm_image_file_set() failed! Could not load image: %s. Error message is \"%s\"\n",
        image_file_path, evas_load_error_str(error));
  } else {
    DLOG_PRINT_DEBUG_MSG("elm_image_file_set() successful. Image %s loaded.",
        image_file_path);
    evas_object_show(image);
  }
}

/**
 * @brief Shows the file selection pop-up.
 * @details Called when "Select file" button is clicked
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in EFL API.
 *
 * @param data The data passed to the callback
 * @param obj The object for which the 'clicked' event was triggered(not used here)
 * @param event_info Additional event information(not used here)
 */
static void
__thumbnail_util_select_file_cb(void *data, Evas_Object * obj,
    void *event_info)
{
  set_file_extension("jpg");
  set_file_selected_callback(file_selected);

  _show_file_popup();
}

/**
 * @brief Initialization function for data module.
 */
void
data_initialize(void)
{
  ml_tensors_info_h in_info;
  int error_code;
  char *res_path = app_get_resource_path();
  gchar *model_path = g_strdup_printf("%s/mobilenet_v1_1.0_224_quant.tflite",
      res_path);

  error_code = image_util_decode_create(&decode_h);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("Can't create the image decode handle.");
    DLOG_PRINT_ERROR("image_util_decode_create", error_code);
  }

  error_code = ml_single_open(&single, model_path, NULL, NULL,
      ML_NNFW_TYPE_TENSORFLOW_LITE, ML_NNFW_HW_ANY);
  if (error_code != ML_ERROR_NONE) {
    PRINT_MSG("Can't open the model file with single shot API.");
    DLOG_PRINT_ERROR("ml_single_open", error_code);
  }

  error_code = ml_single_get_input_info(single, &in_info);
  if (error_code != ML_ERROR_NONE) {
    PRINT_MSG("Can't get the information of input tensor.");
    DLOG_PRINT_ERROR("ml_single_get_input_info", error_code);
  }

  error_code = ml_tensors_data_create(in_info, &input_data);
  if (error_code != ML_ERROR_NONE) {
    PRINT_MSG("Can't create the tensor data for input tensor.");
    DLOG_PRINT_ERROR("ml_tensors_data_create", error_code);
  }

  ml_tensors_info_destroy(in_info);
  g_free(model_path);
  free(res_path);
  init_label_data();
}

/**
 * @brief Finalization function for data module.
 */
void
data_finalize(void)
{
  int error_code = image_util_transform_destroy(handle);

  error_code = image_util_decode_destroy(decode_h);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("Can't destroy the image decode handle.");
    DLOG_PRINT_ERROR("image_util_decode_destroy", error_code);
  }

  ml_tensors_data_destroy(input_data);
  ml_single_close(single);
  g_strfreev(labels);
}

/**
 * @brief Destroys the transformation handle, resources and enables the buttons.
 * @details Called once when the invoke is finished.
 * @remarks This function matches the Ecore_Task_Cb()
 *          type signature defined in the EFL API.
 *
 * @param data The user data passed via void pointer(not used here)
 *
 * @return @c EINA_TRUE on success
 */
Eina_Bool
_invoke_finished_check(void *data)
{
  if (invoke_finished) {

    image_util_transform_destroy(handle);
    handle = NULL;

    media_packet_destroy(media_packet);
    media_packet = NULL;

    media_packet_destroy(packet_h);
    packet_h = NULL;

    for (app_button i = 0; i < BUTTON_COUNT; ++i)
      _disable_button(i, EINA_FALSE);

    if (label_num >= 0) {
      PRINT_MSG("The Detected Label is <b>%s</b>", labels[label_num]);
      DLOG_PRINT_DEBUG_MSG("The Detected Label is %s", labels[label_num]);
    }
    invoke_finished = false;
  }
  return EINA_TRUE;
}

/**
 * @brief Get the label of the imported image after the transformation.
 * @details Called when the transformation of the image is finished.
 *
 * @param dst The result buffer of image util transform
 * @param error_code The error code of image util transform
 * @param user_data The user data passed from the callback registration function
 */
static void
_image_util_completed_cb(media_packet_h * dst, int error_code, void *user_data)
{
  int status;
  int max = -1;
  uint8_t *output_buf;
  size_t data_size;
  ml_tensors_data_h output_data;

  /* Get the buffer where the transformed image is stored. */
  void *packet_buffer = NULL;
  packet_h = *dst;

  error_code = media_packet_get_buffer_data_ptr(*dst, &packet_buffer);
  if (error_code != MEDIA_PACKET_ERROR_NONE) {
    DLOG_PRINT_ERROR("media_packet_get_buffer_data_ptr", error_code);
    image_util_transform_destroy(handle);
    handle = NULL;
    return;
  }

  status = ml_tensors_data_set_tensor_data(input_data, 0, packet_buffer,
      IMG_CHANNEL * IMG_HEIGHT * IMG_WIDTH);

  status = ml_single_invoke(single, input_data, &output_data);

  ml_tensors_data_get_tensor_data(output_data, 0, (void **) &output_buf,
      &data_size);

  for (int i = 0; i < data_size; i++) {
    if (output_buf[i] > 0 && output_buf[i] > max) {
      max = output_buf[i];
      label_num = i;
    }
  }

  ml_tensors_data_destroy(output_data);

  invoke_finished = true;
}

/**
 * @brief Executes the image transformations.
 * @details Called when clicking any button from the Image Util(except
 *          the "Reset" button).
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in the EFL API.
 *
 * @param data The button, which was clicked
 * @param object The object for which the 'clicked' event was triggered(not used here)
 * @param event_info Additional event information(not used here)
 */
static void
_image_util_start_cb(void *data, Evas_Object * obj, void *event_info)
{
  invoke_finished = false;

  for (app_button i = 0; i < BUTTON_COUNT; ++i)
    _disable_button(i, EINA_TRUE);

  /* Decode the given JPEG file to the img_source buffer. */
  unsigned char *img_source = NULL;
  unsigned long width, height;
  unsigned long long size_decode;

  int error_code = image_util_decode_set_input_path(decode_h, image_file_path);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("image_util_decode_set_input_path() failed.");
    DLOG_PRINT_ERROR("image_util_decode_set_input_path", error_code);
    return;
  }

  error_code = image_util_decode_set_output_buffer(decode_h, &img_source);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("image_util_decode_set_output_buffer() failed.");
    DLOG_PRINT_ERROR("image_util_decode_set_output_buffer", error_code);
    return;
  }

  error_code = image_util_decode_set_colorspace(decode_h,
      IMAGE_UTIL_COLORSPACE_RGB888);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("image_util_decode_set_colorspace() failed.");
    DLOG_PRINT_ERROR("image_util_decode_set_colorspace", error_code);
    return;
  }

  error_code = image_util_decode_run(decode_h, &width, &height, &size_decode);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("image_util_decode_run() failed.");
    DLOG_PRINT_ERROR("image_util_decode_run", error_code);
    return;
  }

  /* Create a media format structure. */
  media_format_h fmt;
  error_code = media_format_create(&fmt);
  if (error_code != MEDIA_FORMAT_ERROR_NONE) {
    PRINT_MSG("media_format_create() failed.");
    DLOG_PRINT_ERROR("media_format_create", error_code);
    free(img_source);
    return;
  }

  /* Set the MIME type of the created format. */
  error_code = media_format_set_video_mime(fmt, MEDIA_FORMAT_RGB888);
  if (error_code != MEDIA_FORMAT_ERROR_NONE) {
    PRINT_MSG("media_format_set_video_mime() failed.");
    DLOG_PRINT_ERROR("media_format_set_video_mime", error_code);
    media_format_unref(fmt);
    free(img_source);
    return;
  }

  /* Set the width of the created format. */
  error_code = media_format_set_video_width(fmt, width);
  if (error_code != MEDIA_FORMAT_ERROR_NONE) {
    PRINT_MSG("media_format_set_video_width() failed.");
    DLOG_PRINT_ERROR("media_format_set_video_width", error_code);
    media_format_unref(fmt);
    free(img_source);
    return;
  }

  /* Set the height of the created format. */
  error_code = media_format_set_video_height(fmt, height);
  if (error_code != MEDIA_FORMAT_ERROR_NONE) {
    PRINT_MSG("media_format_set_video_height() failed.");
    DLOG_PRINT_ERROR("media_format_set_video_height", error_code);
    media_format_unref(fmt);
    free(img_source);
    return;
  }

  /* Create a media packet with the image. */
  error_code = media_packet_create_alloc(fmt, NULL, NULL, &media_packet);
  if (error_code != MEDIA_PACKET_ERROR_NONE) {
    PRINT_MSG("media_packet_create_alloc() failed.");
    DLOG_PRINT_ERROR("media_packet_create_alloc", error_code);
    media_format_unref(fmt);
    free(img_source);
    return;
  }

  media_format_unref(fmt);

  /* Get the pointer to the internal media packet buffer, where the image will be stored. */
  void *packet_buffer = NULL;

  error_code = media_packet_get_buffer_data_ptr(media_packet, &packet_buffer);
  if (error_code != MEDIA_PACKET_ERROR_NONE || NULL == packet_buffer) {
    PRINT_MSG("media_packet_get_buffer_data_ptr() failed.");
    DLOG_PRINT_ERROR("media_packet_get_buffer_data_ptr", error_code);
    free(img_source);
    return;
  }

  /* Copy the image content to the media_packet internal buffer. */
  memcpy(packet_buffer, (void *) img_source, size_decode);
  free(img_source);

  /* Create a handle to the transformation. */
  error_code = image_util_transform_create(&handle);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("image_util_transform_create() failed.");
    DLOG_PRINT_ERROR("image_util_transform_create", error_code);
    return;
  }

  error_code = image_util_transform_set_resolution(handle, IMG_WIDTH,
      IMG_HEIGHT);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("image_util_transform_set_resolution() failed.");
    DLOG_PRINT_ERROR("image_util_transform_set_resolution", error_code);
  }

  error_code = image_util_transform_run(handle, media_packet,
      _image_util_completed_cb, NULL);
  if (error_code != IMAGE_UTIL_ERROR_NONE) {
    PRINT_MSG("image_util_transform_run() failed.");
    DLOG_PRINT_ERROR("image_util_transform_run", error_code);
  }
}

/**
 * @brief Creates the application main view.
 */
void
create_buttons_in_main_window()
{
  /* Create the window for the Image Util. */
  Evas_Object *display = _create_new_cd_display("NNStreamer", NULL);
  evas_object_size_hint_weight_set(display, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);

  /* Create buttons for the Image Util. */
  _create_button(SELECT_IMAGE_BTN, display, "Search Image File(*.jpg)",
      __thumbnail_util_select_file_cb);
  _create_button(GET_LABEL_BTN, display, "Get Image Label Result",
      _image_util_start_cb);

  /* Create the top box. */
  Evas_Object *top_box = elm_box_add(display);
  evas_object_size_hint_align_set(top_box, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(top_box, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_box_pack_end(display, top_box);
  evas_object_show(top_box);

  /* Create table for displaying image on the background. */
  Evas_Object *table = elm_table_add(top_box);
  elm_table_homogeneous_set(table, EINA_TRUE);
  evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
  elm_box_pack_end(top_box, table);
  evas_object_show(table);

  /* Fill the top box with black background. */
  Evas_Object *bg = elm_bg_add(table);
  evas_object_size_hint_align_set(bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_bg_color_set(bg, 0, 0, 0);
  elm_table_pack(table, bg, 0, 0, 1, 1);
  evas_object_show(bg);

  /* Create an image object for displaying the image on it. */
  image = elm_image_add(table);
  evas_object_size_hint_align_set(image, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(image, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  elm_table_pack(table, image, 0, 0, 1, 1);
  evas_object_show(image);

  /* Get the path to the sample image stored in the resources. */
  char *resource_path = app_get_resource_path();
  snprintf(image_file_path, BUFLEN, "%s%s", resource_path,
      sample_image_filename);

  /* Set the path to the image file that will be used as a source by the EFL image object. */
  elm_image_file_set(image, image_file_path, NULL);

  /* Check whether any error occurred during loading the image. */
  Evas_Load_Error error = evas_object_image_load_error_get(image);

  if (EVAS_LOAD_ERROR_NONE != error) {
    DLOG_PRINT_DEBUG_MSG
       ("elm_image_file_set() failed! Could not load image: %s. Error message is \"%s\"\n",
        image_file_path, evas_load_error_str(error));
  } else {
    DLOG_PRINT_DEBUG_MSG("elm_image_file_set() successful. Image %s loaded.",
        image_file_path);
    evas_object_show(image);
  }
  ecore_idler_add(_invoke_finished_check, NULL);
}

/**
 * @brief Loads the image which will be used as a source to the display.
 * @details Called when the 'Reset' button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in the EFL API.
 *
 * @param data The user data passed via void pointer(not used here)
 * @param object The object for which the 'clicked' event was triggered(not used here)
 * @param event_info Additional event information(not used here)
 */
void
_image_util_reset_cb(void *data, Evas_Object * obj, void *event_info)
{
  /* Get the path to the sample image stored in the resources. */
  char *resource_path = app_get_resource_path();
  snprintf(image_file_path, BUFLEN, "%s/%s", resource_path,
      sample_image_filename);
  free(resource_path);

  /* Set the path to the image file that will be used as a source by the EFL image object. */
  elm_image_file_set(image, image_file_path, NULL);

  /* Check whether any error occurred during loading the photo. */
  Evas_Load_Error error = evas_object_image_load_error_get(image);

  if (EVAS_LOAD_ERROR_NONE != error) {
    dlog_print(DLOG_ERROR, LOG_TAG,
        "elm_image_file_set() failed! Could not load image: %s. Error message is \"%s\"\n",
        image_file_path, evas_load_error_str(error));
  } else {
    DLOG_PRINT_DEBUG_MSG("elm_image_file_set() successful. Image %s loaded.",
        image_file_path);
    evas_object_show(image);
  }

  for (app_button i = 0; i < BUTTON_COUNT; ++i)
    _disable_button(i, EINA_FALSE);
}
