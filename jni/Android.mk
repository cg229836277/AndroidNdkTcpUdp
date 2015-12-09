LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := Echo
LOCAL_SRC_FILES := Echo.cpp

include $(BUILD_SHARED_LIBRARY)
