# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# yupiter - package metadata
#
# The pkg-config file and the CMake package configuration are generated
# from the templates in etc/ with sed.  No CMake is needed to build the
# project; the configuration file is for consumers that use CMake.

PC_FILE          := $(BUILD_DIR)/yupiter.pc
CMAKE_CONFIG     := $(BUILD_DIR)/yupiter-config.cmake
CMAKE_VERSION    := $(BUILD_DIR)/yupiter-config-version.cmake
PACKAGE_FILES    := $(PC_FILE) $(CMAKE_CONFIG) $(CMAKE_VERSION)

$(PC_FILE): etc/yupiter.pc.in config.mk
	@mkdir -p $(BUILD_DIR)
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@LIBDIR@|$(LIBDIR)|g' \
	    -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|g' \
	    -e 's|@VERSION@|$(VERSION)|g' \
	    etc/yupiter.pc.in > $@

$(CMAKE_CONFIG): etc/yupiter-config.cmake.in config.mk
	@mkdir -p $(BUILD_DIR)
	@components=$$(printf '%s' '$(LIBDIR_REL)' | awk -F/ '{ print NF }'); \
	ups=''; i=0; \
	while [ $$i -lt $$((components + 2)) ]; do \
	    ups="$$ups/.."; \
	    i=$$((i + 1)); \
	done; \
	sed -e "s|@PREFIX_UP@|$$ups|g" \
	    -e 's|@INCLUDEDIR_REL@|$(INCLUDEDIR_REL)|g' \
	    -e 's|@LIBDIR_REL@|$(LIBDIR_REL)|g' \
	    -e 's|@SOVERSION@|$(SOVERSION)|g' \
	    -e 's|@HAVE_SHARED@|$(HAVE_SHARED)|g' \
	    -e 's|@HAVE_STATIC@|$(HAVE_STATIC)|g' \
	    etc/yupiter-config.cmake.in > $@

$(CMAKE_VERSION): etc/yupiter-config-version.cmake.in config.mk
	@mkdir -p $(BUILD_DIR)
	sed -e 's|@VERSION@|$(VERSION)|g' \
	    -e 's|@VERSION_MAJOR@|$(VERSION_MAJOR)|g' \
	    etc/yupiter-config-version.cmake.in > $@
