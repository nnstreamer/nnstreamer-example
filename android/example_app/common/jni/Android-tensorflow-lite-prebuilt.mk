#------------------------------------------------------
# tensorflow-lite
#
# This mk file defines tensorflow-lite module with prebuilt static library.
# To build and run the example with gstreamer binaries, we built a static library (e.g., libtensorflow-lite.a)
# Download and decompress prebuilt library from nnsuite android-resource repository.
# (https://github.com/nnsuite/nnstreamer-android-resource/tree/master/android_api/ext-files)
#------------------------------------------------------
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := tensorflow-lite

TF_LITE_DIR := $(LOCAL_PATH)/tensorflow-lite
TF_LITE_INCLUDES := $(TF_LITE_DIR)/include

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
TF_LITE_LIB_PATH := $(TF_LITE_DIR)/lib/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
TF_LITE_LIB_PATH := $(TF_LITE_DIR)/lib/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
TF_LITE_LIB_PATH := $(TF_LITE_DIR)/lib/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
TF_LITE_LIB_PATH := $(TF_LITE_DIR)/lib/x86_64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

LOCAL_SRC_FILES := $(TF_LITE_LIB_PATH)/libtensorflow-lite.a

include $(PREBUILT_STATIC_LIBRARY)
