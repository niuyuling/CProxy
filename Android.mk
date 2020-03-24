LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_CFLAGS = -O2 -pie -Wall
LOCAL_LDFLAGS = -O2 -pie -Wall
LOCAL_ARM_MODE = arm
LOCAL_MODULE = CProxy
LOCAL_MODULE_FILENAME = CProxy
c_src_files = $(wildcard $(LOCAL_PATH)/*.c)
LOCAL_SRC_FILES = $(c_src_files:$(LOCAL_PATH)/%=%)
include $(BUILD_EXECUTABLE)
