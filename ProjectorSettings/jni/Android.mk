LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ucomm_wrapper_jni.cpp

# jni.h
LOCAL_HEADER_LIBRARIES := jni_headers
LOCAL_USE_VNDK := true

LOCAL_SHARED_LIBRARIES := \
    libucommunicator \
    libcutils \
    libutils \
    liblog \
    libhardware

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_MODULE := libprojectorsettings_jni
LOCAL_MODULE_TAGS := optional

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
