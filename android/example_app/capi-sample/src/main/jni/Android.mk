LOCAL_PATH := $(call my-dir)

#------------------------------------------------------
# nnstreamer prebuilt
#------------------------------------------------------
include $(LOCAL_PATH)/Android-nnstreamer-prebuilt.mk

#------------------------------------------------------
# native code (sample with nnstreamer)
#------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := nnstreamer-sample
LOCAL_SRC_FILES := nnstreamer-native-sample.c
LOCAL_CFLAGS += -O2
LOCAL_C_INCLUDES := $(NNSTREAMER_INCLUDES)
LOCAL_SHARED_LIBRARIES := $(NNSTREAMER_LIBS)
LOCAL_LDLIBS := -llog -landroid

include $(BUILD_SHARED_LIBRARY)
