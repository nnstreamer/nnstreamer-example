#------------------------------------------------------
# nnstreamer
#------------------------------------------------------
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := nnstreamer

ifndef NNSTREAMER_ROOT
$(error NNSTREAMER_ROOT is not defined!)
endif

include $(NNSTREAMER_ROOT)/jni/nnstreamer.mk

NNSTREAMER_SOURCE_AMC_SRCS := \
    $(NNSTREAMER_EXT_HOME)/android_source/gstamcsrc.c \
    $(NNSTREAMER_EXT_HOME)/android_source/gstamcsrc_looper.cc

LOCAL_SRC_FILES := \
    $(NNSTREAMER_COMMON_SRCS) \
    $(NNSTREAMER_PLUGINS_SRCS) \
    $(NNSTREAMER_FILTER_TFLITE_SRCS) \
    $(NNSTREAMER_SOURCE_AMC_SRCS)

LOCAL_C_INCLUDES := \
    $(NNSTREAMER_INCLUDES)

# common headers (gstreamer, glib)
LOCAL_C_INCLUDES += \
    $(GSTREAMER_ROOT)/include/gstreamer-1.0 \
    $(GSTREAMER_ROOT)/include/glib-2.0 \
    $(GSTREAMER_ROOT)/lib/glib-2.0/include \
    $(GSTREAMER_ROOT)/include

# common headers (tensorflow-lite)
LOCAL_C_INCLUDES += \
    $(TF_LITE_INCLUDES)

LOCAL_CFLAGS += -O2 -DVERSION=\"$(NNSTREAMER_VERSION)\"
LOCAL_CXXFLAGS += -std=c++11 -O2 -DVERSION=\"$(NNSTREAMER_VERSION)\" -fexceptions

include $(BUILD_STATIC_LIBRARY)
