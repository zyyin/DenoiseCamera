LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#OPENCV_INSTALL_MODULES:=on
#OPENCV_LIB_TYPE:=STATIC
#include /home/yin/OpenCV-2.4.6-android-sdk/sdk/native/jni/OpenCV.mk

LOCAL_LDLIBS +=  -llog -ldl -ljnigraphics

LOCAL_MODULE    := image_processX
LOCAL_SRC_FILES := Filter.cpp
include $(BUILD_SHARED_LIBRARY) 
