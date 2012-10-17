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

#
# libcpu only on host
#

include $(CLEAR_VARS)

LIBCPU_I386_SRC_FILES := \
	i386_disasm.c \
	i386_dis.h \

LOCAL_SRC_FILES := $(LIBCPU_I386_SRC_FILES)

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libcpu_i386

LOCAL_IS_HOST_MODULE := true
intermediates := $(call local-intermediates-dir)

# generate i386.mnemonics
I386_MNEMONICS_GEN := $(addprefix $(intermediates)/,i386.mnemonics)
$(I386_MNEMONICS_GEN) : PRIVATE_CUSTOM_TOOL = m4 -Di386 -DDISASSEMBLER $(PRIVATE_PATH)/defs/i386 > $@.tmp && sed '1,/^%%/d;/^#/d;/^[[:space:]]*$$/d;s/[^:]*:\([^[:space:]]*\).*/MNE(\1)/;s/{[^}]*}//g;/INVALID/d' $@.tmp | sort -u > $@
$(I386_MNEMONICS_GEN) : PRIVATE_PATH := $(LOCAL_PATH)
$(I386_MNEMONICS_GEN) : PRIVATE_TOP := $(ANDROID_BUILD_TOP)
$(I386_MNEMONICS_GEN) :
	$(transform-generated-source)

LOCAL_GENERATED_SOURCES = $(I386_MNEMONICS_GEN)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../libasm \
	$(LOCAL_PATH)/../libebl \
	$(LOCAL_PATH)/../libdw \
	$(LOCAL_PATH)/../libelf \
	$(intermediates) \

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../host-$(HOST_OS)-fixup

LOCAL_CFLAGS += -DHAVE_CONFIG_H -std=gnu99 -D_GNU_SOURCE

# to suppress the "pointer of type ‘void *’ used in arithmetic" warning
LOCAL_CFLAGS += -Wno-pointer-arith

ifeq ($(HOST_OS),darwin)
	LOCAL_CFLAGS += -fnested-functions
endif

# to fix machine-dependent issues
LOCAL_CFLAGS += -include $(LOCAL_PATH)/../host-$(HOST_OS)-fixup/AndroidFixup.h

include $(BUILD_HOST_STATIC_LIBRARY)

endif
