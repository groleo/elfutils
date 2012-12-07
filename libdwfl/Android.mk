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

supported_platforms := linux-x86 darwin-x86
cur_platform := $(filter $(HOST_OS)-$(HOST_ARCH),$(supported_platforms))

ifdef cur_platform

#
# host libdwfl
#

include $(CLEAR_VARS)

LIBDWFL_SRC_FILES := \
	core-file.c \
	cu.c \
	derelocate.c \
	dwfl_addrdie.c \
	dwfl_addrdwarf.c \
	dwfl_addrmodule.c \
	dwfl_begin.c \
	dwfl_build_id_find_debuginfo.c \
	dwfl_build_id_find_elf.c \
	dwfl_cumodule.c \
	dwfl_dwarf_line.c \
	dwfl_end.c \
	dwfl_error.c \
	dwfl_getdwarf.c \
	dwfl_getmodules.c \
	dwfl_getsrc.c \
	dwfl_getsrclines.c \
	dwfl_line_comp_dir.c \
	dwfl_linecu.c \
	dwfl_lineinfo.c \
	dwfl_linemodule.c \
	dwfl_module_addrdie.c \
	dwfl_module_addrname.c \
	dwfl_module_addrsym.c \
	dwfl_module_build_id.c \
	dwfl_module.c \
	dwfl_module_dwarf_cfi.c \
	dwfl_module_eh_cfi.c \
	dwfl_module_getdwarf.c \
	dwfl_module_getelf.c \
	dwfl_module_getsrc.c \
	dwfl_module_getsrc_file.c \
	dwfl_module_getsym.c \
	dwfl_module_info.c \
	dwfl_module_nextcu.c \
	dwfl_module_register_names.c \
	dwfl_module_report_build_id.c \
	dwfl_module_return_value_location.c \
	dwfl_nextcu.c \
	dwfl_onesrcline.c \
	dwfl_report_elf.c \
	dwfl_segment_report_module.c \
	dwfl_validate_address.c \
	dwfl_version.c \
	elf-from-memory.c \
	find-debuginfo.c \
	gzip.c \
	image-header.c \
	libdwfl_crc32.c \
	libdwfl_crc32_file.c \
	lines.c \
	link_map.c \
	offline.c \
	open.c \
	relocate.c \
	segment.c

LOCAL_SRC_FILES := \
	$(LIBDWFL_SRC_FILES) \
	linux-kernel-modules.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../libdwfl \
	$(LOCAL_PATH)/../libebl \
	$(LOCAL_PATH)/../libdw  \
	$(LOCAL_PATH)/../libelf \
	$(LOCAL_PATH)/../../zlib


LOCAL_C_INCLUDES += $(LOCAL_PATH)/../host-$(HOST_OS)-fixup

LOCAL_CFLAGS += -DHAVE_CONFIG_H -std=gnu99 -D_GNU_SOURCE

# to suppress the "pointer of type ‘void *’ used in arithmetic" warning
LOCAL_CFLAGS += -Wno-pointer-arith

ifeq ($(HOST_OS),darwin)
	LOCAL_CFLAGS += -fnested-functions
endif

# to fix machine-dependent issues
LOCAL_CFLAGS += -include $(LOCAL_PATH)/../host-$(HOST_OS)-fixup/AndroidFixup.h
LOCAL_SHARED_LIBRARIES := libelfrh libdwrh libeblrh libz-host

LOCAL_MODULE:= libdwflrh

include $(BUILD_HOST_SHARED_LIBRARY)
#
# target libdwfl
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(LIBDWFL_SRC_FILES)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../lib \
	$(LOCAL_PATH)/../libdwfl \
	$(LOCAL_PATH)/../libebl \
	$(LOCAL_PATH)/../libdw \
	$(LOCAL_PATH)/../libelf \
	$(LOCAL_PATH)/../../zlib

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../bionic-fixup

LOCAL_CFLAGS += -include $(LOCAL_PATH)/../bionic-fixup/AndroidFixup.h

LOCAL_CFLAGS += -DHAVE_CONFIG_H -std=gnu99 -Werror

# to suppress the "pointer of type ‘void *’ used in arithmetic" warning
LOCAL_CFLAGS += -Wno-pointer-arith

LOCAL_SHARED_LIBRARIES := libelfrh libdwrh libeblrh libz

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= libdwflrh

include $(BUILD_SHARED_LIBRARY)

endif #cur_platform
