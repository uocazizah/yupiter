# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# yupiter - top-level build
#
# Usage:
#
#     ./configure
#     make
#     make check
#     make install
#
# The build is a plain GNU Makefile on purpose: the project is small,
# every compiler flag is part of the design, and nothing about it needs
# a generator.  ./configure detects the target and writes config.mk.
#
# Out-of-source builds are not supported: the build tree is always
# ./build, which is removed by "make clean" and ignored by Git.

ifeq ($(wildcard config.mk),)
$(error config.mk not found: run ./configure first)
endif

include config.mk

include mk/variables.mk
include mk/rules.mk
include mk/library.mk
include mk/tests.mk
include mk/package.mk
include mk/install.mk

.DEFAULT_GOAL := all

.PHONY: all clean distclean help

all: $(ALL_TARGETS)

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -f config.mk

help:
	@echo "Targets:"
	@echo "  all       build the configured libraries (default)"
	@echo "  static    build libyupiter.a"
	@echo "  shared    build libyupiter.so"
	@echo "  check     build and run the test suite"
	@echo "  install   install headers, libraries and package metadata"
	@echo "  uninstall remove the installed files"
	@echo "  clean     remove the build tree"
	@echo "  distclean remove the build tree and config.mk"
