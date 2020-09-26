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

LOCAL_SRC_FILES := \
    $(NNSTREAMER_COMMON_SRCS) \
    $(NNSTREAMER_PLUGINS_SRCS) \
    $(NNSTREAMER_FILTER_TFLITE_SRCS)

LOCAL_C_INCLUDES := \
    $(NNSTREAMER_INCLUDES)

# common headers (gstreamer, glib)
LOCAL_C_INCLUDES += \
    $(GST_HEADERS_COMMON)

# common headers (tensorflow-lite)
LOCAL_C_INCLUDES += \
    $(TF_LITE_INCLUDES)

LOCAL_CFLAGS := -O3 -fPIC -DVERSION=\"$(NNSTREAMER_VERSION)\"
LOCAL_CXXFLAGS := -std=c++11 -O3 -fPIC -frtti -fexceptions -DVERSION=\"$(NNSTREAMER_VERSION)\"

include $(BUILD_STATIC_LIBRARY)
