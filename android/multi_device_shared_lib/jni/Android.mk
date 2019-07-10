LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# location of nnstreamer root directory
ifndef NNSTREAMER_ROOT
$(error NNSTREAMER_ROOT is not defined!)
endif

# ndk path
ifndef ANDROID_NDK
$(error ANDROID_NDK is not defined!)
endif

#include nnstreamer.mk
include $(NNSTREAMER_ROOT)/jni/nnstreamer.mk

#location of gstreamer shared libraries : Cerbero
ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86_64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

# import shared libs in nnstreamer root directory
define shared_lib
	include $(CLEAR_VARS)
	LOCAL_MODULE := $(1)
	LOCAL_SRC_FILES := $(NNSTREAMER_ROOT)/libs/$(TARGET_ARCH_ABI)/lib$(1).so
	include $(PREBUILT_SHARED_LIBRARY)
endef

$(foreach item,$(NNSTREAMER_BUILDING_BLOCK_LIST),$(eval $(call shared_lib,$(item))))

include $(CLEAR_VARS)

LOCAL_MODULE    := nnstreamer_multidevice
LOCAL_SRC_FILES := nnstreamer-ssd.cpp NNStreamerMultiDevice.c
LOCAL_CFLAGS += -O2 -DGST_USE_UNSTABLE_API -fPIC
LOCAL_SHARED_LIBRARIES := $(GST_BUILDING_BLOCK_LIST) gstreamer_android ahc2src

LOCAL_LDLIBS := -llog -landroid -lz
LOCAL_C_INCLUDES := \
     $(NNSTREAMER_INCLUDES) \
     $(ANDROID_NDK)/sysroot/usr/include \
     $(GSTREAMER_ROOT)/include/gstreamer-1.0 \
     $(GSTREAMER_ROOT)/include/glib-2.0 \
     $(GSTREAMER_ROOT)/lib/glib-2.0/include \
     $(GSTREAMER_ROOT)/include


include $(BUILD_SHARED_LIBRARY)

# compile ahc2src for camera input in android device
include $(CLEAR_VARS)

LOCAL_MODULE := ahc2src
LOCAL_SRC_FILES := gstahc2src.c
LOCAL_SHARED_LIBRARIES := $(NNSTREAMER_BUILDING_BLOCK_LIST)
LOCAL_CFLAGS += -O2 -DGST_USE_UNSTABLE_API
LOCAL_LDLIBS := -llog -landroid -lcamera2ndk -lmediandk -lz
LOCAL_C_INCLUDES := \
      $(ANDROID_NDK)/sysroot/usr/include \
      $(GSTREAMER_ROOT)/include/gstreamer-1.0 \
      $(GSTREAMER_ROOT)/include/glib-2.0 \
      $(GSTREAMER_ROOT)/lib/glib-2.0/include \
      $(GSTREAMER_ROOT)/include

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := gstreamer_android
LOCAL_SRC_FILES := gstreamer_android.c
LOCAL_SHARED_LIBRARIES := $(NNSTREAMER_BUILDING_BLOCK_LIST)
LOCAL_CFLAGS += -O2 -DGST_USE_UNSTABLE_API
LOCAL_LDLIBS := -llog -landroid
LOCAL_C_INCLUDES := \
     $(ANDROID_NDK)/sysroot/usr/include \
     $(GSTREAMER_ROOT)/include/gstreamer-1.0 \
     $(GSTREAMER_ROOT)/include/glib-2.0 \
     $(GSTREAMER_ROOT)/lib/glib-2.0/include \
     $(GSTREAMER_ROOT)/include

include $(BUILD_SHARED_LIBRARY)
