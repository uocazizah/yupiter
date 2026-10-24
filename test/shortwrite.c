/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - short-write retry test
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
 * Prove that yupiter_sys_write repeats the write system call until the
 * kernel has taken every byte.
 *
 * A short write cannot be produced on demand from a test: a blocking
 * write completes the whole transfer before it returns, and the cases
 * that do return early - a terminal whose output has been stopped and
 * interrupted by a signal, a non-blocking descriptor - need signal
 * handling or O_NONBLOCK, neither of which the library wraps.  The
 * test therefore replaces "yupiter_syscall3" for the duration of one
 * scenario: the mock records each request and returns a scripted
 * result, while every call it does not script (including the final
 * exit_group) is forwarded to test/rawsyscall_<processor>.S, a copy of
 * the real trampoline.
 *
 * The scenarios cover success after two short writes, a write that
 * makes no progress, a result larger than the request, and a kernel
 * error after a partial transfer.  Each check contributes to the exit
 * status, so the test carries its own verdict and needs no golden
 * file.  Diagnostics are only written once the script is inactive, so
 * they reach the real stderr.
 */

#include <stddef.h>
#include <stdint.h>

#include "yupiter_io.hxx"

/*
 * The real trampoline, compiled into this test from the target's
 * rawsyscall_<processor>.S.  It is declared here rather than in an
 * installed header because it exists only for this test.
 */
long yupiter_test_raw_syscall3(long number, long arg1, long arg2,
                             long arg3);

/*
 * The one system-call number the mock recognises.  The library keeps
 * its own copy in io/syscall_linux.cxx; the test duplicates it on
 * purpose so a change to one without the other is visible here.
 */
#if defined(__x86_64__)
#define YUPITER_TEST_SYSCALL_WRITE 1L
#elif defined(__aarch64__)
#define YUPITER_TEST_SYSCALL_WRITE 64L
#else
#error "yupiter: no short-write syscall numbers for this architecture"
#endif

#define SCRIPT_MAX 8
#define RECORD_MAX 64

static int                  script_active;
static long                 script_values[SCRIPT_MAX];
static size_t               script_count;
static size_t               script_index;
static size_t               write_calls;
static const unsigned char *write_addresses[SCRIPT_MAX];
static size_t               write_lengths[SCRIPT_MAX];
static size_t               recorded_length;
static unsigned char        recorded_bytes[RECORD_MAX];
static int                  record_overflow;
static int                  failures;

/*
 * The replacement for the library's three-argument trampoline.
 *
 * While a scenario is active a write is answered from the script; the
 * bytes the scripted result claims are copied out of the caller's
 * buffer, exactly as the kernel would have consumed them.  Everything
 * else is passed through to the real kernel.
 */
long yupiter_syscall3(long number, long arg1, long arg2, long arg3)
{
    size_t index;

    (void)arg1;

    if ((script_active != 0) && (number == YUPITER_TEST_SYSCALL_WRITE))
    {
        const unsigned char *p_bytes;
        size_t               length;
        long                 result;
        size_t               taken;

        p_bytes = (const unsigned char *)(uintptr_t)arg2;
        length = (size_t)arg3;

        result = 0;

        if (script_index < script_count)
        {
            result = script_values[script_index];
            script_index = script_index + 1;
        }

        if (write_calls < SCRIPT_MAX)
        {
            write_addresses[write_calls] = p_bytes;
            write_lengths[write_calls] = length;
        }

        write_calls = write_calls + 1;

        if ((result > 0) && ((size_t)result <= length))
        {
            taken = (size_t)result;

            if (taken > (RECORD_MAX - recorded_length))
            {
                record_overflow = 1;
            }
            else
            {
                for (index = 0; index < taken; index = index + 1)
                {
                    recorded_bytes[recorded_length + index] = p_bytes[index];
                }

                recorded_length = recorded_length + taken;
            }
        }

        return result;
    }

    return yupiter_test_raw_syscall3(number, arg1, arg2, arg3);
}

static void begin_script(const long *p_values, size_t count)
{
    size_t index;

    script_active = 1;
    script_index = 0;
    script_count = count;
    write_calls = 0;
    recorded_length = 0;
    record_overflow = 0;

    for (index = 0; index < count; index = index + 1)
    {
        script_values[index] = p_values[index];
    }
}

static void end_script(void)
{
    script_active = 0;
}

static void check(int condition, const char *p_what)
{
    if (condition == 0)
    {
        failures = failures + 1;
        yupiter_fprintf(yupiter_stderr, "FAIL: %s\n", p_what);
    }
}

static void check_bytes(const unsigned char *p_expected, size_t length,
                        const char *p_what)
{
    size_t index;
    int    same;

    same = (recorded_length == length) ? 1 : 0;

    for (index = 0; index < length; index = index + 1)
    {
        if (recorded_bytes[index] != p_expected[index])
        {
            same = 0;
        }
    }

    check(same != 0, p_what);
}

/*
 * Two short writes and a final one: the wrapper has to call again with
 * the bytes it has not written yet and report success only when the
 * whole buffer has been taken.
 */
static void test_short_write_is_retried(void)
{
    static const long script[] = { 2L, 1L, 3L };
    static const unsigned char source[] = { 'a', 'b', 'c', 'd', 'e', 'f' };
    int status;

    begin_script(script, (sizeof script) / (sizeof script[0]));
    status = yupiter_sys_write(1, source, sizeof source);
    end_script();

    check(status == 0, "a short write is retried to success");
    check(write_calls == 3, "the write is repeated for every short count");
    check(record_overflow == 0, "the recorded bytes fit the test buffer");

    if (write_calls == 3)
    {
        check(write_lengths[0] == 6, "the first request is the whole buffer");
        check(write_lengths[1] == 4, "the second request skips two bytes");
        check(write_lengths[2] == 3, "the third request skips three bytes");
        check(write_addresses[0] == source,
              "the first request starts at the buffer");
        check(write_addresses[1] == (source + 2),
              "the second request advances by the first count");
        check(write_addresses[2] == (source + 3),
              "the third request advances by the second count");
    }

    check_bytes(source, sizeof source,
                "the retried write delivers every byte once");
}

/*
 * A write that completes nothing must not be retried forever: the
 * wrapper reports a failure after the first result.
 */
static void test_no_progress_fails(void)
{
    static const long script[] = { 0L };
    static const unsigned char source[] = { 'x', 'y' };

    begin_script(script, (sizeof script) / (sizeof script[0]));
    check(yupiter_sys_write(1, source, sizeof source) == -1,
          "a write of zero bytes is a failure");
    end_script();

    check(write_calls == 1, "a zero result is not retried");
    check(recorded_length == 0, "a zero result consumes no bytes");
}

/*
 * A kernel that reports more than it was asked for must not push the
 * cursor past the buffer.
 */
static void test_oversized_result_fails(void)
{
    static const long script[] = { 7L };
    static const unsigned char source[] = { 'x', 'y', 'z', 'w', 'q', 'r' };

    begin_script(script, (sizeof script) / (sizeof script[0]));
    check(yupiter_sys_write(1, source, sizeof source) == -1,
          "a result larger than the request is a failure");
    end_script();

    check(write_calls == 1, "an oversized result stops the transfer");
    check(recorded_length == 0, "an oversized result consumes no bytes");
}

/*
 * A kernel error after a short write is reported, and the bytes the
 * first call took stay observable through the recorded addresses.
 */
static void test_error_after_partial_write(void)
{
    static const long script[] = { 2L, -5L };
    static const unsigned char source[] = { 'a', 'b', 'c', 'd', 'e', 'f' };

    begin_script(script, (sizeof script) / (sizeof script[0]));
    check(yupiter_sys_write(1, source, sizeof source) == -1,
          "a kernel error fails the write");
    end_script();

    check(write_calls == 2, "the error follows one short write");
    check(recorded_length == 2, "the first count is still recorded");

    if (write_calls == 2)
    {
        check(write_addresses[1] == (source + 2),
              "the retry starts after the bytes already written");
    }

    check_bytes(source, 2, "the partial write carries the first bytes");
}

/*
 * A request the kernel accepts in one call stays one call.
 */
static void test_complete_write(void)
{
    static const long script[] = { 6L };
    static const unsigned char source[] = { 'a', 'b', 'c', 'd', 'e', 'f' };

    begin_script(script, (sizeof script) / (sizeof script[0]));
    check(yupiter_sys_write(1, source, sizeof source) == 0,
          "a complete write succeeds");
    end_script();

    check(write_calls == 1, "a complete write is not repeated");
    check_bytes(source, sizeof source, "a complete write delivers the bytes");
}

int main(void)
{
    failures = 0;

    test_short_write_is_retried();
    test_no_progress_fails();
    test_oversized_result_fails();
    test_error_after_partial_write();
    test_complete_write();

    if (failures == 0)
    {
        yupiter_fprintf(yupiter_stderr, "short write: all checks passed\n");
    }

    return failures;
}
