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

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
AHC_LIB_PATH := $(LOCAL_PATH)/ahc/lib/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
AHC_LIB_PATH := $(LOCAL_PATH)/ahc/lib/arm64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

LOCAL_SRC_FILES := $(AHC_LIB_PATH)/libahc.a

include $(PREBUILT_STATIC_LIBRARY)
