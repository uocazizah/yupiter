#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

#
# yupiter - install the library and build a program against it
#
# Usage:
#
#     sh test/install-test.sh [build-directory]
#
# Everything happens under <build-directory>/install-test, which is
# thrown away and rebuilt, so the test never touches the source tree
# and can be run as many times as wanted.
#
# What is checked:
#
#   1. the configured installation stages into a DESTDIR tree;
#   2. the headers, both libraries and the package metadata are where
#      the install rules promised to put them;
#   3. a separate consumer builds and runs against pkg-config;
#   4. when CMake is installed, the same consumer builds and runs with
#      find_package(yupiter), which proves the installed CMake package.
#
# The failure of any step stops the test: a partially installed package
# is not a pass.

set -u

builddir=${1:-build}
srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# CMake treats a relative CMAKE_PREFIX_PATH relative to its own working
# directory, so the staging tree is addressed by an absolute path.
case "$builddir" in
    /*)
        ;;
    *)
        builddir=$(pwd)/$builddir
        ;;
esac

workdir=$builddir/install-test
stage=$workdir/stage
log=$workdir/install.log

if [ ! -f "$srcdir/config.mk" ]
then
    echo "install-test: no config.mk; run ./configure first" >&2
    exit 1
fi

value()
{
    sed -n "s/^$1[[:space:]]*:=[[:space:]]*//p" "$srcdir/config.mk" |
        head -n 1
}

prefix=$(value PREFIX)
libdir_rel=$(value LIBDIR_REL)
includedir_rel=$(value INCLUDEDIR_REL)
build_static=$(value BUILD_STATIC)
build_shared=$(value BUILD_SHARED)
version=$(value VERSION)
soversion=$(value SOVERSION)

if [ -z "$prefix" ] || [ -z "$libdir_rel" ] || [ -z "$includedir_rel" ]
then
    echo "install-test: config.mk is incomplete; re-run ./configure" >&2
    exit 1
fi

rm -rf "$workdir"
mkdir -p "$workdir"

if ! make -C "$srcdir" BUILD_DIR="$builddir" install DESTDIR="$stage" \
        > "$log" 2>&1
then
    echo "install-test: make install failed:" >&2
    cat "$log" >&2
    exit 1
fi

# The installation root inside the staging tree.
root=$stage$prefix
libdir=$root/$libdir_rel
includedir=$root/$includedir_rel

missing=0

for header in yupiter.h yupiter.hpp
do
    if [ ! -f "$includedir/$header" ]
    then
        echo "install-test: the package has no $includedir_rel/$header" >&2
        missing=1
    fi
done

if [ "$build_static" = yes ] && [ ! -f "$libdir/libyupiter.a" ]
then
    echo "install-test: the package has no $libdir_rel/libyupiter.a" >&2
    missing=1
fi

if [ "$build_shared" = yes ]
then
    for name in "libyupiter.so.$version" "libyupiter.so.$soversion" \
                "libyupiter.so"
    do
        if [ ! -e "$libdir/$name" ]
        then
            echo "install-test: the package has no $libdir_rel/$name" >&2
            missing=1
        fi
    done
fi

for metadata in "pkgconfig/yupiter.pc" \
                "cmake/yupiter/yupiter-config.cmake" \
                "cmake/yupiter/yupiter-config-version.cmake"
do
    if [ ! -f "$libdir/$metadata" ]
    then
        echo "install-test: the package has no $libdir_rel/$metadata" >&2
        missing=1
    fi
done

if [ "$missing" -ne 0 ]
then
    exit 1
fi

# The runtime path for a consumer of the shared library.
runtime_path=$libdir
if [ -n "${LD_LIBRARY_PATH:-}" ]
then
    runtime_path=$runtime_path:$LD_LIBRARY_PATH
fi

consumer_status()
{
    program=$1
    output=$("$program" 2> "$workdir/consumer.err")
    status=$?

    if [ "$status" -ne 0 ]
    then
        echo "install-test: the consumer exited with status $status" >&2
        cat "$workdir/consumer.err" >&2
        return 1
    fi

    if [ "$output" != "consumed yupiter" ]
    then
        echo "install-test: the consumer printed something unexpected:" >&2
        echo "$output" >&2
        return 1
    fi

    return 0
}

# --- pkg-config ------------------------------------------------------

if command -v pkg-config > /dev/null 2>&1
then
    pc_path=$libdir/pkgconfig
    cc=${CC:-cc}

    if ! cflags=$(PKG_CONFIG_PATH="$pc_path" \
                  PKG_CONFIG_SYSROOT_DIR="$stage" \
                  pkg-config --cflags yupiter)
    then
        echo "install-test: pkg-config cannot describe yupiter" >&2
        exit 1
    fi

    if ! libs=$(PKG_CONFIG_PATH="$pc_path" \
                PKG_CONFIG_SYSROOT_DIR="$stage" \
                pkg-config --libs yupiter)
    then
        echo "install-test: pkg-config cannot describe yupiter" >&2
        exit 1
    fi

    case " $libs " in
        *" -lyupiter "*)
            ;;
        *)
            echo "install-test: pkg-config does not name -lyupiter:" >&2
            echo "$libs" >&2
            exit 1
            ;;
    esac

    # The flags are split into words on purpose: pkg-config prints a
    # list of independent options.
    if ! $cc -std=c17 $cflags "$srcdir/test/consumer/main.c" $libs \
            -o "$workdir/consumer-pkg" > "$workdir/pkg.log" 2>&1
    then
        echo "install-test: the pkg-config consumer failed to build:" >&2
        cat "$workdir/pkg.log" >&2
        exit 1
    fi

    if ! LD_LIBRARY_PATH="$runtime_path" \
         consumer_status "$workdir/consumer-pkg"
    then
        exit 1
    fi

    echo "install-test: pkg-config consumer passed"
else
    echo "install-test: pkg-config is not installed; the .pc file was"
    echo "  installed but not exercised"
fi

# --- CMake -----------------------------------------------------------

if command -v cmake > /dev/null 2>&1
then
    if ! cmake -S "$srcdir/test/consumer" -B "$workdir/consumer-build" \
            -DCMAKE_PREFIX_PATH="$root" > "$workdir/cmake.log" 2>&1
    then
        echo "install-test: find_package(yupiter) failed:" >&2
        cat "$workdir/cmake.log" >&2
        exit 1
    fi

    if ! cmake --build "$workdir/consumer-build" \
            >> "$workdir/cmake.log" 2>&1
    then
        echo "install-test: the CMake consumer failed to build:" >&2
        cat "$workdir/cmake.log" >&2
        exit 1
    fi

    if ! LD_LIBRARY_PATH="$runtime_path" \
         consumer_status "$workdir/consumer-build/consumer"
    then
        exit 1
    fi

    echo "install-test: CMake consumer passed"
else
    echo "install-test: cmake is not installed; the CMake package was"
    echo "  installed but not exercised"
fi

exit 0
