# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# yupiter - build variables
#
# Everything derived from config.mk lives here: the source lists, the
# object lists, the warning sets and the flags of the two languages.

BUILD_DIR ?= build

OBJDIR  := $(BUILD_DIR)/obj
TESTDIR := $(BUILD_DIR)/test

# Sources.
IO_SOURCES  := io/file.cxx io/fprintf.cxx io/printf.cxx io/string.cxx \
               io/syscall_linux.cxx io/vfprintf.cxx
MEM_SOURCES := mem/heap.cxx
LIB_SOURCES := $(IO_SOURCES) $(MEM_SOURCES)

START_SOURCE    := assembly/yupiter_start_$(TARGET_ARCH).S
SYSCALL_SOURCES := assembly/yupiter_syscall_$(TARGET_ARCH).S \
                   assembly/yupiter_syscall3_$(TARGET_ARCH).S

LIB_OBJECTS     := $(LIB_SOURCES:%.cxx=$(OBJDIR)/%.o)
START_OBJECT    := $(START_SOURCE:%.S=$(OBJDIR)/%.o)
SYSCALL_OBJECTS := $(SYSCALL_SOURCES:%.S=$(OBJDIR)/%.o)

# The startup file belongs to the static archive only.  A hosted
# program already has an entry point from its C runtime, and a shared
# object is loaded by one; shipping "_start" in the shared object would
# export a symbol that does not belong to the interface.
ARCHIVE_OBJECTS := $(LIB_OBJECTS) $(SYSCALL_OBJECTS) $(START_OBJECT)
SHARED_OBJECTS  := $(LIB_OBJECTS) $(SYSCALL_OBJECTS)

STATIC_LIB        := $(BUILD_DIR)/libyupiter.a
SHARED_REAL_NAME  := libyupiter.so.$(VERSION)
SHARED_SONAME_NAME := libyupiter.so.$(SOVERSION)
SHARED_LINK_NAME  := libyupiter.so
SHARED_LIB        := $(BUILD_DIR)/$(SHARED_REAL_NAME)
SHARED_SONAME     := $(BUILD_DIR)/$(SHARED_SONAME_NAME)
SHARED_LINK       := $(BUILD_DIR)/$(SHARED_LINK_NAME)

# Warnings.  The library and the tests share the common set; the
# language-specific additions are the ones the project has always used.
WARNINGS_COMMON := \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Werror \
    -Wconversion \
    -Wsign-conversion \
    -Wshadow \
    -Wcast-qual \
    -Wundef \
    -Wswitch-default \
    -Wdouble-promotion \
    -Wvla

WARNINGS_C := $(WARNINGS_COMMON) -Wstrict-prototypes -Wmissing-prototypes

WARNINGS_CXX := $(WARNINGS_COMMON) \
    -Wold-style-cast \
    -Wnon-virtual-dtor \
    -Woverloaded-virtual \
    -Wzero-as-null-pointer-constant

# Build type.
OPTIONS_debug         := -O0 -g3
OPTIONS_release       := -O2 -DNDEBUG
OPTIONS_relwithdebinfo := -O2 -g -DNDEBUG
OPTIONS_minsize       := -Os -DNDEBUG
OPTIONS               := $(OPTIONS_$(BUILD_TYPE))

# The library is freestanding C++23.  "-fPIC" is used for the objects
# of both libraries so that a static archive can also be linked into a
# position-independent executable, which matters on aarch64.
LIB_RUNTIME_FLAGS := \
    -ffreestanding \
    -fno-builtin \
    -fno-stack-protector \
    -fno-exceptions \
    -fno-rtti \
    -fno-threadsafe-statics

LIB_CPPFLAGS := -Iinclude -DYUPITER_HEAP_SIZE=$(HEAP_SIZE)
LIB_CXXFLAGS := -std=c++23 $(WARNINGS_CXX) $(OPTIONS) \
                $(LIB_RUNTIME_FLAGS) -fPIC -fvisibility=hidden
LIB_ASFLAGS  := $(OPTIONS)

# The C tests are freestanding and link without a C runtime.  They keep
# the runtime assumptions that matter for such a link - no stack
# protector and no compiler builtins - but are not compiled with
# "-ffreestanding": it would make clang mangle main() and drop the
# implicit "return 0" of a main that runs off the end.
TEST_CPPFLAGS := -Iinclude -DYUPITER_HEAP_SIZE=$(HEAP_SIZE)
TEST_CFLAGS   := -std=c17 $(WARNINGS_C) $(OPTIONS) \
                 -fno-builtin -fno-stack-protector
TEST_CXXFLAGS := -std=c++17 $(WARNINGS_CXX) $(OPTIONS) \
                 -fno-builtin -fno-stack-protector
TEST_LDFLAGS  := -nostdlib -static

TEST_HEADERS := include/yupiter.h include/yupiter.hpp \
                include/yupiter_io.hxx include/yupiter_mem.hxx

FREESTANDING_TESTS := test_index test_format test_differential test_errors \
                      test_stream test_largewrite test_shortwrite test_string \
                      test_heap test_heap_abort
HOSTED_TESTS := test_differential_reference test_index_cxx

TEST_BINARIES := $(addprefix $(TESTDIR)/,$(FREESTANDING_TESTS) $(HOSTED_TESTS))

# What "make all" builds.
ALL_TARGETS :=

ifeq ($(BUILD_STATIC),yes)
ALL_TARGETS += static
endif

ifeq ($(BUILD_SHARED),yes)
ALL_TARGETS += shared
endif

# What the package metadata says.
HAVE_STATIC := $(if $(filter yes,$(BUILD_STATIC)),ON,OFF)
HAVE_SHARED := $(if $(filter yes,$(BUILD_SHARED)),ON,OFF)
