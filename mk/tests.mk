# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# yupiter - test programs
#
# The C tests are freestanding executables: they link the static
# archive together with the startup file of the target and no C
# runtime, so the only printf, the only _start and the only string
# helpers they can use are the ones this project provides.
#
# Two programs are hosted by design: test_differential_reference is the
# reference half of the differential test and calls the host C library,
# and test_index_cxx is a C++17 program that proves the public header
# and the archive are usable from C++ without C++23.

.PHONY: check

$(TESTDIR)/test_%: test/%.c $(STATIC_LIB) $(START_OBJECT) $(TEST_HEADERS)
	@mkdir -p $(TESTDIR)
	$(CC) $(TEST_CFLAGS) $(TEST_CPPFLAGS) -o $@ $< \
	    $(START_OBJECT) $(STATIC_LIB) $(TEST_LDFLAGS)

$(TESTDIR)/test_shortwrite: test/shortwrite.c \
                            test/rawsyscall_$(TARGET_ARCH).S \
                            $(STATIC_LIB) $(START_OBJECT) $(TEST_HEADERS)
	@mkdir -p $(TESTDIR)
	$(CC) $(TEST_CFLAGS) $(TEST_CPPFLAGS) -o $@ \
	    test/shortwrite.c test/rawsyscall_$(TARGET_ARCH).S \
	    $(START_OBJECT) $(STATIC_LIB) $(TEST_LDFLAGS)

$(TESTDIR)/test_differential_reference: test/differential.c $(TEST_HEADERS)
	@mkdir -p $(TESTDIR)
	$(CC) $(TEST_CFLAGS) $(TEST_CPPFLAGS) -DYUPITER_REFERENCE -o $@ $<

$(TESTDIR)/test_index_cxx: test/index.cxx $(STATIC_LIB) $(TEST_HEADERS)
	@mkdir -p $(TESTDIR)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_CPPFLAGS) -o $@ $< $(STATIC_LIB)

ifeq ($(BUILD_TESTS),yes)
check: all $(TEST_BINARIES)
	@sh test/run-tests.sh "$(BUILD_DIR)"
else
check:
	@echo "yupiter: tests are disabled; run ./configure --enable-tests=yes" >&2
	@exit 1
endif
