/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - stream staging, discard and ordering test
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
 * Staging, discarding, and the relative order of stdout and stderr.
 *
 * stdout is buffered and pushed out when a newline is staged; stderr is
 * unbuffered.  That is the documented behaviour of the two streams, and
 * it is pinned here in the only way that can pin it: the golden file is
 * the two streams merged into one, taken with "> file 2>&1", so it
 * records where a diagnostic lands relative to output that was produced
 * before it and flushed later.
 *
 * The staging buffer is crossed on purpose: a line longer than
 * YUPITER_BUFFER_SIZE has to bypass or refill the buffer and still
 * arrive whole.  A discarded stretch of bytes has to leave no trace,
 * while everything flushed before the discard has to survive it.
 *
 * The public interface covers all of this except the discard, whose
 * effect a caller can only observe through a failed formatted call.
 * The internal header is included for that one function; see its
 * declaration in include/yupiter_io.hxx.
 */

#include <stddef.h>

#include "yupiter_io.hxx"

/*
 * Longer than the staging buffer, and patterned so that a chunk which
 * was dropped or repeated changes more than the length of the line.
 */
#define LONG_LINE_LENGTH 600

int main(void)
{
    static const char pattern[] = "0123456789abcdef";
    char              long_line[LONG_LINE_LENGTH + 1];
    size_t            index;
    int               written;

    for (index = 0; index < LONG_LINE_LENGTH; index = index + 1)
    {
        long_line[index] = pattern[index % (sizeof(pattern) - 1u)];
    }

    long_line[LONG_LINE_LENGTH] = '\0';

    /*
     * Both streams.  "stdout-two-staged" has no newline yet when the
     * second stderr line is written, so the unbuffered stream reaches
     * the output first.
     */
    yupiter_printf("order:stdout-one\n");
    yupiter_fprintf(yupiter_stderr, "order:stderr-one\n");
    yupiter_printf("order:stdout-two-staged");
    yupiter_fprintf(yupiter_stderr, "order:stderr-two\n");
    yupiter_printf("-and-completed\n");

    /* Discarding drops the staged bytes and nothing else. */
    yupiter_printf("discard:already-flushed\n");
    yupiter_printf("discard:staged-and-thrown-away");
    yupiter_stream_discard(yupiter_stdout);
    yupiter_printf("discard:still-alive\n");

    /* A line longer than the staging buffer, on both streams. */
    written = yupiter_printf("boundary:%s\n", long_line);
    yupiter_printf("boundary:written=%d\n", written);

    written = yupiter_fprintf(yupiter_stderr, "stderr-boundary:%s\n",
                              long_line);
    yupiter_printf("stderr-boundary:written=%d\n", written);

    return 0;
}
