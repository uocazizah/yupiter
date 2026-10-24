# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# yupiter - installation
#
# "make install" honours DESTDIR, so a package builder can install into
# a staging tree:
#
#     make install DESTDIR=/tmp/stage
#
# The prefix is fixed when ./configure runs; it is not a make variable
# that can be overridden at install time, because the package metadata
# is generated from it.
#
# When the shared object is installed into the running system, the
# dynamic linker cache is refreshed so that a program linked against
# libyupiter.so.1 finds it without LD_LIBRARY_PATH.  A staged
# installation (DESTDIR non-empty) leaves the cache alone: the package
# manager of the target system owns that step.

LDCONFIG ?= ldconfig

.PHONY: install uninstall install-headers install-libraries \
        install-metadata

install: all install-headers install-libraries install-metadata
ifeq ($(BUILD_SHARED),yes)
ifneq ($(DESTDIR),)
	@echo "install: DESTDIR is set; the dynamic linker cache is not updated"
else
	@if [ -z "$(LDCONFIG)" ]; then \
	    echo "install: LDCONFIG is empty; the dynamic linker cache is not updated"; \
	elif command -v "$(LDCONFIG)" >/dev/null 2>&1; then \
	    if "$(LDCONFIG)"; then \
	        if "$(LDCONFIG)" -p | grep -F "libyupiter.so.$(SOVERSION)" >/dev/null 2>&1; then \
	            echo "install: updated the dynamic linker cache"; \
	        else \
	            echo "install: warning: $(LIBDIR) is not in the dynamic linker" >&2; \
	            echo "  search path; add it to /etc/ld.so.conf.d/ and run" >&2; \
	            echo "  \"$(LDCONFIG)\", or set LD_LIBRARY_PATH" >&2; \
	        fi; \
	    else \
	        echo "install: warning: could not update the dynamic linker" >&2; \
	        echo "  cache; run \"$(LDCONFIG)\" as root if the library is" >&2; \
	        echo "  not found at run time" >&2; \
	    fi; \
	else \
	    echo "install: warning: $(LDCONFIG) was not found; the dynamic" >&2; \
	    echo "  linker cache was not updated" >&2; \
	fi
endif
endif

install-headers:
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL) -m 644 include/yupiter.h include/yupiter.hpp \
	    $(DESTDIR)$(INCLUDEDIR)

install-libraries:
ifeq ($(BUILD_STATIC),yes)
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 644 $(STATIC_LIB) $(DESTDIR)$(LIBDIR)/libyupiter.a
endif
ifeq ($(BUILD_SHARED),yes)
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 755 $(SHARED_LIB) \
	    $(DESTDIR)$(LIBDIR)/$(SHARED_REAL_NAME)
	ln -sf $(SHARED_REAL_NAME) $(DESTDIR)$(LIBDIR)/$(SHARED_SONAME_NAME)
	ln -sf $(SHARED_REAL_NAME) $(DESTDIR)$(LIBDIR)/$(SHARED_LINK_NAME)
endif

install-metadata: $(PACKAGE_FILES)
	$(INSTALL) -d $(DESTDIR)$(PKGCONFIGDIR)
	$(INSTALL) -m 644 $(PC_FILE) $(DESTDIR)$(PKGCONFIGDIR)/yupiter.pc
	$(INSTALL) -d $(DESTDIR)$(CMAKEDIR)
	$(INSTALL) -m 644 $(CMAKE_CONFIG) \
	    $(DESTDIR)$(CMAKEDIR)/yupiter-config.cmake
	$(INSTALL) -m 644 $(CMAKE_VERSION) \
	    $(DESTDIR)$(CMAKEDIR)/yupiter-config-version.cmake

uninstall:
	rm -f $(DESTDIR)$(INCLUDEDIR)/yupiter.h
	rm -f $(DESTDIR)$(INCLUDEDIR)/yupiter.hpp
	rm -f $(DESTDIR)$(LIBDIR)/libyupiter.a
	rm -f $(DESTDIR)$(LIBDIR)/$(SHARED_LINK_NAME)
	rm -f $(DESTDIR)$(LIBDIR)/$(SHARED_SONAME_NAME)
	rm -f $(DESTDIR)$(LIBDIR)/$(SHARED_REAL_NAME)
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/yupiter.pc
	rm -f $(DESTDIR)$(CMAKEDIR)/yupiter-config.cmake
	rm -f $(DESTDIR)$(CMAKEDIR)/yupiter-config-version.cmake
	-rmdir $(DESTDIR)$(CMAKEDIR) $(DESTDIR)$(PKGCONFIGDIR) 2>/dev/null || true
ifeq ($(DESTDIR),)
ifeq ($(BUILD_SHARED),yes)
	@if [ -n "$(LDCONFIG)" ] && command -v "$(LDCONFIG)" >/dev/null 2>&1; then \
	    if ! "$(LDCONFIG)" >/dev/null 2>&1; then \
	        echo "uninstall: warning: could not update the dynamic linker cache" >&2; \
	    fi; \
	fi
endif
endif
