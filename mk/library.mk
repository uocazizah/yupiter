# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# yupiter - static and shared libraries
#
# libyupiter.a is the freestanding archive: it contains the startup
# file, so a program linked with "-nostdlib" gets its entry point from
# it.  libyupiter.so is the hosted form: it contains no entry point and
# is meant to be loaded by a program that already has one.
#
# The shared object carries the usual three names:
#
#     libyupiter.so.1.0.2   the real file
#     libyupiter.so.1       the soname the dynamic linker records
#     libyupiter.so         the link-time name

.PHONY: static shared

static: $(STATIC_LIB)

shared: $(SHARED_LIB) $(SHARED_SONAME) $(SHARED_LINK)

$(STATIC_LIB): $(ARCHIVE_OBJECTS)
	@mkdir -p $(dir $@)
	rm -f $@
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(SHARED_LIB): $(SHARED_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) -shared -nostdlib -Wl,--no-undefined \
	    -Wl,-soname,$(SHARED_SONAME_NAME) -o $@ $^

$(SHARED_SONAME): $(SHARED_LIB)
	ln -sf $(SHARED_REAL_NAME) $@

$(SHARED_LINK): $(SHARED_LIB)
	ln -sf $(SHARED_REAL_NAME) $@
