# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO := comm_server
LOCAL_COPY_HEADERS := ./ucomm_ext.h
include $(BUILD_COPY_HEADERS)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := ucommsvr.c ucommsvr_input.c expatparser.c
LOCAL_C_INCLUDES := external/expat/lib
LOCAL_SHARED_LIBRARIES := liblog libcutils libexpat libpolyreg
LOCAL_MODULE := ucommsvr
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := sony
LOCAL_INIT_RC_64   := vendor/etc/init/ucommsvr.rc
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := ucomm_autofocus_test.c
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_MODULE := ucomm_autofocus_test
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := sony
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := ucomm_ctl.c
LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \

LOCAL_MODULE := libucommunicator
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := sony
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
