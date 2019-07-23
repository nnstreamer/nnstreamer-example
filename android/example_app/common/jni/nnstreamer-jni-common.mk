# This mk file defines common libraries to build the examples

# Save the local path
LOCAL_PATH_TEMP := $(LOCAL_PATH)

LOCAL_PATH := $(call my-dir)

#------------------------------------------------------
# tensorflow-lite
# Android-tensorflow-lite-prebuilt.mk is a script to use prebuilt static library.
# If it needs to utilize tensorflow-lite, modify Android-tensorflow-lite.mk and build your own library.
#------------------------------------------------------
#include $(LOCAL_PATH)/Android-tensorflow-lite.mk
include $(LOCAL_PATH)/Android-tensorflow-lite-prebuilt.mk

#------------------------------------------------------
# ahc (ahc2src, GStreamer android.hardware.Camera2 Source)
#------------------------------------------------------
include $(LOCAL_PATH)/Android-ahc.mk

# Restore the local path
LOCAL_PATH := $(LOCAL_PATH_TEMP)
