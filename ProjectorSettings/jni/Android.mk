LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ucomm_wrapper_jni.cpp

LOCAL_C_INCLUDES += \
    $(JNI_H_INCLUDE) \

LOCAL_SHARED_LIBRARIES := \
    libandroid_runtime \
    libnativehelper \
    libucommunicator \
    libcutils \
    libutils \
    liblog \
    libhardware

#LOCAL_MULTILIB := 32

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LOCAL_MODULE := libprojectorsettings_jni
LOCAL_MODULE_TAGS := optional

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
