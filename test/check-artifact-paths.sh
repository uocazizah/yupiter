#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# Regression for argument/response-file injection in the artifact checker.
set -eu
srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
archive=$(realpath -- "$1")
shared=${2:-}
if [ -n "$shared" ] && [ -f "$shared" ]; then
    shared=$(realpath -- "$shared")
else
    shared=
fi
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT INT TERM
cd "$tmpdir"
for name in ordinary @response --plugin=untrusted 'artifact with spaces'; do
    cp -- "$archive" "./$name.a"
    if [ -n "$shared" ]; then
        cp -- "$shared" "./$name.so"
        sh "$srcdir/test/check-freestanding.sh" "$name.a" "$name.so"
    else
        sh "$srcdir/test/check-freestanding.sh" "$name.a"
    fi
done
