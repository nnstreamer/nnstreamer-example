/* GStreamer android.hardware.Camera2 Source
 * Copyright (C) 2017, Collabora Ltd.
 *   Author:Justin Kim <justin.kim@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstahc2src.h"

#include <math.h>
#include <stdbool.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include <gst/interfaces/photography.h>

#include <camera/NdkCameraDevice.h>
#include <media/NdkMediaError.h>
/* FIXME: NdkCameraManager.h uses 'ACameraDevice_StateCallbacks'
 * instead of a small 's'.*/
#ifndef ACameraDevice_StateCallbacks
#define ACameraDevice_StateCallbacks ACameraDevice_stateCallbacks
#endif
#include <camera/NdkCameraManager.h>

#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#define DEFAULT_MAX_IMAGES 5

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define GST_CAT_DEFAULT gst_debug_ahc2_src
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

struct _GstAHC2SrcPrivate
{
  GstDataQueue *outbound_queue;
  GstClockTime previous_ts;
  gboolean started;
  GMutex mutex;
  gint max_images;

  gint camera_index;
  ACameraDevice *camera_device;
  ACameraManager *camera_manager;
  ACameraIdList *camera_id_list;
  gint camera_template_type;
  ACameraOutputTarget *camera_output_target;
  ACameraCaptureSession *camera_capture_session;

  AImageReader *image_reader;
  AImageReader_ImageListener image_reader_listener;

  ACaptureRequest *capture_request;
  ACaptureSessionOutput *capture_sout;
  ACaptureSessionOutputContainer *capture_sout_container;

  ACameraDevice_stateCallbacks device_state_cb;
  ACameraCaptureSession_stateCallbacks session_state_cb;
};

typedef struct
{
  gsize refcount;
  GstAHC2Src *ahc2src;
  AImage *image;
} GstWrappedAImage;

#define GST_TYPE_WRAPPED_AIMAGE (gst_wrapped_aimage_get_type ())
GType gst_wrapped_aimage_get_type (void);

static GstWrappedAImage *
gst_wrapped_aimage_ref (GstWrappedAImage * self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ahc2src != NULL, NULL);
  g_return_val_if_fail (self->image != NULL, NULL);
  g_return_val_if_fail (self->refcount >= 1, NULL);

  GST_TRACE_OBJECT (self->ahc2src,
      "ref GstWrappedAImage %p, refcount: %" G_GSIZE_FORMAT, self,
      self->refcount);

  self->refcount++;
  return self;
}

static void
gst_wrapped_aimage_unref (GstWrappedAImage * self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ahc2src != NULL);
  g_return_if_fail (self->image != NULL);
  g_return_if_fail (self->refcount >= 1);

  GST_TRACE_OBJECT (self->ahc2src,
      "unref GstWrappedAImage %p, refcount: %" G_GSIZE_FORMAT, self,
      self->refcount);
  if (--self->refcount == 0) {
    gst_object_unref (self->ahc2src);
    g_clear_pointer (&self->image, (GDestroyNotify) AImage_delete);
  }
}

G_DEFINE_BOXED_TYPE (GstWrappedAImage, gst_wrapped_aimage,
    gst_wrapped_aimage_ref, gst_wrapped_aimage_unref)
#define GST_AHC2_SRC_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_AHC2_SRC, GstAHC2SrcPrivate))

static void gst_ahc2_src_photography_init (gpointer g_iface,
    gpointer iface_data);

#define gst_ahc2_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAHC2Src, gst_ahc2_src, GST_TYPE_PUSH_SRC,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PHOTOGRAPHY, gst_ahc2_src_photography_init)
    G_ADD_PRIVATE (GstAHC2Src)
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "ahc2src", 0, "AHC2 Source"))

typedef enum
{
  PROP_CAMERA_INDEX = 1,
  PROP_N_CAMERAS,
  PROP_CAMERA_TEMPLATE_TYPE,
  PROP_MAX_IMAGES,

  /* override photography properties */
  PROP_ANALOG_GAIN,
  PROP_APERTURE,
  PROP_CAPABILITIES,
  PROP_COLOR_TEMPERATURE,
  PROP_COLOR_TONE_MODE,
  PROP_EV_COMPENSATION,
  PROP_EXPOSURE_TIME,
  PROP_FLASH_MODE,
  PROP_FLICKER_MODE,
  PROP_FOCUS_MODE,
  PROP_IMAGE_CAPTURE_SUPPORTED_CAPS,
  PROP_IMAGE_PREVIEW_SUPPORTED_CAPS,
  PROP_ISO_SPEED,
  PROP_LENS_FOCUS,
  PROP_MAX_EXPOSURE_TIME,
  PROP_MIN_EXPOSURE_TIME,
  PROP_NOISE_REDUCTION,
  PROP_SCENE_MODE,
  PROP_WHITE_BALANCE_MODE,
  PROP_WHITE_POINT,
  PROP_ZOOM,

  /*< private > */
  PROP_N_INSTALL = PROP_MAX_IMAGES + 1,
  PROP_LAST = PROP_ZOOM,
} GstAHC2SrcProperty;

static GParamSpec *properties[PROP_LAST + 1];

enum
{
  SIG_GET_CAMERA_ID_BY_INDEX,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GstPhotographyCaps
gst_ahc2_src_get_capabilities (GstPhotography * photo)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  GstPhotographyCaps caps = GST_PHOTOGRAPHY_CAPS_EV_COMP |
      GST_PHOTOGRAPHY_CAPS_WB_MODE | GST_PHOTOGRAPHY_CAPS_TONE |
      GST_PHOTOGRAPHY_CAPS_SCENE | GST_PHOTOGRAPHY_CAPS_FLASH |
      GST_PHOTOGRAPHY_CAPS_FOCUS;

  ACameraMetadata *metadata = NULL;
  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->camera_manager != NULL, caps);
  g_return_val_if_fail (priv->camera_id_list != NULL, caps);
  g_return_val_if_fail (priv->camera_index < priv->camera_id_list->numCameras,
      caps);

  if (ACameraManager_getCameraCharacteristics (priv->camera_manager,
          priv->camera_id_list->cameraIds[priv->camera_index],
          &metadata) != ACAMERA_OK) {
    return caps;
  }

  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &entry) == ACAMERA_OK) {
    if (entry.count > 1) {
      caps |= GST_PHOTOGRAPHY_CAPS_ZOOM;
    }
  }

  g_clear_pointer (&metadata, (GDestroyNotify) ACameraMetadata_free);

  return caps;
}

static gboolean
gst_ahc2_src_get_color_tone_mode (GstPhotography * photo,
    GstPhotographyColorToneMode * tone_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACaptureRequest_getConstEntry (priv->capture_request,
          ACAMERA_CONTROL_EFFECT_MODE, &entry) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get EFFECT MODE");
    return FALSE;
  }

  switch (entry.data.u8[0]) {
    case ACAMERA_CONTROL_EFFECT_MODE_OFF:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_NORMAL;
      break;
    case ACAMERA_CONTROL_EFFECT_MODE_MONO:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_GRAYSCALE;
      break;
    case ACAMERA_CONTROL_EFFECT_MODE_NEGATIVE:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_NEGATIVE;
      break;
    case ACAMERA_CONTROL_EFFECT_MODE_SOLARIZE:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_SOLARIZE;
      break;
    case ACAMERA_CONTROL_EFFECT_MODE_SEPIA:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_SEPIA;
      break;
    case ACAMERA_CONTROL_EFFECT_MODE_POSTERIZE:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_POSTERIZE;
      break;
    case ACAMERA_CONTROL_EFFECT_MODE_WHITEBOARD:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_WHITEBOARD;
      break;
    case ACAMERA_CONTROL_EFFECT_MODE_BLACKBOARD:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_BLACKBOARD;
      break;
    case ACAMERA_CONTROL_EFFECT_MODE_AQUA:
      *tone_mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_AQUA;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

static gboolean
gst_ahc2_src_set_color_tone_mode (GstPhotography * photo,
    GstPhotographyColorToneMode tone_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  guint8 mode;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  switch (tone_mode) {
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_NORMAL:
      mode = ACAMERA_CONTROL_EFFECT_MODE_OFF;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_SEPIA:
      mode = ACAMERA_CONTROL_EFFECT_MODE_SEPIA;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_NEGATIVE:
      mode = ACAMERA_CONTROL_EFFECT_MODE_NEGATIVE;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_GRAYSCALE:
      mode = ACAMERA_CONTROL_EFFECT_MODE_MONO;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_SOLARIZE:
      mode = ACAMERA_CONTROL_EFFECT_MODE_SOLARIZE;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_POSTERIZE:
      mode = GST_PHOTOGRAPHY_COLOR_TONE_MODE_POSTERIZE;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_WHITEBOARD:
      mode = ACAMERA_CONTROL_EFFECT_MODE_WHITEBOARD;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_BLACKBOARD:
      mode = ACAMERA_CONTROL_EFFECT_MODE_BLACKBOARD;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_AQUA:
      mode = ACAMERA_CONTROL_EFFECT_MODE_AQUA;
      break;
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_NATURAL:
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_VIVID:
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_COLORSWAP:
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_OUT_OF_FOCUS:
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_SKY_BLUE:
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_GRASS_GREEN:
    case GST_PHOTOGRAPHY_COLOR_TONE_MODE_SKIN_WHITEN:
    default:
      GST_WARNING_OBJECT (self, "Unsupported color tone (%d)", tone_mode);
      return FALSE;
  }

  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  ACaptureRequest_setEntry_u8 (priv->capture_request,
      ACAMERA_CONTROL_EFFECT_MODE, 1, &mode);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  return TRUE;
}

static void
gst_ahc2_src_set_autofocus (GstPhotography * photo, gboolean on)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  guint8 mode = on ? ACAMERA_CONTROL_AF_MODE_AUTO : ACAMERA_CONTROL_AF_MODE_OFF;

  g_return_if_fail (priv->capture_request != NULL);

  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  ACaptureRequest_setEntry_u8 (priv->capture_request,
      ACAMERA_CONTROL_AF_MODE, 1, &mode);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);
}

static gboolean
gst_ahc2_src_get_focus_mode (GstPhotography * photo,
    GstPhotographyFocusMode * focus_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACaptureRequest_getConstEntry (priv->capture_request,
          ACAMERA_CONTROL_AF_MODE, &entry) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get AF MODE");
    return FALSE;
  }

  /* GstPhotographyFocusMode doesn't matches
   * acamera_metadata_enum_acamera_control_af_mode.  */
  switch (entry.data.u8[0]) {
    case ACAMERA_CONTROL_AF_MODE_OFF:
      *focus_mode = GST_PHOTOGRAPHY_FOCUS_MODE_MANUAL;
      break;
    case ACAMERA_CONTROL_AF_MODE_AUTO:
      *focus_mode = GST_PHOTOGRAPHY_FOCUS_MODE_AUTO;
      break;
    case ACAMERA_CONTROL_AF_MODE_MACRO:
      *focus_mode = GST_PHOTOGRAPHY_FOCUS_MODE_MACRO;
      break;
    case ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
      *focus_mode = GST_PHOTOGRAPHY_FOCUS_MODE_HYPERFOCAL;
      break;
    case ACAMERA_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
      *focus_mode = GST_PHOTOGRAPHY_FOCUS_MODE_CONTINUOUS_NORMAL;
      break;
    case ACAMERA_CONTROL_AF_MODE_EDOF:
      *focus_mode = GST_PHOTOGRAPHY_FOCUS_MODE_EXTENDED;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

static gboolean
gst_ahc2_src_set_focus_mode (GstPhotography * photo,
    GstPhotographyFocusMode focus_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  guint8 mode;
  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  switch (focus_mode) {
    case GST_PHOTOGRAPHY_FOCUS_MODE_AUTO:
      mode = ACAMERA_CONTROL_AF_MODE_AUTO;
      break;
    case GST_PHOTOGRAPHY_FOCUS_MODE_MACRO:
      mode = ACAMERA_CONTROL_AF_MODE_MACRO;
      break;
    case GST_PHOTOGRAPHY_FOCUS_MODE_PORTRAIT:
      mode = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
      break;
    case GST_PHOTOGRAPHY_FOCUS_MODE_INFINITY:
    case GST_PHOTOGRAPHY_FOCUS_MODE_HYPERFOCAL:
    case GST_PHOTOGRAPHY_FOCUS_MODE_CONTINUOUS_NORMAL:
    case GST_PHOTOGRAPHY_FOCUS_MODE_CONTINUOUS_EXTENDED:
      mode = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
      break;
    case GST_PHOTOGRAPHY_FOCUS_MODE_MANUAL:
      mode = ACAMERA_CONTROL_AF_MODE_OFF;
      break;
    case GST_PHOTOGRAPHY_FOCUS_MODE_EXTENDED:
      mode = ACAMERA_CONTROL_AF_MODE_EDOF;
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  ACaptureRequest_setEntry_u8 (priv->capture_request,
      ACAMERA_CONTROL_AF_MODE, 1, &mode);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  return TRUE;
}

static gboolean
gst_ahc2_src_get_ev_compensation (GstPhotography * photo, gfloat * ev_comp)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  ACameraMetadata *metadata = NULL;
  ACameraMetadata_const_entry entry;
  gint ev;
  ACameraMetadata_rational ev_step;
  gboolean ret = FALSE;

  g_return_val_if_fail (priv->camera_manager != NULL, FALSE);
  g_return_val_if_fail (priv->camera_id_list != NULL, FALSE);
  g_return_val_if_fail (priv->camera_index < priv->camera_id_list->numCameras,
      FALSE);
  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACameraManager_getCameraCharacteristics (priv->camera_manager,
          priv->camera_id_list->cameraIds[priv->camera_index],
          &metadata) != ACAMERA_OK) {
    GST_ERROR_OBJECT (self, "Failed to get metadata object");
    goto out;
  }

  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_CONTROL_AE_COMPENSATION_STEP, &entry) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get EV STEP");
    goto out;
  }

  ev_step = entry.data.r[0];

  if (ACaptureRequest_getConstEntry (priv->capture_request,
          ACAMERA_CONTROL_AE_EXPOSURE_COMPENSATION, &entry) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get EV COMPENSATION");
    goto out;
  }

  ev = entry.data.i32[0];

  *ev_comp = (gfloat) (ev * ev_step.numerator / ev_step.denominator);

  ret = TRUE;
out:
  g_clear_pointer (&metadata, (GDestroyNotify) ACameraMetadata_free);
  return ret;
}

static gboolean
gst_ahc2_src_set_ev_compensation (GstPhotography * photo, gfloat ev_comp)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  ACameraMetadata *metadata = NULL;
  ACameraMetadata_const_entry entry;
  gint ev, ev_min, ev_max;
  ACameraMetadata_rational ev_step;
  gboolean ret = FALSE;

  g_return_val_if_fail (priv->camera_manager != NULL, FALSE);
  g_return_val_if_fail (priv->camera_id_list != NULL, FALSE);
  g_return_val_if_fail (priv->camera_index < priv->camera_id_list->numCameras,
      FALSE);
  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACameraManager_getCameraCharacteristics (priv->camera_manager,
          priv->camera_id_list->cameraIds[priv->camera_index],
          &metadata) != ACAMERA_OK) {
    GST_ERROR_OBJECT (self, "Failed to get metadata object");
    goto out;
  }

  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_CONTROL_AE_COMPENSATION_STEP, &entry) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get EV STEP");
    goto out;
  }

  ev_step = entry.data.r[0];

  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_CONTROL_AE_COMPENSATION_RANGE, &entry) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get EV RANGE");
    goto out;
  }

  ev_min = entry.data.i32[0];
  ev_max = entry.data.i32[1];

  ev = (gint) round (ev_comp / ev_step.numerator * ev_step.denominator);

  if (ev < ev_min) {
    GST_WARNING_OBJECT (self, "converted EV(%d) is less than min(%d)", ev,
        ev_min);
    ev = ev_min;
  }

  if (ev > ev_max) {
    GST_WARNING_OBJECT (self, "converted EV(%d) is greater than max(%d)", ev,
        ev_max);
    ev = ev_max;
  }

  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  ACaptureRequest_setEntry_i32 (priv->capture_request,
      ACAMERA_CONTROL_AE_EXPOSURE_COMPENSATION, 1, &ev);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  ret = TRUE;
out:
  g_clear_pointer (&metadata, (GDestroyNotify) ACameraMetadata_free);
  return ret;
}

static gboolean
gst_ahc2_src_get_flash_mode (GstPhotography * photo,
    GstPhotographyFlashMode * flash_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACaptureRequest_getConstEntry (priv->capture_request,
          ACAMERA_FLASH_MODE, &entry) != ACAMERA_OK) {
    GST_ERROR_OBJECT (self, "Failed to get FLASH MODE");
    return FALSE;
  }

  switch (entry.data.u8[0]) {
    case ACAMERA_FLASH_MODE_OFF:
      *flash_mode = GST_PHOTOGRAPHY_FLASH_MODE_OFF;
      break;
    case ACAMERA_FLASH_MODE_SINGLE:
      *flash_mode = GST_PHOTOGRAPHY_FLASH_MODE_AUTO;
      break;
    case ACAMERA_FLASH_MODE_TORCH:
      *flash_mode = GST_PHOTOGRAPHY_FLASH_MODE_ON;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

static gboolean
gst_ahc2_src_set_flash_mode (GstPhotography * photo,
    GstPhotographyFlashMode flash_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  guint8 mode;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  switch (flash_mode) {
    case GST_PHOTOGRAPHY_FLASH_MODE_OFF:
      mode = ACAMERA_FLASH_MODE_OFF;
      break;
    case GST_PHOTOGRAPHY_FLASH_MODE_ON:
      mode = ACAMERA_FLASH_MODE_TORCH;
      break;
    case GST_PHOTOGRAPHY_FLASH_MODE_AUTO:
    case GST_PHOTOGRAPHY_FLASH_MODE_FILL_IN:
    case GST_PHOTOGRAPHY_FLASH_MODE_RED_EYE:
      mode = ACAMERA_FLASH_MODE_SINGLE;
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  ACaptureRequest_setEntry_u8 (priv->capture_request,
      ACAMERA_FLASH_MODE, 1, &mode);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  return TRUE;
}

static gboolean
gst_ahc2_src_get_flicker_mode (GstPhotography * photo,
    GstPhotographyFlickerReductionMode * flicker_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACaptureRequest_getConstEntry (priv->capture_request,
          ACAMERA_CONTROL_AE_ANTIBANDING_MODE, &entry) != ACAMERA_OK) {
    GST_ERROR_OBJECT (self, "Failed to get ANTIBANDING MODE");
    return FALSE;
  }

  switch (entry.data.u8[0]) {
    case ACAMERA_CONTROL_AE_ANTIBANDING_MODE_OFF:
      *flicker_mode = GST_PHOTOGRAPHY_FLICKER_REDUCTION_OFF;
      break;
    case ACAMERA_CONTROL_AE_ANTIBANDING_MODE_50HZ:
      *flicker_mode = GST_PHOTOGRAPHY_FLICKER_REDUCTION_50HZ;
      break;
    case ACAMERA_CONTROL_AE_ANTIBANDING_MODE_60HZ:
      *flicker_mode = GST_PHOTOGRAPHY_FLICKER_REDUCTION_60HZ;
      break;
    case ACAMERA_CONTROL_AE_ANTIBANDING_MODE_AUTO:
      *flicker_mode = GST_PHOTOGRAPHY_FLICKER_REDUCTION_AUTO;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

static gboolean
gst_ahc2_src_set_flicker_mode (GstPhotography * photo,
    GstPhotographyFlickerReductionMode flicker_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  guint8 mode;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  switch (flicker_mode) {
    case GST_PHOTOGRAPHY_FLICKER_REDUCTION_OFF:
      mode = ACAMERA_CONTROL_AE_ANTIBANDING_MODE_OFF;
      break;
    case GST_PHOTOGRAPHY_FLICKER_REDUCTION_50HZ:
      mode = ACAMERA_CONTROL_AE_ANTIBANDING_MODE_50HZ;
      break;
    case GST_PHOTOGRAPHY_FLICKER_REDUCTION_60HZ:
      mode = ACAMERA_CONTROL_AE_ANTIBANDING_MODE_60HZ;
      break;
    case GST_PHOTOGRAPHY_FLICKER_REDUCTION_AUTO:
      mode = ACAMERA_CONTROL_AE_ANTIBANDING_MODE_AUTO;
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported flicker-mode (%d)", flicker_mode);
      break;
  }

  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  ACaptureRequest_setEntry_u8 (priv->capture_request,
      ACAMERA_CONTROL_AE_ANTIBANDING_MODE, 1, &mode);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  return TRUE;
}

static gboolean
gst_ahc2_src_get_scene_mode (GstPhotography * photo,
    GstPhotographySceneMode * scene_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACaptureRequest_getConstEntry (priv->capture_request,
          ACAMERA_CONTROL_SCENE_MODE, &entry) != ACAMERA_OK) {
    GST_ERROR_OBJECT (self, "Failed to get SCENE MODE");
    return FALSE;
  }

  switch (entry.data.u8[0]) {
    case ACAMERA_CONTROL_SCENE_MODE_DISABLED:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_MANUAL;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_ACTION:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_ACTION;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_PORTRAIT:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_LANDSCAPE:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_LANDSCAPE;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_NIGHT:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_NIGHT;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_NIGHT_PORTRAIT:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_NIGHT_PORTRAIT;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_THEATRE:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_THEATRE;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_BEACH:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_BEACH;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_SNOW:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_SNOW;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_SUNSET:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_SUNSET;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_STEADYPHOTO:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_STEADY_PHOTO;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_FIREWORKS:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_FIREWORKS;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_SPORTS:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_SPORT;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_PARTY:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_PARTY;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_CANDLELIGHT:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_CANDLELIGHT;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_BARCODE:
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_BARCODE;
      break;
    case ACAMERA_CONTROL_SCENE_MODE_HDR:
    case ACAMERA_CONTROL_SCENE_MODE_FACE_PRIORITY:
      GST_WARNING_OBJECT (self,
          "Device returns scene mode (%d), but no proper interface is defined",
          entry.data.u8[0]);
      *scene_mode = GST_PHOTOGRAPHY_SCENE_MODE_MANUAL;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return TRUE;
}

static gboolean
gst_ahc2_src_set_scene_mode (GstPhotography * photo,
    GstPhotographySceneMode scene_mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  guint8 mode;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  switch (scene_mode) {
    case GST_PHOTOGRAPHY_SCENE_MODE_MANUAL:
      mode = ACAMERA_CONTROL_SCENE_MODE_DISABLED;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_PORTRAIT:
      mode = ACAMERA_CONTROL_SCENE_MODE_PORTRAIT;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_LANDSCAPE:
      mode = ACAMERA_CONTROL_SCENE_MODE_LANDSCAPE;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_SPORT:
      mode = ACAMERA_CONTROL_SCENE_MODE_SPORTS;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_NIGHT:
      mode = ACAMERA_CONTROL_SCENE_MODE_NIGHT;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_ACTION:
      mode = ACAMERA_CONTROL_SCENE_MODE_ACTION;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_NIGHT_PORTRAIT:
      mode = ACAMERA_CONTROL_SCENE_MODE_NIGHT_PORTRAIT;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_THEATRE:
      mode = ACAMERA_CONTROL_SCENE_MODE_THEATRE;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_BEACH:
      mode = ACAMERA_CONTROL_SCENE_MODE_BEACH;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_SNOW:
      mode = ACAMERA_CONTROL_SCENE_MODE_SNOW;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_SUNSET:
      mode = ACAMERA_CONTROL_SCENE_MODE_SUNSET;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_STEADY_PHOTO:
      mode = ACAMERA_CONTROL_SCENE_MODE_STEADYPHOTO;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_FIREWORKS:
      mode = ACAMERA_CONTROL_SCENE_MODE_FIREWORKS;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_PARTY:
      mode = ACAMERA_CONTROL_SCENE_MODE_PARTY;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_CANDLELIGHT:
      mode = ACAMERA_CONTROL_SCENE_MODE_CANDLELIGHT;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_BARCODE:
      mode = ACAMERA_CONTROL_SCENE_MODE_BARCODE;
      break;
    case GST_PHOTOGRAPHY_SCENE_MODE_AUTO:
    case GST_PHOTOGRAPHY_SCENE_MODE_CLOSEUP:
    default:
      GST_WARNING_OBJECT (self, "Unsupported scene mode (%d)", scene_mode);
      return FALSE;
  }

  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  ACaptureRequest_setEntry_u8 (priv->capture_request,
      ACAMERA_CONTROL_SCENE_MODE, 1, &mode);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  return TRUE;
}

static gboolean
gst_ahc2_src_get_white_balance_mode (GstPhotography * photo,
    GstPhotographyWhiteBalanceMode * mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACaptureRequest_getConstEntry (priv->capture_request,
          ACAMERA_CONTROL_AWB_MODE, &entry) != ACAMERA_OK) {
    GST_ERROR_OBJECT (self, "Failed to get AWB_MODE");
    return FALSE;
  }

  switch (entry.data.u8[0]) {
    case ACAMERA_CONTROL_AWB_MODE_OFF:
      *mode = GST_PHOTOGRAPHY_WB_MODE_MANUAL;
      break;
    case ACAMERA_CONTROL_AWB_MODE_AUTO:
      *mode = GST_PHOTOGRAPHY_WB_MODE_AUTO;
      break;
    case ACAMERA_CONTROL_AWB_MODE_INCANDESCENT:
      *mode = GST_PHOTOGRAPHY_WB_MODE_TUNGSTEN;
      break;
    case ACAMERA_CONTROL_AWB_MODE_FLUORESCENT:
      *mode = GST_PHOTOGRAPHY_WB_MODE_FLUORESCENT;
      break;
    case ACAMERA_CONTROL_AWB_MODE_WARM_FLUORESCENT:
      *mode = GST_PHOTOGRAPHY_WB_MODE_WARM_FLUORESCENT;
      break;
    case ACAMERA_CONTROL_AWB_MODE_DAYLIGHT:
      *mode = GST_PHOTOGRAPHY_WB_MODE_DAYLIGHT;
      break;
    case ACAMERA_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT:
      *mode = GST_PHOTOGRAPHY_WB_MODE_CLOUDY;
      break;
    case ACAMERA_CONTROL_AWB_MODE_TWILIGHT:
      *mode = GST_PHOTOGRAPHY_WB_MODE_SUNSET;
      break;
    case ACAMERA_CONTROL_AWB_MODE_SHADE:
      *mode = GST_PHOTOGRAPHY_WB_MODE_SHADE;
      break;
    default:
      GST_WARNING_OBJECT (self, "Uknown AWB parameter (0x%x)",
          entry.data.u8[0]);
      return FALSE;
      break;
  }

  GST_DEBUG_OBJECT (self, "AWB parameter (0x%x -> 0x%x)", entry.data.u8[0],
      *mode);

  return TRUE;
}

static gboolean
gst_ahc2_src_set_white_balance_mode (GstPhotography * photo,
    GstPhotographyWhiteBalanceMode mode)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  guint8 wb_mode;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  switch (mode) {
    case GST_PHOTOGRAPHY_WB_MODE_AUTO:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_AUTO;
      break;
    case GST_PHOTOGRAPHY_WB_MODE_DAYLIGHT:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_DAYLIGHT;
      break;
    case GST_PHOTOGRAPHY_WB_MODE_CLOUDY:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT;
      break;
    case GST_PHOTOGRAPHY_WB_MODE_SUNSET:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_TWILIGHT;
      break;
    case GST_PHOTOGRAPHY_WB_MODE_TUNGSTEN:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_INCANDESCENT;
      break;
    case GST_PHOTOGRAPHY_WB_MODE_FLUORESCENT:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_FLUORESCENT;
      break;
    case GST_PHOTOGRAPHY_WB_MODE_MANUAL:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_OFF;
      break;
    case GST_PHOTOGRAPHY_WB_MODE_WARM_FLUORESCENT:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_WARM_FLUORESCENT;
      break;
    case GST_PHOTOGRAPHY_WB_MODE_SHADE:
      wb_mode = ACAMERA_CONTROL_AWB_MODE_SHADE;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  ACaptureRequest_setEntry_u8 (priv->capture_request,
      ACAMERA_CONTROL_AWB_MODE, 1, &wb_mode);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  return TRUE;
}

static gboolean
gst_ahc2_src_get_zoom (GstPhotography * photo, gfloat * zoom)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACaptureRequest_getConstEntry (priv->capture_request,
          ACAMERA_LENS_FOCAL_LENGTH, &entry) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get FOCAL LENGTH (optical zoom)");
    return FALSE;
  }

  *zoom = entry.data.f[0];

  return TRUE;
}

static gboolean
gst_ahc2_src_set_zoom (GstPhotography * photo, gfloat zoom)
{
  GstAHC2Src *self = GST_AHC2_SRC (photo);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  gboolean ret = FALSE;
  ACameraMetadata *metadata = NULL;
  ACameraMetadata_const_entry entry;

  g_return_val_if_fail (priv->camera_manager != NULL, FALSE);
  g_return_val_if_fail (priv->camera_id_list != NULL, FALSE);
  g_return_val_if_fail (priv->camera_index < priv->camera_id_list->numCameras,
      FALSE);
  g_return_val_if_fail (priv->capture_request != NULL, FALSE);

  if (ACameraManager_getCameraCharacteristics (priv->camera_manager,
          priv->camera_id_list->cameraIds[priv->camera_index],
          &metadata) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get metadata");
    return FALSE;
  }

  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &entry) != ACAMERA_OK) {
    goto out;
  }

  if (entry.count == 1) {
    GST_WARNING_OBJECT (self,
        "This device has fixed focal length(%.2f).", entry.data.f[0]);
    goto out;
  }

  ACameraCaptureSession_stopRepeating (priv->camera_capture_session);

  /* FIXME: Do we need to set nearest value? */
  ACaptureRequest_setEntry_float (priv->capture_request,
      ACAMERA_LENS_FOCAL_LENGTH, 1, &zoom);

  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  ret = TRUE;
out:
  g_clear_pointer (&metadata, (GDestroyNotify) ACameraMetadata_free);

  return ret;
}

static GstCaps *
gst_ahc2_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstAHC2Src *self = GST_AHC2_SRC (src);
  GstStructure *s = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (self, "Fixating : %" GST_PTR_FORMAT, caps);

  caps = gst_caps_make_writable (caps);

  /* Width/height will be fixed already here, format will
   * be left for fixation by the default handler.
   * We only have to fixate framerate here, to the
   * highest possible framerate.
   */
  gst_structure_fixate_field_nearest_fraction (s, "framerate", G_MAXINT, 1);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (src, caps);

  return caps;
}

static gboolean
gst_ahc2_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstAHC2Src *self = GST_AHC2_SRC (src);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  GstStructure *s;
  GstVideoFormat format;
  gint fmt = -1;
  const gchar *format_str = NULL;
  const GValue *framerate_value;
  gint width, height, fps_min = 0, fps_max = 0;

  ANativeWindow *image_reader_window = NULL;

  g_return_val_if_fail (priv->camera_index < priv->camera_id_list->numCameras,
      FALSE);

  GST_DEBUG_OBJECT (self, "%" GST_PTR_FORMAT, caps);

  s = gst_caps_get_structure (caps, 0);
  format_str = gst_structure_get_string (s, "format");
  format = gst_video_format_from_string (format_str);

  gst_structure_get_int (s, "width", &width);
  gst_structure_get_int (s, "height", &height);
  framerate_value = gst_structure_get_value (s, "framerate");

  if (GST_VALUE_HOLDS_FRACTION_RANGE (framerate_value)) {
    const GValue *min, *max;

    min = gst_value_get_fraction_range_min (framerate_value);
    max = gst_value_get_fraction_range_max (framerate_value);

    fps_min = gst_value_get_fraction_numerator (min) / gst_value_get_fraction_denominator (min);
    fps_max = gst_value_get_fraction_numerator (max) / gst_value_get_fraction_denominator (max);

  } else if (GST_VALUE_HOLDS_FRACTION (framerate_value)) {
    fps_min = fps_max =
        gst_value_get_fraction_numerator (framerate_value) / gst_value_get_fraction_denominator (framerate_value);
  } else {
    GST_WARNING_OBJECT (self, "framerate holds unrecognizable value");
  }

  if (fps_min != 0 && fps_max != 0) {
    gint fps_range[2] = { fps_min, fps_max };
    ACaptureRequest_setEntry_i32 (priv->capture_request,
      ACAMERA_CONTROL_AE_TARGET_FPS_RANGE, 2, fps_range);
    GST_DEBUG_OBJECT (self, "setting fps range [%d, %d]", fps_min, fps_max);
  }

  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
      fmt = AIMAGE_FORMAT_YUV_420_888;
      break;
    default:
      GST_WARNING_OBJECT (self, "unsupported video format (%s)", format_str);
      goto failed;
  }

  g_clear_pointer (&priv->image_reader, (GDestroyNotify) AImageReader_delete);

  if (AImageReader_new (width, height, fmt, priv->max_images,
          &priv->image_reader) != AMEDIA_OK) {
    GST_ERROR_OBJECT (self, "One or more of parameter "
        "(width: %d, height: %d, format: %s, max-images: %d) "
        "is not supported.", width, height, format_str, priv->max_images);

    goto failed;
  }

  if (AImageReader_getWindow (priv->image_reader,
          &image_reader_window) != AMEDIA_OK) {
    GST_WARNING_OBJECT (self, "Failed to get AImageReader surface");
    goto failed;
  }

  AImageReader_setImageListener (priv->image_reader,
      &priv->image_reader_listener);

  if (priv->camera_output_target != NULL) {
    ACaptureRequest_removeTarget (priv->capture_request,
        priv->camera_output_target);
  }
  g_clear_pointer (&priv->camera_output_target,
      (GDestroyNotify) ACameraOutputTarget_free);
  if (ACameraOutputTarget_create (image_reader_window,
          &priv->camera_output_target) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to create ACameraOuputTarget");
    goto failed;
  }

  if (priv->capture_sout != NULL) {
    ACaptureSessionOutputContainer_remove (priv->capture_sout_container,
        priv->capture_sout);
  }
  g_clear_pointer (&priv->capture_sout,
      (GDestroyNotify) ACaptureSessionOutput_free);
  if (ACaptureSessionOutput_create (image_reader_window,
          &priv->capture_sout) != ACAMERA_OK) {
    GST_WARNING_OBJECT (self, "Failed to create ACaptureSessionOutput");
    goto failed;
  }

  ACaptureRequest_addTarget (priv->capture_request, priv->camera_output_target);
  ACaptureSessionOutputContainer_add (priv->capture_sout_container,
      priv->capture_sout);

  ACameraDevice_createCaptureSession (priv->camera_device,
      priv->capture_sout_container, &priv->session_state_cb,
      &priv->camera_capture_session);

  GST_DEBUG_OBJECT (self, "Starting capture request");
  ACameraCaptureSession_setRepeatingRequest (priv->camera_capture_session,
      NULL, 1, &priv->capture_request, NULL);

  return TRUE;

failed:
  g_clear_pointer (&priv->image_reader, (GDestroyNotify) AImageReader_delete);

  g_clear_pointer (&priv->camera_output_target,
      (GDestroyNotify) ACameraOutputTarget_free);

  return FALSE;
}

static GstCaps *
gst_ahc2_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstAHC2Src *self = GST_AHC2_SRC (src);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  GstCaps *caps = gst_caps_new_empty ();
  GstStructure *format = NULL;

  ACameraMetadata *metadata = NULL;
  ACameraMetadata_const_entry entry, fps_entry;

  if (ACameraManager_getCameraCharacteristics (priv->camera_manager,
          priv->camera_id_list->cameraIds[priv->camera_index],
          &metadata) != ACAMERA_OK) {

    GST_ERROR_OBJECT (self, "Failed to get metadata from camera device");

    return caps;
  }

  GST_OBJECT_LOCK (self);

#ifndef GST_DISABLE_GST_DEBUG
  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL, &entry) == ACAMERA_OK) {
    guint8 level = entry.data.u8[0];
    gchar *level_str = NULL;
    switch (level) {
      case ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED:
        level_str = g_strdup ("limited");
        break;
      case ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_FULL:
        level_str = g_strdup ("full");
        break;
      case ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY:
        level_str = g_strdup ("legacy");
        break;
      case ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_3:
        level_str = g_strdup ("3");
        break;
      default:
        level_str = g_strdup ("unknown");
        GST_WARNING_OBJECT (self, "Unknown supported hardware level (%d)",
            level);
        break;
    }

    GST_DEBUG_OBJECT (self, "Hardware supported level: %s", level_str);
    g_free (level_str);
  }

  GST_DEBUG_OBJECT (self, "List of capabilities:");
  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_REQUEST_AVAILABLE_CAPABILITIES, &entry) == ACAMERA_OK) {
    int i = 0;
    for (; i < entry.count; i++) {
      guint8 cap = entry.data.u8[i];
      gchar *cap_str = NULL;
      switch (cap) {
        case ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE:
          cap_str = g_strdup ("  backward compatible");
          break;
        case ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR:
          cap_str = g_strdup ("  manual sensor");
          break;
        case ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING:
          cap_str = g_strdup ("  manual post processing");
          break;
        case ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_RAW:
          cap_str = g_strdup ("  raw");
          break;
        case ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS:
          cap_str = g_strdup ("  read sensor settings");
          break;
        case ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE:
          cap_str = g_strdup ("  burst capture");
          break;
        case ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT:
          cap_str = g_strdup ("  depth output");
          break;
        default:
          cap_str = g_strdup ("  unknown");
          break;
      }
      GST_DEBUG_OBJECT (self, "  %s(%u)", cap_str, cap);
      g_free (cap_str);
    }
  }
#endif

  GST_DEBUG_OBJECT (self, "Checking available FPS ranges:");
  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, &fps_entry) == ACAMERA_OK) {
    int i = 0;
    for (; i < fps_entry.count; i += 2) {
      GST_DEBUG_OBJECT (self, "  (min: %d, max: %d)", fps_entry.data.i32[i], fps_entry.data.i32[i + 1]);
    }
  }

  /* Only NV12 is acceptable. */
  format = gst_structure_new ("video/x-raw",
      "format", G_TYPE_STRING, "NV12", NULL);

  GST_DEBUG_OBJECT (self, "Supported available stream configurations:");
  if (ACameraMetadata_getConstEntry (metadata,
          ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
          &entry) == ACAMERA_OK) {
    gint i = 0;
    for (; i < entry.count; i += 4) {
      gint input = entry.data.i32[i + 3];
      gint fmt = entry.data.i32[i + 0];

      if (input)
        continue;

      if (fmt == AIMAGE_FORMAT_YUV_420_888) {
        gint fps_idx = 0;
        gint width = entry.data.i32[i + 1];
        gint height = entry.data.i32[i + 2];
        GstStructure *size = gst_structure_copy (format);

        GST_DEBUG_OBJECT (self, "  (w: %d, h: %d)", width, height);

        gst_structure_set (size,
            "width", G_TYPE_INT, width,
            "height", G_TYPE_INT, height,
            "interlaced", G_TYPE_BOOLEAN, FALSE,
            "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);

        for (fps_idx = 0; fps_idx < fps_entry.count; fps_idx += 2) {
          GstStructure *s = gst_structure_copy (size);

          if (fps_entry.data.i32[fps_idx] == fps_entry.data.i32[fps_idx + 1]) {
            gst_structure_set (s, "framerate", GST_TYPE_FRACTION,
                fps_entry.data.i32[fps_idx], 1, NULL);
          } else {
            gst_structure_set (s, "framerate", GST_TYPE_FRACTION_RANGE,
                fps_entry.data.i32[fps_idx], 1, fps_entry.data.i32[fps_idx + 1], 1, NULL);
          }
          gst_caps_append_structure (caps, s);
        }
        gst_structure_free (size);
      }
    }
  } else {
    GST_WARNING_OBJECT (self, "  No available stream configurations detected");
  }

  g_clear_pointer (&format, gst_structure_free);
  g_clear_pointer (&metadata, (GDestroyNotify) ACameraMetadata_free);

  GST_DEBUG_OBJECT (self, "%" GST_PTR_FORMAT, caps);

  GST_OBJECT_UNLOCK (self);

  return caps;
}

static gboolean
gst_ahc2_src_unlock (GstBaseSrc * src)
{
  GstAHC2Src *self = GST_AHC2_SRC (src);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  GST_DEBUG_OBJECT (self, "unlocking create");
  gst_data_queue_set_flushing (priv->outbound_queue, TRUE);

  return TRUE;
}

static gboolean
gst_ahc2_src_unlock_stop (GstBaseSrc * src)
{
  GstAHC2Src *self = GST_AHC2_SRC (src);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  GST_DEBUG_OBJECT (self, "stopping unlock");
  gst_data_queue_set_flushing (priv->outbound_queue, FALSE);

  return TRUE;
}

static GstFlowReturn
gst_ahc2_src_create (GstPushSrc * src, GstBuffer ** buffer)
{
  GstAHC2Src *self = GST_AHC2_SRC (src);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  GstDataQueueItem *item;

  if (!gst_data_queue_pop (priv->outbound_queue, &item)) {
    GST_DEBUG_OBJECT (self, "We're flushing");
    return GST_FLOW_FLUSHING;
  }

  *buffer = GST_BUFFER (item->object);
  g_free (item);

  return GST_FLOW_OK;
}

static void
gst_ahc2_src_camera_close (GstAHC2Src * self)
{
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  GST_DEBUG_OBJECT (self, "Closing Camera");

  g_clear_pointer (&priv->capture_sout,
      (GDestroyNotify) ACaptureSessionOutput_free);

  g_clear_pointer (&priv->camera_output_target,
      (GDestroyNotify) ACameraOutputTarget_free);

  g_clear_pointer (&priv->capture_request,
      (GDestroyNotify) ACaptureRequest_free);

  g_clear_pointer (&priv->capture_sout_container,
      (GDestroyNotify) ACaptureSessionOutputContainer_free);

  g_clear_pointer (&priv->camera_device, (GDestroyNotify) ACameraDevice_close);
}

static gboolean
gst_ahc2_src_camera_open (GstAHC2Src * self)
{
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  gboolean ret = FALSE;
  const gchar *camera_id;

  g_return_val_if_fail (priv->camera_id_list != NULL, FALSE);

  g_return_val_if_fail (priv->camera_index < priv->camera_id_list->numCameras,
      FALSE);

  camera_id = priv->camera_id_list->cameraIds[priv->camera_index];

  GST_DEBUG_OBJECT (self, "Trying to open camera device (id: %s)", camera_id);

  if (ACameraManager_openCamera (priv->camera_manager, camera_id,
          &priv->device_state_cb, &priv->camera_device) != ACAMERA_OK) {

    GST_ERROR_OBJECT (self, "Failed to open camera device (id: %s)", camera_id);
    goto out;
  }

  if (ACameraDevice_createCaptureRequest (priv->camera_device,
          priv->camera_template_type, &priv->capture_request) != ACAMERA_OK) {
    GST_ERROR_OBJECT (self, "Failed to create camera request (camera id: %s)",
        camera_id);
    goto out;
  }

  if (ACaptureSessionOutputContainer_create (&priv->capture_sout_container) !=
      ACAMERA_OK) {
    GST_ERROR_OBJECT (self,
        "Failed to create capture session output container (camera id: %s)",
        camera_id);
    goto out;
  }

  ret = TRUE;

out:

  if (!ret)
    gst_ahc2_src_camera_close (self);

  return ret;
}

static gboolean
gst_ahc2_src_start (GstBaseSrc * src)
{
  GstAHC2Src *self = GST_AHC2_SRC (src);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);
  g_mutex_lock (&priv->mutex);

  if (!gst_ahc2_src_camera_open (self)) {
    goto out;
  }

  priv->previous_ts = GST_CLOCK_TIME_NONE;
  priv->started = TRUE;

out:
  g_mutex_unlock (&priv->mutex);

  return priv->started;
}

static gboolean
gst_ahc2_src_stop (GstBaseSrc * src)
{
  GstAHC2Src *self = GST_AHC2_SRC (src);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  g_mutex_lock (&priv->mutex);
  ACameraCaptureSession_abortCaptures (priv->camera_capture_session);
  gst_ahc2_src_camera_close (self);

  gst_data_queue_flush (priv->outbound_queue);

  priv->started = FALSE;
  g_mutex_unlock (&priv->mutex);

  return TRUE;
}

static void
gst_ahc2_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstAHC2Src *self = GST_AHC2_SRC (object);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  switch ((GstAHC2SrcProperty) prop_id) {
    case PROP_CAMERA_INDEX:
      priv->camera_index = g_value_get_int (value);
      break;
    case PROP_CAMERA_TEMPLATE_TYPE:
      priv->camera_template_type = g_value_get_int (value);
      break;
    case PROP_MAX_IMAGES:
      priv->max_images = g_value_get_int (value);
      break;
    case PROP_COLOR_TONE_MODE:{
      GstPhotographyColorToneMode tone = g_value_get_enum (value);
      gst_ahc2_src_set_color_tone_mode (GST_PHOTOGRAPHY (self), tone);
      break;
    }
    case PROP_EV_COMPENSATION:{
      gfloat ev = g_value_get_float (value);
      gst_ahc2_src_set_ev_compensation (GST_PHOTOGRAPHY (self), ev);
      break;
    }
    case PROP_SCENE_MODE:{
      GstPhotographySceneMode scene = g_value_get_enum (value);
      gst_ahc2_src_set_scene_mode (GST_PHOTOGRAPHY (self), scene);
      break;
    }
    case PROP_WHITE_BALANCE_MODE:{
      GstPhotographyWhiteBalanceMode wb = g_value_get_enum (value);
      gst_ahc2_src_set_white_balance_mode (GST_PHOTOGRAPHY (self), wb);
      break;
    }
    case PROP_FLASH_MODE:{
      GstPhotographyFlashMode flash = g_value_get_enum (value);
      gst_ahc2_src_set_flash_mode (GST_PHOTOGRAPHY (self), flash);
      break;
    }
    case PROP_FLICKER_MODE:{
      GstPhotographyFlickerReductionMode flicker = g_value_get_enum (value);
      gst_ahc2_src_set_flicker_mode (GST_PHOTOGRAPHY (self), flicker);
      break;
    }
    case PROP_FOCUS_MODE:{
      GstPhotographyFocusMode focus = g_value_get_enum (value);
      gst_ahc2_src_set_focus_mode (GST_PHOTOGRAPHY (self), focus);
      break;
    }
    case PROP_ZOOM:{
      gfloat zoom = g_value_get_float (value);
      gst_ahc2_src_set_zoom (GST_PHOTOGRAPHY (self), zoom);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ahc2_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstAHC2Src *self = GST_AHC2_SRC (object);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  switch ((GstAHC2SrcProperty) prop_id) {
    case PROP_CAMERA_INDEX:
      g_value_set_int (value, priv->camera_index);
      break;
    case PROP_N_CAMERAS:
      if (priv->camera_id_list != NULL) {
        g_value_set_int (value, priv->camera_id_list->numCameras);
      } else {
        GST_WARNING_OBJECT (self, "Camera list doesn't exist.");
        g_value_set_int (value, 0);
      }
      break;
    case PROP_CAMERA_TEMPLATE_TYPE:
      g_value_set_int (value, priv->camera_template_type);
      break;
    case PROP_MAX_IMAGES:
      g_value_set_int (value, priv->max_images);
      break;
    case PROP_CAPABILITIES:{
      GstPhotographyCaps caps;

      caps = gst_ahc2_src_get_capabilities (GST_PHOTOGRAPHY (self));
      g_value_set_ulong (value, caps);

      break;
    }
    case PROP_COLOR_TONE_MODE:{
      GstPhotographyColorToneMode tone;
      if (gst_ahc2_src_get_color_tone_mode (GST_PHOTOGRAPHY (self), &tone))
        g_value_set_enum (value, tone);
      break;
    }
    case PROP_EV_COMPENSATION:{
      gfloat ev;

      if (gst_ahc2_src_get_ev_compensation (GST_PHOTOGRAPHY (self), &ev))
        g_value_set_float (value, ev);

      break;
    }
    case PROP_WHITE_BALANCE_MODE:{
      GstPhotographyWhiteBalanceMode wb;
      if (gst_ahc2_src_get_white_balance_mode (GST_PHOTOGRAPHY (self), &wb))
        g_value_set_enum (value, wb);
      break;
    }
    case PROP_FLASH_MODE:{
      GstPhotographyFlashMode flash;
      if (gst_ahc2_src_get_flash_mode (GST_PHOTOGRAPHY (self), &flash))
        g_value_set_enum (value, flash);
      break;
    }
    case PROP_FLICKER_MODE:{
      GstPhotographyFlickerReductionMode flicker;
      if (gst_ahc2_src_get_flicker_mode (GST_PHOTOGRAPHY (self), &flicker))
        g_value_set_enum (value, flicker);
      break;
    }
    case PROP_FOCUS_MODE:{
      GstPhotographyFocusMode focus;
      if (gst_ahc2_src_get_focus_mode (GST_PHOTOGRAPHY (self), &focus))
        g_value_set_enum (value, focus);
      break;
    }
    case PROP_SCENE_MODE:{
      GstPhotographySceneMode scene;
      if (gst_ahc2_src_get_scene_mode (GST_PHOTOGRAPHY (self), &scene))
        g_value_set_enum (value, scene);
      break;
    }
    case PROP_ZOOM:{
      gfloat zoom;
      if (gst_ahc2_src_get_zoom (GST_PHOTOGRAPHY (self), &zoom))
        g_value_set_float (value, zoom);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ahc2_src_finalize (GObject * object)
{
  GstAHC2Src *self = GST_AHC2_SRC (object);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  g_clear_pointer (&priv->camera_id_list,
      (GDestroyNotify) ACameraManager_deleteCameraIdList);

  g_clear_pointer (&priv->camera_manager,
      (GDestroyNotify) ACameraManager_delete);

  g_clear_pointer (&priv->outbound_queue, gst_object_unref);

  g_mutex_clear (&priv->mutex);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
data_queue_item_free (GstDataQueueItem * item)
{
  g_clear_pointer (&item->object, (GDestroyNotify) gst_mini_object_unref);
  g_free (item);
}

static void
image_reader_on_image_available (void *context, AImageReader * reader)
{
  GstAHC2Src *self = GST_AHC2_SRC (context);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  GstBuffer *buffer;
  GstDataQueueItem *item;
  GstClockTime duration = GST_CLOCK_TIME_NONE;
  GstClockTime current_ts = GST_CLOCK_TIME_NONE;
  GstClock *clock;

  GstWrappedAImage *wrapped_aimage;
  AImage *image = NULL;
  gint n_planes;
  gint i;

  if ((clock = GST_ELEMENT_CLOCK (self))) {
    GstClockTime base_time = GST_ELEMENT_CAST (self)->base_time;
    gst_object_ref (clock);
    current_ts = gst_clock_get_time (clock) - base_time;
    gst_object_unref (clock);
  }

  if (AImageReader_acquireLatestImage (reader, &image) != AMEDIA_OK) {
    GST_DEBUG_OBJECT (self, "No image available");
    return;
  }
  g_mutex_lock (&priv->mutex);

  GST_TRACE_OBJECT (self, "Acquired an image (ts: %" GST_TIME_FORMAT ")",
      GST_TIME_ARGS (current_ts));

#ifndef GST_DISABLE_GST_DEBUG
  {
    gint64 image_ts;
    /* AImageReader has its own clock. */
    AImage_getTimestamp (image, &image_ts);
    GST_TRACE_OBJECT (self, "  AImage internal ts: %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (image_ts));
  }
#endif

  if (!priv->started || GST_CLOCK_TIME_IS_VALID (priv->previous_ts)) {
    duration = current_ts - priv->previous_ts;
    priv->previous_ts = current_ts;
  } else {
    priv->previous_ts = current_ts;
    /* Dropping first image to calculate duration */
    AImage_delete (image);
    GST_DEBUG_OBJECT (self, "Droping image (reason: %s)",
        priv->started ? "first frame" : "not yet started");
    g_mutex_unlock (&priv->mutex);
    return;
  }

  AImage_getNumberOfPlanes (image, &n_planes);

  buffer = gst_buffer_new ();
  GST_BUFFER_DURATION (buffer) = duration;
  GST_BUFFER_PTS (buffer) = current_ts;

  wrapped_aimage = g_new0 (GstWrappedAImage, 1);
  wrapped_aimage->refcount = 1;
  wrapped_aimage->ahc2src = g_object_ref (self);
  wrapped_aimage->image = image;

  for (i = 0; i < n_planes; i++) {
    guint8 *data;
    gint length;
    GstMemory *mem;

    AImage_getPlaneData (image, i, &data, &length);

    mem = gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
        data, length, 0, length,
        gst_wrapped_aimage_ref (wrapped_aimage),
        (GDestroyNotify) gst_wrapped_aimage_unref);

    GST_TRACE_OBJECT (self, "Created a wrapped memory (ptr: %p, length: %d)",
        mem, length);
    gst_buffer_append_memory (buffer, mem);
  }

  item = g_new0 (GstDataQueueItem, 1);
  item->object = GST_MINI_OBJECT (buffer);
  item->size = gst_buffer_get_size (buffer);
  item->visible = TRUE;
  item->destroy = (GDestroyNotify) data_queue_item_free;

  if (!gst_data_queue_push (priv->outbound_queue, item)) {
    item->destroy (item);
    GST_DEBUG_OBJECT (self, "Failed to push item because we're flushing");
  }

  gst_wrapped_aimage_unref (wrapped_aimage);
  g_mutex_unlock (&priv->mutex);

  GST_TRACE_OBJECT (self,
      "created buffer from image callback %" G_GSIZE_FORMAT ", ts %"
      GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT
      ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
      gst_buffer_get_size (buffer),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)),
      GST_BUFFER_OFFSET (buffer), GST_BUFFER_OFFSET_END (buffer));
}

static void
camera_device_on_disconnected (void *context, ACameraDevice * device)
{
  GstAHC2Src *self = GST_AHC2_SRC (context);

  GST_DEBUG_OBJECT (self, "Camera[%s] is disconnected",
      ACameraDevice_getId (device));
}

static void
camera_device_on_error (void *context, ACameraDevice * device, int error)
{
  GstAHC2Src *self = GST_AHC2_SRC (context);

  GST_ERROR_OBJECT (self, "Error (code: %d) on Camera[%s]", error,
      ACameraDevice_getId (device));
}

static void
capture_session_on_ready (void *context, ACameraCaptureSession * session)
{
  GstAHC2Src *self = GST_AHC2_SRC (context);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  GST_DEBUG_OBJECT (self, "Camera[%s] capture session is ready",
      ACameraDevice_getId (priv->camera_device));
}

static void
capture_session_on_active (void *context, ACameraCaptureSession * session)
{
  GstAHC2Src *self = GST_AHC2_SRC (context);
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  GST_DEBUG_OBJECT (self, "Camera[%s] capture session is activated",
      ACameraDevice_getId (priv->camera_device));
}

static const gchar *
gst_ahc2_src_get_camera_id_by_index (GstAHC2Src * self, gint idx)
{
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  g_return_val_if_fail (priv->camera_id_list != NULL, NULL);
  g_return_val_if_fail (idx >= priv->camera_id_list->numCameras, NULL);

  return priv->camera_id_list->cameraIds[idx];
}

static void
gst_ahc2_src_class_init (GstAHC2SrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_ahc2_src_set_property;
  gobject_class->get_property = gst_ahc2_src_get_property;
  gobject_class->finalize = gst_ahc2_src_finalize;

  /**
   * GstAHC2Src:camera-index:
   *
   * The Camera index to be connected.
   */
  properties[PROP_CAMERA_INDEX] =
      g_param_spec_int ("camera-index", "Camera Index",
      "The camera device index to open", 0, G_MAXINT, 0,
      G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY | G_PARAM_STATIC_STRINGS);

  /**
   * GstAHC2Src:n-cameras:
   *
   * The number of connected camera devices.
   */
  properties[PROP_N_CAMERAS] =
      g_param_spec_int ("n-cameras", "Number of Cameras",
      "The number of connected camera devices", 0, G_MAXINT, 0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GstAHC2Src:camera-template-type:
   * The suitable template type for the purpose of running camera.
   */
  properties[PROP_CAMERA_TEMPLATE_TYPE] =
      g_param_spec_int ("camera-template-type", "Camera Template Type",
      "A request template for the camera running purpose", -1, G_MAXINT,
      TEMPLATE_PREVIEW,
      G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY | G_PARAM_STATIC_STRINGS);

  /**
   * GstAHC2Src:max-images:
   * The number of buffering images.
   */
  properties[PROP_MAX_IMAGES] =
      g_param_spec_int ("max-images", "Max Images",
      "The number of buffering images", 0, G_MAXINT,
      DEFAULT_MAX_IMAGES,
      G_PARAM_READWRITE | GST_PARAM_MUTABLE_PAUSED | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_N_INSTALL, properties);

  /* Override GstPhotography properties */
  g_object_class_override_property (gobject_class, PROP_ANALOG_GAIN,
      GST_PHOTOGRAPHY_PROP_ANALOG_GAIN);
  properties[PROP_ANALOG_GAIN] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_ANALOG_GAIN);

  g_object_class_override_property (gobject_class, PROP_APERTURE,
      GST_PHOTOGRAPHY_PROP_APERTURE);
  properties[PROP_APERTURE] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_APERTURE);

  g_object_class_override_property (gobject_class, PROP_CAPABILITIES,
      GST_PHOTOGRAPHY_PROP_CAPABILITIES);
  properties[PROP_CAPABILITIES] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_CAPABILITIES);

  g_object_class_override_property (gobject_class, PROP_COLOR_TEMPERATURE,
      GST_PHOTOGRAPHY_PROP_COLOR_TEMPERATURE);
  properties[PROP_COLOR_TEMPERATURE] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_COLOR_TEMPERATURE);

  g_object_class_override_property (gobject_class, PROP_COLOR_TONE_MODE,
      GST_PHOTOGRAPHY_PROP_COLOR_TONE);
  properties[PROP_COLOR_TONE_MODE] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_COLOR_TONE);

  g_object_class_override_property (gobject_class, PROP_EV_COMPENSATION,
      GST_PHOTOGRAPHY_PROP_EV_COMP);
  properties[PROP_EV_COMPENSATION] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_EV_COMP);

  g_object_class_override_property (gobject_class, PROP_EXPOSURE_TIME,
      GST_PHOTOGRAPHY_PROP_EXPOSURE_TIME);
  properties[PROP_EXPOSURE_TIME] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_EXPOSURE_TIME);

  g_object_class_override_property (gobject_class, PROP_FLASH_MODE,
      GST_PHOTOGRAPHY_PROP_FLASH_MODE);
  properties[PROP_FLASH_MODE] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_FLASH_MODE);

  g_object_class_override_property (gobject_class, PROP_FLICKER_MODE,
      GST_PHOTOGRAPHY_PROP_FLICKER_MODE);
  properties[PROP_FLICKER_MODE] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_FLICKER_MODE);

  g_object_class_override_property (gobject_class, PROP_FOCUS_MODE,
      GST_PHOTOGRAPHY_PROP_FOCUS_MODE);
  properties[PROP_FOCUS_MODE] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_FOCUS_MODE);

  g_object_class_override_property (gobject_class,
      PROP_IMAGE_CAPTURE_SUPPORTED_CAPS,
      GST_PHOTOGRAPHY_PROP_IMAGE_CAPTURE_SUPPORTED_CAPS);
  properties[PROP_IMAGE_CAPTURE_SUPPORTED_CAPS] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_IMAGE_CAPTURE_SUPPORTED_CAPS);

  g_object_class_override_property (gobject_class,
      PROP_IMAGE_PREVIEW_SUPPORTED_CAPS,
      GST_PHOTOGRAPHY_PROP_IMAGE_PREVIEW_SUPPORTED_CAPS);
  properties[PROP_IMAGE_PREVIEW_SUPPORTED_CAPS] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_IMAGE_PREVIEW_SUPPORTED_CAPS);

  g_object_class_override_property (gobject_class, PROP_ISO_SPEED,
      GST_PHOTOGRAPHY_PROP_ISO_SPEED);
  properties[PROP_ISO_SPEED] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_ISO_SPEED);

  g_object_class_override_property (gobject_class, PROP_LENS_FOCUS,
      GST_PHOTOGRAPHY_PROP_LENS_FOCUS);
  properties[PROP_LENS_FOCUS] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_LENS_FOCUS);

  g_object_class_override_property (gobject_class, PROP_MAX_EXPOSURE_TIME,
      GST_PHOTOGRAPHY_PROP_MAX_EXPOSURE_TIME);
  properties[PROP_MAX_EXPOSURE_TIME] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_MAX_EXPOSURE_TIME);

  g_object_class_override_property (gobject_class, PROP_MIN_EXPOSURE_TIME,
      GST_PHOTOGRAPHY_PROP_MIN_EXPOSURE_TIME);
  properties[PROP_MIN_EXPOSURE_TIME] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_MIN_EXPOSURE_TIME);

  g_object_class_override_property (gobject_class, PROP_NOISE_REDUCTION,
      GST_PHOTOGRAPHY_PROP_NOISE_REDUCTION);
  properties[PROP_NOISE_REDUCTION] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_NOISE_REDUCTION);

  g_object_class_override_property (gobject_class, PROP_SCENE_MODE,
      GST_PHOTOGRAPHY_PROP_SCENE_MODE);
  properties[PROP_SCENE_MODE] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_SCENE_MODE);

  g_object_class_override_property (gobject_class, PROP_WHITE_BALANCE_MODE,
      GST_PHOTOGRAPHY_PROP_WB_MODE);
  properties[PROP_WHITE_BALANCE_MODE] =
      g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_WB_MODE);

  g_object_class_override_property (gobject_class, PROP_WHITE_POINT,
      GST_PHOTOGRAPHY_PROP_WHITE_POINT);
  properties[PROP_WHITE_POINT] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_WHITE_POINT);

  g_object_class_override_property (gobject_class, PROP_ZOOM,
      GST_PHOTOGRAPHY_PROP_ZOOM);
  properties[PROP_ZOOM] = g_object_class_find_property (gobject_class,
      GST_PHOTOGRAPHY_PROP_ZOOM);

  signals[SIG_GET_CAMERA_ID_BY_INDEX] =
      g_signal_new ("get-camera-id-by-index", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (GstAHC2SrcClass,
          get_camera_id_by_index), NULL, NULL, NULL,
      G_TYPE_CHAR, 1, G_TYPE_INT);

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_set_static_metadata (gstelement_class,
      "Android Camera2 Source", "Source/Video/Device",
      "Source element for Android hardware camers 2 API",
      "Justin Kim <justin.kim@collabora.com>");

  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_ahc2_src_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_ahc2_src_set_caps);
  gstbasesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_ahc2_src_fixate);
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_ahc2_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_ahc2_src_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_ahc2_src_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_ahc2_src_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_ahc2_src_create);

  klass->get_camera_id_by_index = gst_ahc2_src_get_camera_id_by_index;
}

static gboolean
data_queue_check_full_cb (GstDataQueue * queue, guint visible,
    guint bytes, guint64 time, gpointer checkdata)
{
  return FALSE;
}

static void
gst_ahc2_src_init (GstAHC2Src * self)
{
  GstAHC2SrcPrivate *priv = GST_AHC2_SRC_GET_PRIVATE (self);

  priv->camera_template_type = TEMPLATE_PREVIEW;
  priv->max_images = DEFAULT_MAX_IMAGES;
  priv->outbound_queue =
      gst_data_queue_new (data_queue_check_full_cb, NULL, NULL, NULL);

  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);

  priv->camera_manager = ACameraManager_create ();

  if (ACameraManager_getCameraIdList (priv->camera_manager,
          &priv->camera_id_list) != ACAMERA_OK) {
    GST_ERROR_OBJECT (self, "Failed to get camera id list");
    goto fail;
  }

  if (priv->camera_id_list->numCameras < 1) {
    GST_WARNING_OBJECT (self, "No camera device detected.");
    goto fail;
  }

  priv->device_state_cb.context = self;
  priv->device_state_cb.onDisconnected = camera_device_on_disconnected;
  priv->device_state_cb.onError = camera_device_on_error;

  priv->session_state_cb.context = self;
  priv->session_state_cb.onReady = capture_session_on_ready;
  priv->session_state_cb.onActive = capture_session_on_active;

  priv->image_reader_listener.context = self;
  priv->image_reader_listener.onImageAvailable =
      image_reader_on_image_available;

  g_mutex_init (&priv->mutex);

#ifndef GST_DISABLE_GST_DEBUG
  {
    int idx = 0;

    GST_DEBUG_OBJECT (self, "detected %d cameras",
        priv->camera_id_list->numCameras);
    for (; idx < priv->camera_id_list->numCameras; idx++) {
      GST_DEBUG_OBJECT (self, "  camera device[%d]: %s", idx,
          priv->camera_id_list->cameraIds[idx]);
    }
  }
#endif

  return;

fail:
  g_clear_pointer (&priv->camera_id_list,
      (GDestroyNotify) ACameraManager_deleteCameraIdList);

  g_clear_pointer (&priv->camera_manager,
      (GDestroyNotify) ACameraManager_delete);
}

static void
gst_ahc2_src_photography_init (gpointer g_iface, gpointer iface_data)
{
  GstPhotographyInterface *iface = g_iface;

  iface->get_capabilities = gst_ahc2_src_get_capabilities;

  iface->get_color_tone_mode = gst_ahc2_src_get_color_tone_mode;
  iface->set_color_tone_mode = gst_ahc2_src_set_color_tone_mode;

  iface->set_autofocus = gst_ahc2_src_set_autofocus;
  iface->get_focus_mode = gst_ahc2_src_get_focus_mode;
  iface->set_focus_mode = gst_ahc2_src_set_focus_mode;

  iface->get_flash_mode = gst_ahc2_src_get_flash_mode;
  iface->set_flash_mode = gst_ahc2_src_set_flash_mode;

  iface->get_flicker_mode = gst_ahc2_src_get_flicker_mode;
  iface->set_flicker_mode = gst_ahc2_src_set_flicker_mode;

  iface->get_scene_mode = gst_ahc2_src_get_scene_mode;
  iface->set_scene_mode = gst_ahc2_src_set_scene_mode;

  iface->get_white_balance_mode = gst_ahc2_src_get_white_balance_mode;
  iface->set_white_balance_mode = gst_ahc2_src_set_white_balance_mode;

  iface->get_ev_compensation = gst_ahc2_src_get_ev_compensation;
  iface->set_ev_compensation = gst_ahc2_src_set_ev_compensation;

  iface->get_zoom = gst_ahc2_src_get_zoom;
  iface->set_zoom = gst_ahc2_src_set_zoom;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
   if (!gst_element_register (plugin, "ahc2src", GST_RANK_NONE, GST_TYPE_AHC2_SRC)) {
      GST_ERROR("Failed to register ahc2src plugin ahc2src");
    return FALSE;
  }

  return TRUE;
}

#ifndef PACKAGE
#define PACKAGE "ahc2src"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ahc2src,
    "ahc2src source",
    plugin_init, "1.15.2", "LGPL", "ahc2src", "https://developer.gnome.org/gstreamer/")
