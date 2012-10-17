# Copyright (C) 2012 Intel
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

supported_platforms := linux-x86 darwin-x86
cur_platform := $(filter $(HOST_OS)-$(HOST_ARCH),$(supported_platforms))

ifdef cur_platform

I386_SRC_FILES := \
	i386_init.c \
	i386_symbol.c \
	i386_corenote.c \
	i386_cfi.c \
	i386_retval.c \
	i386_regs.c \
	i386_auxv.c \
	i386_syscall.c \

ARM_SRC_FILES =: \
	arm_init.c \
	arm_symbol.c \
	arm_regs.c \
	arm_corenote.c \
	arm_auxv.c \
	arm_attrs.c \
	arm_retval.c \

#
# ebl backends only for host
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(I386_SRC_FILES)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../libelf \
	$(LOCAL_PATH)/../libebl \
	$(LOCAL_PATH)/../libasm \
	$(LOCAL_PATH)/../libdw \

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../host-$(HOST_OS)-fixup

LOCAL_CFLAGS += -DHAVE_CONFIG_H -std=gnu99 -D_GNU_SOURCE

# to suppress the "pointer of type ‘void *’ used in arithmetic" warning
LOCAL_CFLAGS += -Wno-pointer-arith

ifeq ($(HOST_OS),darwin)
	LOCAL_CFLAGS += -fnested-functions
endif

# to fix machine-dependent issues
LOCAL_CFLAGS += -include $(LOCAL_PATH)/../host-$(HOST_OS)-fixup/AndroidFixup.h
LOCAL_SHARED_LIBRARIES := libdwrh libelfrh
LOCAL_STATIC_LIBRARIES := libcpu_i386

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libebl_i386

include $(BUILD_HOST_SHARED_LIBRARY)

endif
