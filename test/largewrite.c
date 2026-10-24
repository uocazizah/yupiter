/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - short write retry test
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
 * One buffer, much larger than the stream buffer and larger than a
 * pipe can hold, handed to the write wrapper in a single call.
 *
 * CheckLargeWrite.cmake runs this program twice - once with stdout on a
 * regular file and once with stdout on a pipe - and checks the status,
 * the length and the bytes of each run.
 *
 * The two destinations are different kernel paths.  A file takes the
 * buffer as it is; a pipe has to carry it out in pipe-sized pieces
 * while the program is blocked in the call, and the kernel reports the
 * whole transfer as one count when it is done.  A wrapper that read a
 * count above 4095 as an error fails the first run's status; a wrapper
 * that lost part of a multi-piece transfer fails the second run's
 * bytes.
 *
 * The retry loop in yupiter_sys_write is not what the pipe run exercises:
 * a blocking write completes the whole transfer before it returns, so
 * the kernel does not hand back a short count here.  A short count is
 * defensive - it is what a terminal produces when a signal arrives
 * mid-write - and cannot be produced on demand; docs/testing records
 * that.  What the pipe run does pin is that a transfer which the kernel
 * has to carry out in pieces arrives whole and is reported as success.
 *
 * The size is well above the 4096 a kernel error can reach and well
 * above any pipe capacity, so neither property depends on timing.
 */

#include <stddef.h>

#include "yupiter_io.hxx"

#define PAYLOAD_SIZE 200000

static unsigned char payload[PAYLOAD_SIZE];

int main(void)
{
    size_t index;

    /*
     * A note for whoever runs this program by hand.  It goes to the
     * unbuffered stream, never to stdout, because stdout is nothing but
     * the payload under test.
     *
     * The note has a second effect, and it is why the call is here
     * rather than left out: reaching for the stream objects gives the
     * read-write segment file-backed content.  A static executable
     * whose read-write segment has none gets that segment mapped from
     * a file offset past the end of the file, and the kernel then
     * terminates the process with SIGBUS on the first access to the
     * .bss that follows - before main() does anything.  Nothing to do
     * with this test in particular; it is a property of linking
     * without a C runtime.  See docs/building.
     */
    yupiter_fprintf(yupiter_stderr,
                  "large write test: writing %d bytes to stdout\n",
                  PAYLOAD_SIZE);

    for (index = 0; index < PAYLOAD_SIZE; index = index + 1)
    {
        payload[index] = (unsigned char)('0' + (index % 10u));
    }

    if (yupiter_sys_write(1, payload, PAYLOAD_SIZE) != 0)
    {
        return 1;
    }

    return 0;
}
