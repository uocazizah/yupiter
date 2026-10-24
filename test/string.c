/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - internal string helper test
 * Copyright (C) 2026 Uoc Azizah
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Alternatively, this program may be used, copied, modified and
 * distributed under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Alternatively, and at your option, this program may be used,
 * copied, modified and distributed under the terms of the Apache
 * License, Version 2.0, in which case the provisions of that
 * license apply instead of those above.  You may obtain a copy of
 * the License at <https://www.apache.org/licenses/LICENSE-2.0>.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License and the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and the GNU Lesser General Public License along with this program;
 * if not, see <https://www.gnu.org/licenses/>.
 */

/*
 * The internal helpers behind the formatting engine: length, copy and
 * fill.
 *
 * The null and zero-length cases are part of the contract in
 * include/yupiter_io.hxx.  They are checked here rather than left to the
 * callers, because a helper whose safety depends on every caller
 * remembering a guard is one edit away from a null dereference.  The
 * program carries its own verdict in its exit status.
 */

#include <stddef.h>

#include "yupiter_io.hxx"

static int failures;

static void check(int condition, const char *p_what)
{
    if (condition == 0)
    {
        failures = failures + 1;
        yupiter_fprintf(yupiter_stderr, "FAIL: %s\n", p_what);
    }
}

static void test_length(void)
{
    check(yupiter_strlen(NULL) == 0, "strlen of NULL is zero");
    check(yupiter_strlen("") == 0, "strlen of an empty string is zero");
    check(yupiter_strlen("a") == 1, "strlen of one character is one");
    check(yupiter_strlen("yupiter") == 7, "strlen counts every character");
}

static void test_copy(void)
{
    static const unsigned char source[37] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25
    };
    unsigned char destination[37];
    size_t        index;
    int           same;
    void         *p_result;
    unsigned char marker[4] = { 0x7e, 0x7f, 0x80, 0x81 };

    for (index = 0; index < sizeof destination; index = index + 1)
    {
        destination[index] = 0u;
    }

    p_result = yupiter_memcpy(destination, source, sizeof source);
    check(p_result == destination, "memcpy returns the destination");

    same = 1;

    for (index = 0; index < sizeof source; index = index + 1)
    {
        if (destination[index] != source[index])
        {
            same = 0;
        }
    }

    check(same != 0, "memcpy copies every byte, including the block tail");

    check(yupiter_memcpy(NULL, source, sizeof source) == NULL,
          "memcpy with a null destination returns it");
    check(yupiter_memcpy(destination, NULL, sizeof source) == destination,
          "memcpy with a null source returns the destination");
    check(yupiter_memcpy(NULL, NULL, 0) == NULL,
          "memcpy of nothing between null pointers returns null");

    p_result = yupiter_memcpy(marker, marker, sizeof marker);
    check(p_result == marker, "memcpy of a buffer onto itself succeeds");
    check((marker[0] == 0x7eu) && (marker[3] == 0x81u),
          "memcpy of a buffer onto itself leaves it unchanged");
}

static void test_fill(void)
{
    unsigned char buffer[37];
    size_t        index;
    int           all_set;
    void         *p_result;

    p_result = yupiter_memset(buffer, 0x5a, sizeof buffer);
    check(p_result == buffer, "memset returns the destination");

    all_set = 1;

    for (index = 0; index < sizeof buffer; index = index + 1)
    {
        if (buffer[index] != 0x5au)
        {
            all_set = 0;
        }
    }

    check(all_set != 0, "memset fills every byte, including the block tail");

    p_result = yupiter_memset(buffer, 0, 0);
    check(p_result == buffer, "memset of nothing returns the destination");

    check(yupiter_memset(NULL, 0x11, 8) == NULL,
          "memset with a null destination returns it");
}

int main(void)
{
    failures = 0;

    test_length();
    test_copy();
    test_fill();

    if (failures == 0)
    {
        yupiter_fprintf(yupiter_stderr, "string: all checks passed\n");
    }

    return failures;
}
