#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

#
# yupiter - run the test suite
#
# Usage:
#
#     sh test/run-tests.sh [build-directory]
#
# The build directory defaults to "build".  Every check prints PASS or
# FAIL and the script exits non-zero when any check failed, so it can
# be used from the Makefile, from GitHub Actions and by hand.
#
# The C test programs are freestanding executables; the two hosted
# programs are the differential reference build and the C++ test.  The
# install test stages an installation into the build tree and builds a
# consumer against it.

set -u

builddir=${1:-build}
srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bindir=$builddir/test
workdir=$builddir/test-work

if [ ! -d "$bindir" ]
then
    echo "run-tests: no test directory \"$bindir\"; run make first" >&2
    exit 1
fi

rm -rf "$workdir"
mkdir -p "$workdir"

checks=0
failures=0

pass()
{
    checks=$((checks + 1))
    echo "PASS $1"
}

fail()
{
    checks=$((checks + 1))
    failures=$((failures + 1))
    echo "FAIL $1"
    shift
    for line in "$@"
    do
        echo "     $line"
    done
}

# Compare a program's output and status with a golden file.
check_output()
{
    name=$1
    program=$2
    expected=$3
    actual=$workdir/$name.out
    error=$workdir/$name.err

    if ! "$program" > "$actual" 2> "$error"
    then
        fail "$name" "the program exited non-zero" "$(cat "$error")"
        return
    fi

    if cmp -s "$expected" "$actual"
    then
        pass "$name"
    else
        fail "$name" "the output does not match $expected"
        diff -u "$expected" "$actual" | head -n 40
    fi
}

# Compare the output of two programs byte for byte.
check_two()
{
    name=$1
    first=$2
    second=$3

    if ! "$first" > "$workdir/$name.a" 2> "$workdir/$name.a.err"
    then
        fail "$name" "$first exited non-zero"
        return
    fi

    if ! "$second" > "$workdir/$name.b" 2> "$workdir/$name.b.err"
    then
        fail "$name" "$second exited non-zero"
        return
    fi

    if cmp -s "$workdir/$name.a" "$workdir/$name.b"
    then
        pass "$name"
    else
        fail "$name" "the two programs printed different output"
        diff -u "$workdir/$name.a" "$workdir/$name.b" | head -n 40
    fi
}

# Run a program and require a successful status.
check_status()
{
    name=$1
    shift

    if "$@" > "$workdir/$name.out" 2> "$workdir/$name.err"
    then
        pass "$name"
    else
        fail "$name" "the program exited non-zero" "$(cat "$workdir/$name.err")"
    fi
}

# Run a program with both streams merged into one file and compare it
# with a golden file, so the relative order of stdout and stderr is
# checked and not only their contents.
check_merged()
{
    name=$1
    program=$2
    expected=$3
    actual=$workdir/$name.merged

    if ! "$program" > "$actual" 2>&1
    then
        fail "$name" "the program exited non-zero"
        return
    fi

    if cmp -s "$expected" "$actual"
    then
        pass "$name"
    else
        fail "$name" "the merged output does not match $expected"
        diff -u "$expected" "$actual" | head -n 40
    fi
}

# Run one scenario of the heap abort test and require status 134, the
# documented status of the safe heap functions.
check_fatal()
{
    name=$1
    scenario=$2

    "$bindir/test_heap_abort" "$scenario" > /dev/null 2>&1
    status=$?

    if [ "$status" -eq 134 ]
    then
        pass "$name"
    else
        fail "$name" "scenario \"$scenario\" ended with status $status," \
                       "the documented fatal status is 134"
    fi
}

# One large write has to arrive whole on a file and through a pipe.
check_largewrite()
{
    name=large_write_is_reported
    size=200000
    pattern=$workdir/$name.pattern
    direct=$workdir/$name.direct
    piped=$workdir/$name.piped

    awk 'BEGIN { for (i = 0; i < 20000; i++) printf "0123456789" }' \
        > "$pattern"

    if ! "$bindir/test_largewrite" > "$direct" 2> "$workdir/$name.err"
    then
        fail "$name" "the file run did not report the write as a success"
        return
    fi

    "$bindir/test_largewrite" 2> "$workdir/$name.pipe.err" | cat > "$piped"

    for destination in "$direct" "$piped"
    do
        actual_size=$(wc -c < "$destination" | tr -d ' ')

        if [ "$actual_size" -ne "$size" ]
        then
            fail "$name" \
                 "$destination holds $actual_size bytes instead of $size"
            return
        fi

        if ! cmp -s "$pattern" "$destination"
        then
            fail "$name" "$destination holds the wrong bytes"
            return
        fi
    done

    pass "$name"
}

# The smoke test, the formatting suite and the C++ smoke test.
check_output index_output "$bindir/test_index" \
             "$srcdir/test/expected-index.txt"
check_output format_output "$bindir/test_format" \
             "$srcdir/test/expected-format.txt"
check_output index_cxx_output "$bindir/test_index_cxx" \
             "$srcdir/test/expected-index.txt"

# The same source against the host C library and against yupiter.
check_two differential "$bindir/test_differential" \
          "$bindir/test_differential_reference"

# Failure paths, the staging buffer, the syscall wrapper and the
# internal string helpers.
check_status error_handling "$bindir/test_errors"
check_merged stream_output "$bindir/test_stream" \
             "$srcdir/test/expected-stream.txt"
check_largewrite
check_status short_write_retry "$bindir/test_shortwrite"
check_status string_checks "$bindir/test_string"

# The allocator and its leak accounting.
check_status heap_checks "$bindir/test_heap"

for scenario in overflow oom double_free invalid_pointer \
                realloc_overflow realloc_oom realloc_double_free \
                realloc_invalid_pointer
do
    check_fatal "heap_abort_$scenario" "$scenario"
done

# The static archive has to stay freestanding, and the shared object
# has to be self-contained and export only the C interface.
archive=$builddir/libyupiter.a
shared=$builddir/libyupiter.so

if [ -f "$archive" ]
then
    if sh "$srcdir/test/check-freestanding.sh" "$archive" "$shared"
    then
        pass archive_is_freestanding
    else
        fail archive_is_freestanding
    fi
else
    fail archive_is_freestanding "no static archive at $archive"
fi

check_status artifact_paths sh "$srcdir/test/check-artifact-paths.sh" \
    "$archive" "$shared"

# Install into a staging tree and build a consumer against it.
if sh "$srcdir/test/install-test.sh" "$builddir"
then
    pass install_and_consume
else
    fail install_and_consume
fi

echo
echo "$checks checks, $failures failed"

if [ "$failures" -ne 0 ]
then
    exit 1
fi
