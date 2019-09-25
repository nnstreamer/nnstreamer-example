# This mk file defines common libraries to build the examples

# Save the local path
LOCAL_PATH_TEMP := $(LOCAL_PATH)

LOCAL_PATH := $(call my-dir)

#------------------------------------------------------
# tensorflow-lite
#------------------------------------------------------
include $(LOCAL_PATH)/Android-tensorflow-lite-prebuilt.mk

#------------------------------------------------------
# ahc (ahc2src, GStreamer android.hardware.Camera2 Source)
#------------------------------------------------------
include $(LOCAL_PATH)/Android-ahc.mk

# Restore the local path
LOCAL_PATH := $(LOCAL_PATH_TEMP)
