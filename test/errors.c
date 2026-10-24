/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - failure path test
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
 * The paths on which the formatted functions report failure.  Each
 * check adds to a counter that becomes the exit status, so the test
 * carries its own verdict and needs no golden file.
 *
 * The internal header is included deliberately: a stream bound to a
 * bad descriptor cannot be obtained through the public interface, and
 * the sticky-error behaviour that follows a failed write is worth
 * testing.
 *
 * A few cases use a format string that "-Wformat" rejects because the
 * library defines the behaviour on purpose: a width or precision
 * literal too large for an "int" and an empty format string.  Those
 * calls go through a function pointer, so the attribute on the
 * declaration does not apply and the rest of the file keeps the
 * compile-time check.
 */

#include "yupiter_io.hxx"

typedef int (*print_function)(const char *, ...);
typedef int (*stream_print_function)(yupiter_FILE *, const char *, ...);

static print_function volatile printf_unchecked = yupiter_printf;
static stream_print_function volatile fprintf_unchecked = yupiter_fprintf;

static int failures;

/*
 * Record a check that did not hold.
 */
static void check(int condition, const char *p_what)
{
    if (condition == 0)
    {
        failures = failures + 1;
        yupiter_fprintf(yupiter_stderr, "FAIL: %s\n", p_what);
    }
}

/*
 * A stream bound to a descriptor that does not exist.  Writing to it
 * has to fail, and the failure has to stick.
 */
static yupiter_FILE broken_stream =
{
    .fd = -1,
    .error = 0,
    .unbuffered = 1,
    .length = 0,
    .buffer = { 0 }
};

int main(void)
{
    failures = 0;

    /* A field wider than "int" is a failure, not a very wide field. */
    check(printf_unchecked("%2147483648d", 1) == -1,
          "width literal overflows");
    check(printf_unchecked("%99999999999d", 1) == -1,
          "wide width literal overflows");
    check(printf_unchecked("[%4294967296d]", 1) == -1,
          "huge width literal overflows");
    check(printf_unchecked("%.99999999999d", 1) == -1,
          "precision literal overflows");

    /* A field that cannot fit in the return value is a failure too. */
    check(yupiter_printf("%2147483647d", 1) == -1,
          "field exceeds return value");
    check(yupiter_printf("%2147483647s", "x") == -1, "string field too wide");

    /* Null arguments. */
    check(yupiter_printf(NULL) == -1, "null format");
    check(yupiter_fprintf(NULL, "x") == -1, "null stream");
    check(yupiter_fprintf(yupiter_stdout, NULL) == -1,
          "null format with stream");

    /* A write that the operating system rejects. */
    check(yupiter_fprintf(&broken_stream, "x") == -1,
          "write to a bad descriptor");
    check(broken_stream.error != 0, "failure is recorded in the stream");
    check(yupiter_fprintf(&broken_stream, "y") == -1,
          "a stream in error keeps failing");
    check(yupiter_stream_flush(&broken_stream) == -1,
          "flushing a stream in error fails");

    /* Nothing staged must flush cleanly, and the good streams work. */
    check(yupiter_stream_flush(yupiter_stdout) == 0,
          "flush with nothing staged");
    check(printf_unchecked("") == 0, "empty format");
    check(fprintf_unchecked(yupiter_stderr, "") == 0,
          "empty format on stderr");

    if (failures == 0)
    {
        yupiter_fprintf(yupiter_stderr, "error handling: all checks passed\n");
    }

    return failures;
}
