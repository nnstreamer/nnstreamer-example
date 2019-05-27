#------------------------------------------------------
# ahc (ahc2src, GStreamer android.hardware.Camera2 Source)
#
# ahc2src itself just passes image pointers to source pad and manage the reference counts.
# See below links for more details.
# https://justinjoy9to5.blogspot.com/2017/10/gstreamer-camera-2-source-for-android.html
# https://gitlab.collabora.com/joykim/gst-plugins-bad/blob/wip/ahc2/sys/androidmedia/gstahc2src.c
#------------------------------------------------------
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ahc

LOCAL_SRC_FILES := $(LOCAL_PATH)/ahc/gstahc2src.c

LOCAL_C_INCLUDES := \
    $(ANDROID_NDK)/sysroot/usr/include \
    $(GSTREAMER_ROOT)/include/gstreamer-1.0 \
    $(GSTREAMER_ROOT)/include/glib-2.0 \
    $(GSTREAMER_ROOT)/lib/glib-2.0/include \
    $(GSTREAMER_ROOT)/include

# ahc2src uses the interface gst-photography.
# To avoid the warning, define GST_USE_UNSTABLE_API.
# See photography.h in downloaded gstreamer binaries directoy for more details.
LOCAL_CFLAGS += -O2 -DGST_USE_UNSTABLE_API

include $(BUILD_STATIC_LIBRARY)
