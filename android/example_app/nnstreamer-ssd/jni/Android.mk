LOCAL_PATH := $(call my-dir)

ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

#------------------------------------------------------
# tensorflow-lite
# Android-tensorflow-lite-prebuilt.mk is a script to use prebuilt static library.
# If it needs to utilize tensorflow-lite, modify Android-tensorflow-lite.mk and build your own library.
#------------------------------------------------------
#include $(LOCAL_PATH)/Android-tensorflow-lite.mk
include $(LOCAL_PATH)/Android-tensorflow-lite-prebuilt.mk

#------------------------------------------------------
# ahc (ahc2src, GStreamer android.hardware.Camera2 Source)
# Android-ahc-prebuilt.mk is a script to use prebuilt static library.
# If you want to build the ahc2src element, use Android-ahc.mk instead.
#------------------------------------------------------
#include $(LOCAL_PATH)/Android-ahc.mk
include $(LOCAL_PATH)/Android-ahc-prebuilt.mk

#------------------------------------------------------
# nnstreamer
#------------------------------------------------------
include $(LOCAL_PATH)/Android-nnstreamer.mk

#------------------------------------------------------
# nnstreamer example
#------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE    := nnstreamer-jni
LOCAL_SRC_FILES := nnstreamer-jni.c nnstreamer-ex.cpp
LOCAL_STATIC_LIBRARIES := nnstreamer tensorflow-lite ahc
LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_LDLIBS := -llog -landroid -lcamera2ndk -lmediandk

include $(BUILD_SHARED_LIBRARY)

GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk
# add necessary gstreamer plugins
GSTREAMER_PLUGINS         := $(GSTREAMER_PLUGINS_CORE) $(GSTREAMER_PLUGINS_SYS) cairo
GSTREAMER_EXTRA_DEPS      := gstreamer-video-1.0 gstreamer-audio-1.0 gobject-2.0
# cairo graphics library is used to display detected objects in this example.
# (cairooverlay, https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-good/html/gst-plugins-good-plugins-cairooverlay.html)
# gst-photography is interface for digital imaging, used in ahc2src.
# (https://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-bad-libs/html/gst-plugins-bad-libs-GstPhotography.html)
GSTREAMER_EXTRA_LIBS      := -liconv -lcairo -lgstphotography-1.0
include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk
