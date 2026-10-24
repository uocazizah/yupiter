/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - a minimal standard C library
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
 * Operating-system system-call wrappers.
 *
 * This file is the only place where Linux system-call numbers and the
 * "-errno in the return register" convention appear.  Everything above
 * it works in terms of 0 / -1 results, so the rest of the library does
 * not depend on the operating system.  A write is retried for whatever
 * the kernel did not take, so a short write reaches the layer above as
 * a success that really did write everything it was given.
 *
 * Target: Linux, for the architectures the build accepts (aarch64 and
 * x86_64).  The system-call numbers are chosen from the compiler's own
 * target below, and an architecture without an entry there fails the
 * compilation instead of being silently mapped onto numbers that
 * belong to another one.  How the kernel is entered - which registers
 * carry the number, the arguments and the result - belongs to the
 * assembly trampolines (assembly/yupiter_syscall_aarch64.S and
 * assembly/yupiter_syscall_x86_64.S); this file sees a plain C function.
 *
 * The public wrappers carry the C linkage the internal header
 * declares, so the C tests can call them under their unmangled names.
 */

#include <cstddef>
#include <cstdint>

#include "yupiter_io.hxx"

namespace yupiter::io
{

namespace
{

/*
 * Linux system-call numbers for the target architecture.
 *
 * They are numeric on purpose: no "<asm/unistd.h>" or other kernel
 * header is consulted, so the build has no dependency outside this
 * repository.  The numbers follow the compiler's target, so a
 * translation unit compiled for an architecture without an entry here
 * is rejected by the "#error" rather than given the numbers of the
 * other one.
 */
#if defined(__aarch64__)
constexpr long syscall_close = 57L;
constexpr long syscall_read = 63L;
constexpr long syscall_write = 64L;
constexpr long syscall_exit_group = 94L;
#elif defined(__x86_64__)
constexpr long syscall_close = 3L;
constexpr long syscall_read = 0L;
constexpr long syscall_write = 1L;
constexpr long syscall_exit_group = 231L;
#else
#error "yupiter: no Linux system-call numbers for this architecture"
#endif

/*
 * Lowest value the kernel uses to report a failure.
 *
 * A failure comes back as "-errno" with errno in the range 1 .. 4095,
 * so every failure is at least -4095.  Read as an unsigned value that
 * is the very top of the range, above every count a successful call
 * can return - counts are not bounded by 4095, and a write of a whole
 * buffer is one number as ordinary as a write of four bytes - which
 * is what lets a single comparison classify both.
 */
constexpr long kernel_error_floor = -4095L;

/*
 * Translate a raw kernel result into the 0 / -1 convention, optionally
 * reporting what the kernel did.
 *
 * The kernel reports a failure as "-errno" in the range -4095 .. -1 and
 * a count of what it completed otherwise.  Comparing the raw value as
 * an unsigned integer against -4095 separates the two in one step: a
 * failure lands in the top 4095 values of the unsigned range, while
 * every count - however large - stays below that.  Comparing against
 * the magnitude of the error instead would read any count of 4096 or
 * more as a failure, which is a whole buffer's worth of output
 * reported as lost.
 *
 * The count is stored in "*p_count" when that is not null.  The
 * comparison is done on the unsigned representation so that it is well
 * defined for every long value, including negative ones.
 */
int yupiter_kernel_result(long result, std::size_t *p_count)
{
    unsigned long magnitude;

    magnitude = static_cast<unsigned long>(result);

    if (magnitude >= static_cast<unsigned long>(kernel_error_floor))
    {
        return -1;
    }

    if (p_count != nullptr)
    {
        *p_count = static_cast<std::size_t>(result);
    }

    return 0;
}

} /* namespace */

/*
 * Write "length" bytes, repeating the system call for whatever the
 * kernel did not take.
 *
 * A blocking write completes the whole transfer before it returns, but a
 * short count is a normal result for a terminal whose output has been
 * stopped and interrupted, for a non-blocking descriptor, and for
 * anything else that runs out of room mid-transfer, so handing the
 * buffer over once and reporting success would silently drop the rest
 * of it.  A result of zero for a request that was not zero is treated
 * as a failure instead of being retried: it would otherwise repeat
 * forever without the write ever making progress.
 */
extern "C" int yupiter_sys_write(int fd, const void *p_buffer,
                               std::size_t length)
{
    const unsigned char *p_bytes;
    std::size_t          offset;
    std::size_t          remaining;

    if ((p_buffer == nullptr) && (length != 0))
    {
        return -1;
    }

    p_bytes = static_cast<const unsigned char *>(p_buffer);
    offset = 0;
    remaining = length;

    while (remaining > 0)
    {
        std::size_t    written;
        std::uintptr_t address;

        address = reinterpret_cast<std::uintptr_t>(p_bytes + offset);

        if (yupiter_kernel_result(
                yupiter_syscall3(syscall_write, static_cast<long>(fd),
                               static_cast<long>(address),
                               static_cast<long>(remaining)),
                &written) != 0)
        {
            return -1;
        }

        if (written == 0)
        {
            return -1;
        }

        /*
         * A kernel that reports more than it was asked for would push
         * the cursor past the end of the object, so the result is not
         * trusted beyond that point.
         */
        if (written > remaining)
        {
            return -1;
        }

        offset = offset + written;
        remaining = remaining - written;
    }

    return 0;
}

/*
 * Read into "p_buffer".
 *
 * A read is reported the same way as every other wrapper, so a short
 * read is a success here; the count the kernel returns is kept out of
 * this interface until the input layer needs it, rather than being
 * exposed now for a caller that does not exist.
 */
extern "C" int yupiter_sys_read(int fd, void *p_buffer, std::size_t length,
                               std::size_t *p_count)
{
    long result;

    if ((p_buffer == nullptr) && (length != 0))
    {
        return -1;
    }

    result = yupiter_syscall3(
        syscall_read, static_cast<long>(fd),
        static_cast<long>(reinterpret_cast<std::uintptr_t>(p_buffer)),
        static_cast<long>(length));

    return yupiter_kernel_result(result, p_count);
}

extern "C" int yupiter_sys_close(int fd)
{
    long result;

    result = yupiter_syscall3(syscall_close, static_cast<long>(fd), 0L, 0L);

    return yupiter_kernel_result(result, nullptr);
}

extern "C" void yupiter_sys_exit(int status)
{
    /*
     * exit_group is used rather than exit so that every thread of the
     * process is terminated; yupiter does not create threads, but a
     * program linked against it may.
     *
     * The system call never returns.  Repeating it rather than
     * spinning keeps the function's promise - the process ends here -
     * even on a kernel that somehow resumed execution, and gives the
     * compiler an external call to reason about, so the loop cannot be
     * mistaken for one that has no observable behaviour.
     */
    for (;;)
    {
        static_cast<void>(
            yupiter_syscall3(syscall_exit_group, static_cast<long>(status),
                           0L, 0L));
    }
}

} /* namespace yupiter::io */
