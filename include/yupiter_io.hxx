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

#ifndef YUPITER_IO_H
#define YUPITER_IO_H

/*
 * Internal interface shared by the yupiter I/O and platform layers.
 *
 * Nothing declared here is part of the public API, and the header is
 * never installed.  The declarations exist so that the formatting
 * engine, the stream objects and the operating-system wrappers stay
 * separated and individually testable.
 *
 * The declarations are wrapped in an "extern "C"" specification so
 * that the C++23 implementation defines them with the same linkage the
 * declarations promise, and the header itself is kept ISO C17 so that
 * the C test programs can include it too: a test that reaches an
 * internal interface is how the stream discard, the system-call
 * wrapper and the heap accounting are covered.
 */

#include <stdarg.h>
#include <stddef.h>

#include "yupiter.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Size of the per-stream staging buffer, in bytes.
 *
 * The buffer lives inside the stream object, so staging output never
 * allocates: the only heap use in the library is the scratch block
 * that the formatting engine borrows for one formatted call (see
 * io/vfprintf.cxx).  256 bytes keeps a whole formatted line in one
 * write system call in the common case without growing the stream
 * object meaningfully.
 */
#define YUPITER_BUFFER_SIZE 256

/*
 * A yupiter output stream.
 *
 * "fd" is an operating-system file descriptor.  "error" is a sticky
 * failure flag: once set, every further write to the stream fails
 * without touching the system call layer.  "unbuffered" forces the
 * staged bytes out after each write instead of waiting for the buffer
 * to fill, which is how stderr keeps its promised ordering.  "length"
 * is the number of bytes currently staged in "buffer".
 */
struct yupiter_FILE
{
    int    fd;
    int    error;
    int    unbuffered;
    size_t length;
    char   buffer[YUPITER_BUFFER_SIZE];
};

/*
 * Operating-system layer (io/syscall_linux.cxx).
 *
 * Each wrapper returns 0 on success and -1 on failure, mirroring the
 * convention documented for the public functions.  The wrappers are
 * the only place that knows the system-call numbers and the argument
 * convention of the target, so the formatting engine stays portable.
 *
 * yupiter_sys_write repeats the system call until the kernel has taken
 * every byte, so a short write - which a non-blocking descriptor, or a
 * terminal whose output was stopped and interrupted, produces - is
 * never reported as a success that lost part of the buffer.
 *
 * yupiter_sys_read and yupiter_sys_close belong to the platform layer of
 * the input release that yupiter_stdin is waiting for; nothing in the
 * library calls them yet.  They are kept next to the write wrapper so
 * that the whole operating-system surface stays in one file.
 */
int yupiter_sys_write(int fd, const void *p_buffer, size_t length);
int yupiter_sys_read(int fd, void *p_buffer, size_t length);
int yupiter_sys_close(int fd);

/* Terminates the process; never returns.  "status" is the exit code. */
void yupiter_sys_exit(int status) __attribute__((noreturn));

/*
 * Generic system-call trampoline (assembly/yupiter_syscall_aarch64.S and
 * assembly/yupiter_syscall_x86_64.S, one per architecture).
 *
 * Arguments are passed following the ordinary C calling convention of
 * the target; the trampoline rearranges them into the system-call
 * convention of the kernel.  The raw kernel return value is returned
 * unmodified, so callers must interpret negative values in the
 * "-4095 .. -1" range as errors.
 *
 * yupiter_syscall3 is the entry point the library uses for the calls
 * that take at most three arguments - read, write, close and exit.  It
 * performs the same translation as yupiter_syscall for those arguments,
 * without moving the three registers the kernel would ignore.  The
 * six-argument form is retained for a future call that needs them.
 */
long yupiter_syscall(long number, long arg1, long arg2, long arg3,
                   long arg4, long arg5, long arg6);

long yupiter_syscall3(long number, long arg1, long arg2, long arg3);

/*
 * Stream helpers (io/file.cxx).
 *
 * yupiter_stream_write writes "length" bytes to the stream.  Small
 * writes to a buffered stream are staged and flushed when the buffer
 * fills, when a newline is staged, or (for an unbuffered stream) when
 * the call finishes.  With nothing staged, an unbuffered stream or a
 * payload larger than YUPITER_BUFFER_SIZE goes directly to
 * yupiter_sys_write, so it is not copied through the staging buffer.
 * The function returns 0 on success and -1 on failure, and records the
 * failure in the stream.
 *
 * yupiter_stream_flush pushes the staged bytes to the operating system
 * and clears the buffer.  It succeeds when nothing is staged.
 *
 * yupiter_stream_discard throws away the staged bytes without writing
 * them, which is what a failed call has to do so that half of a
 * conversion never reaches the output.  Bytes already written through
 * the direct path have reached the operating system and are not
 * affected.
 */
int yupiter_stream_write(yupiter_FILE *stream, const void *p_buffer,
                         size_t length);
int yupiter_stream_flush(yupiter_FILE *stream);
void yupiter_stream_discard(yupiter_FILE *stream);

/*
 * Formatting engine (io/vfprintf.cxx).
 *
 * Returns the number of characters written, or -1 on failure.
 */
int yupiter_vfprintf(yupiter_FILE *stream, const char *format, va_list args);

/*
 * Internal string helpers.
 *
 * These exist so that the library links with "-nostdlib" without
 * pulling symbols out of a host C library.  They are deliberately
 * prefixed and are not exported as public API.
 *
 * yupiter_strlen returns 0 for a null pointer.  The formatting engine
 * checks a string argument before it calls the helper, so the null
 * case is not needed there; it is specified anyway so that every
 * caller sees the same total function instead of a helper whose
 * behaviour depends on its caller's checks.  yupiter_memcpy and
 * yupiter_memset return "p_destination" unchanged when either pointer is
 * null or when the length is zero, rather than failing, which keeps
 * the zero-length cases of the formatting paths branch-free at the
 * call site.
 */
size_t yupiter_strlen(const char *p_string);
void  *yupiter_memcpy(void *p_destination, const void *p_source, size_t length);
void  *yupiter_memset(void *p_destination, int value, size_t length);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* YUPITER_IO_H */
