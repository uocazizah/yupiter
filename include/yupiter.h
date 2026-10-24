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

#ifndef YUPITER_H
#define YUPITER_H

/*
 * Public interface of yupiter.
 *
 * The header is written in ISO C17 and is deliberately usable in
 * freestanding builds: it depends only on "<stdarg.h>" and
 * "<stddef.h>", which are provided by the compiler rather than by any
 * C library.
 */

#include <stdarg.h>
#include <stddef.h>

/*
 * Version of the library.
 *
 * The same values are assembled into the package version of the
 * pkg-config file and the shared-object version of the build; they are
 * part of the interface so that a consumer can check at compile time
 * which release it was built against.
 */
#define YUPITER_VERSION_MAJOR 1
#define YUPITER_VERSION_MINOR 0
#define YUPITER_VERSION_PATCH 2
#define YUPITER_VERSION_STRING "1.0.2"

/*
 * Symbol visibility.
 *
 * The library is built with "-fvisibility=hidden", so every symbol is
 * local to the shared object unless this header marks it as exported.
 * The attribute is a no-op for a static archive and for a consumer that
 * includes the header, and it expands to nothing on a compiler that
 * does not understand it, so the header still compiles as ISO C17.
 */
#if defined(__GNUC__) || defined(__clang__)
#define YUPITER_PUBLIC __attribute__((visibility("default")))
#else
#define YUPITER_PUBLIC
#endif

/*
 * Compile-time format checking.
 *
 * When the compiler understands the GNU attribute, yupiter_printf and
 * yupiter_fprintf are checked against the arguments they are given, so a
 * mismatch is reported while the program is built instead of when it
 * runs.  The attribute is the only compiler extension in this header:
 * it changes no declaration and no calling convention, it is provided
 * by both GNU C and Clang, and it expands to nothing anywhere else, so
 * the header still compiles as ISO C17.
 *
 * The check is stricter than this interface in one place.  "%+p" and
 * "% p" are accepted by the functions, which follow the reference C
 * library and print "+0x1234" and " 0x1234", but "-Wformat" reports
 * the '+' and ' ' flags on a pointer conversion.  A caller that wants
 * that output can quiet the one diagnostic around the one call that
 * uses it.
 */
#if defined(__GNUC__) || defined(__clang__)
#define YUPITER_PRINTF_FORMAT(_format_position, _first_argument) \
    __attribute__((format(printf, _format_position, _first_argument)))
#else
#define YUPITER_PRINTF_FORMAT(_format_position, _first_argument)
#endif

/*
 * End-of-file and error indicator returned by the input functions.
 *
 * yupiter_getchar and yupiter_fgetc return this value on end of file or
 * on a read error; yupiter_feof and yupiter_ferror distinguish the two.
 */
#define YUPITER_EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque output stream.
 *
 * The layout is an implementation detail and must not be relied upon.
 * A stream is obtained from yupiter (yupiter_stdout, yupiter_stderr and
 * yupiter_stdin are the only streams provided in this release) and is
 * passed to the functions below.  Passing a null stream pointer, or a
 * pointer to an object that is not a yupiter stream, is undefined
 * behaviour.
 */
typedef struct yupiter_FILE yupiter_FILE;

/*
 * Standard streams.
 *
 * yupiter_stdout is buffered; the buffer is flushed when a newline is
 * written and at the end of every completed call.  yupiter_stderr is
 * unbuffered.  yupiter_stdin is the input stream used by the input
 * functions declared below.
 *
 * The objects are constant pointers: the stream they point to stays
 * valid for the whole program lifetime, but the stream itself is not
 * modified through the pointer.
 */
extern YUPITER_PUBLIC yupiter_FILE *const yupiter_stdout;
extern YUPITER_PUBLIC yupiter_FILE *const yupiter_stderr;
extern YUPITER_PUBLIC yupiter_FILE *const yupiter_stdin;

/*
 * Write the formatted string described by "format" to yupiter_stdout.
 *
 * "format" must not be null.  The conversions supported in this
 * release are:
 *
 *   %d %i          signed decimal int
 *   %u             unsigned decimal int
 *   %o             unsigned octal int
 *   %x %X          unsigned hexadecimal int (lower/upper case)
 *   %c             int converted to unsigned char
 *   %s             null-terminated string
 *   %p             pointer (a null pointer prints as "(nil)")
 *   %%             a literal percent sign
 *
 * with the flags '-', '+', ' ' (space), '#' and '0'; an all-numeric
 * or '*' width; a '.' precision (numeric or '*'); and the length
 * modifiers 'hh', 'h', 'l', 'll', 'z', 't' and 'j'.
 *
 * The '+' and ' ' flags apply to the signed conversions and to %p.
 * For a non-null pointer they are printed in front of the "0x" prefix
 * - "%+p" prints "+0x1234" and "% p" prints " 0x1234" - which is what
 * the reference C library does.  A null pointer prints as "(nil)",
 * with no sign.  For the unsigned conversions the two flags are
 * ignored.
 *
 * A width given as a negative '*' argument is treated as the '-'
 * flag with a positive width.  A precision given as a negative '*'
 * argument is treated as if no precision were given.  A null "s"
 * conversion argument prints as "(null)", or as nothing at all when
 * the precision is narrower than that word.
 *
 * A width or precision written as a literal that does not fit in an
 * "int" makes the whole call fail, as does a field that would not fit
 * in the return value.
 *
 * Returns the number of characters written, or -1 if the stream
 * could not be written.  On failure the stream keeps a sticky error
 * and later calls fail immediately.
 *
 * Each call borrows one small scratch block from the yupiter heap
 * for the duration of the call and releases it before returning, so
 * a completed call leaves nothing allocated.  When the heap is
 * exhausted the call falls back to an internal stack buffer instead,
 * so running out of heap never makes the formatted functions fail.
 */
YUPITER_PUBLIC int yupiter_printf(const char *format, ...)
    YUPITER_PRINTF_FORMAT(1, 2);

/*
 * Same as yupiter_printf, but writes to "stream" instead of
 * yupiter_stdout.  See yupiter_printf for the supported conversions and
 * for the meaning of the return value.
 */
YUPITER_PUBLIC int yupiter_fprintf(yupiter_FILE *stream, const char *format,
                                   ...) YUPITER_PRINTF_FORMAT(2, 3);

/*
 * Input.
 *
 * yupiter_getchar reads one character from yupiter_stdin.  yupiter_fgetc
 * reads one character from "stream".  Both return the byte read as an
 * unsigned char converted to int, or YUPITER_EOF on end of file or on a
 * read error; use yupiter_feof and yupiter_ferror to tell the two apart.
 *
 * yupiter_fgets reads at most "n - 1" characters from "stream" into "s",
 * stopping after a newline, and writes a terminating null byte.  It
 * returns "s" on success and null on end of file or error with no
 * characters read.
 *
 * yupiter_fread reads up to "nmemb" elements of "size" bytes from
 * "stream" into "ptr" and returns the number of full elements read.
 * A "size" or "nmemb" of 0 reads nothing and returns 0.
 *
 * yupiter_ungetc pushes "c" back onto "stream" so that the next read
 * returns it.  Only one byte of pushback is guaranteed; pushing a second
 * byte before the first is consumed fails.  Returns "c" on success and
 * YUPITER_EOF on failure.
 *
 * yupiter_feof reports whether a previous read reached end of file.
 * yupiter_ferror reports whether a previous read failed.  yupiter_clearerr
 * clears both flags.  yupiter_fflush pushes the staged bytes of a
 * buffered output stream to the operating system.
 */
YUPITER_PUBLIC int yupiter_getchar(void);
YUPITER_PUBLIC int yupiter_fgetc(yupiter_FILE *stream);
YUPITER_PUBLIC char *yupiter_fgets(char *s, int n, yupiter_FILE *stream);
YUPITER_PUBLIC size_t yupiter_fread(void *ptr, size_t size, size_t nmemb,
                                    yupiter_FILE *stream);
YUPITER_PUBLIC int yupiter_ungetc(int c, yupiter_FILE *stream);
YUPITER_PUBLIC int yupiter_feof(yupiter_FILE *stream);
YUPITER_PUBLIC int yupiter_ferror(yupiter_FILE *stream);
YUPITER_PUBLIC void yupiter_clearerr(yupiter_FILE *stream);
YUPITER_PUBLIC int yupiter_fflush(yupiter_FILE *stream);

/*
 * Dynamic memory.
 *
 * yupiter allocates from a private static arena whose size is fixed
 * when the library is compiled; the heap never grows through the
 * operating system.  A build may set the arena size with
 * "./configure --heap-size=<bytes>" or, when the library is compiled
 * by other means, with "-DYUPITER_HEAP_SIZE=<bytes>".  The default is
 * 4096 bytes, which also bounds the worst case leak: memory that is
 * never released stays inside that arena and is reported by the
 * library's own accounting rather than accumulating unnoticed.
 *
 * The heap is not thread-safe: yupiter creates no threads and
 * provides no synchronisation.
 */

/*
 * Allocate "size" bytes and return a pointer suitably aligned for any
 * object of that size, or null when no block of that size is free.
 * The memory is not initialised.
 *
 * A "size" of 0 allocates nothing and returns a null pointer, which
 * is not a failure.  Returns null on failure.
 */
YUPITER_PUBLIC void *yupiter_malloc(size_t size);

/*
 * Release a block obtained from yupiter_malloc, yupiter_malloc_safe,
 * yupiter_realloc or yupiter_realloc_safe.
 *
 * A null pointer releases nothing.  Anything else that is not the
 * start of a live block - a pointer from outside the heap, a pointer
 * into the middle of a block, a block that is already free - is
 * ignored without modifying the heap.  Use yupiter_free_safe when such
 * a pointer must be reported instead.
 */
YUPITER_PUBLIC void yupiter_free(void *p);

/*
 * Same as yupiter_malloc, but with the checks a caller should not have
 * to repeat:
 *
 *   - "count * size" is computed with an overflow check, so a request
 *     whose size wraps around is rejected instead of being truncated;
 *   - the memory is filled with zero bytes before it is returned.
 *
 * A failure to satisfy the request - an overflowing size or an
 * exhausted heap - writes a diagnostic to yupiter_stderr and terminates
 * the process with status 134, so this function never returns a null
 * pointer for a request it could not meet.  The only null result is
 * the documented "nothing was requested" case ("count" or "size" is
 * 0), which allocates nothing and is not an error.
 */
YUPITER_PUBLIC void *yupiter_malloc_safe(size_t count, size_t size);

/*
 * Same as yupiter_free, but a pointer that is not the start of a live
 * block is reported: the function writes a diagnostic naming the
 * problem (outside the heap, not a block start, double free or a
 * corrupted header) to yupiter_stderr and terminates the process with
 * status 134 instead of ignoring it.  A null pointer still releases
 * nothing and is not an error.
 */
YUPITER_PUBLIC void yupiter_free_safe(void *p);

/*
 * Resize the block that "p" points at so that it holds "size" bytes.
 *
 * "p" must be null or the start of a live block obtained from
 * yupiter_malloc, yupiter_malloc_safe, yupiter_realloc or
 * yupiter_realloc_safe.  A null pointer behaves like
 * yupiter_malloc(size); otherwise the block keeps its contents up to
 * the smaller of the old and new sizes.
 *
 * A block grows in place when the free space after it is large
 * enough, and shrinks in place when the remainder can become a free
 * block; only otherwise is it copied into a fresh block and the old
 * one released.  Bytes added by a growing resize are not initialised.
 *
 * Returns the resized block, or null when the request cannot be met.
 * A null result leaves the original block unchanged and still live.
 * A "size" of 0 releases the block and returns null, as C17 permits;
 * because null is also the failure result, the caller has to look at
 * "size" to tell the two cases apart.
 */
YUPITER_PUBLIC void *yupiter_realloc(void *p, size_t size);

/*
 * Same as yupiter_realloc, but with the checks a caller should not have
 * to repeat:
 *
 *   - "count * size" is computed with an overflow check, so a size
 *     that wraps around is rejected instead of being truncated;
 *   - an invalid pointer, an overflowing size and an exhausted heap
 *     all write a diagnostic to yupiter_stderr and terminate the
 *     process with status 134 instead of returning null.
 *
 * A null "p" allocates like yupiter_malloc_safe, including its zero
 * fill.  Growing an existing block does not initialise the bytes it
 * adds; initialise them before use.  A "count" or "size" of 0 releases
 * "p" (a null pointer releases nothing) and returns null, which is not
 * an error.
 */
YUPITER_PUBLIC void *yupiter_realloc_safe(void *p, size_t count, size_t size);

/*
 * Non-terminating checked operations for hosted callers such as language
 * bindings.  They report a failure through the return value instead of
 * terminating the process; the existing safe functions keep their fatal
 * failure policy.
 *
 * The status values are:
 *
 *   YUPITER_MEMORY_OK               the operation succeeded
 *   YUPITER_MEMORY_INVALID_POINTER  not the start of a live heap block
 *   YUPITER_MEMORY_OUT_OF_MEMORY    no free block in the arena fits
 *   YUPITER_MEMORY_OVERFLOW         the requested size cannot be
 *                                   represented
 */
typedef enum yupiter_memory_status
{
    YUPITER_MEMORY_OK = 0,
    YUPITER_MEMORY_INVALID_POINTER = 1,
    YUPITER_MEMORY_OUT_OF_MEMORY = 2,
    YUPITER_MEMORY_OVERFLOW = 3
} yupiter_memory_status;

/*
 * Release the block that "p" points at and report the outcome.
 *
 * A null pointer succeeds and releases nothing.  The validation is the
 * same as yupiter_free_safe, but a pointer that is not the start of a
 * live block is reported as YUPITER_MEMORY_INVALID_POINTER and the heap
 * is left unchanged; the process is never terminated.
 *
 * Returns YUPITER_MEMORY_OK or YUPITER_MEMORY_INVALID_POINTER.
 */
YUPITER_PUBLIC yupiter_memory_status yupiter_free_checked(void *p);

/*
 * Resize the block that "p" points at to "size" bytes and report the
 * outcome.
 *
 * The resize and zero-size rules are those of yupiter_realloc: a null
 * pointer allocates, and a size of 0 releases the block.  The new
 * pointer is stored through "result", which must point to writable
 * pointer storage that does not overlap the heap.  On failure *result
 * is set to null and the original block remains live and unchanged.
 *
 * Returns YUPITER_MEMORY_OK, YUPITER_MEMORY_INVALID_POINTER,
 * YUPITER_MEMORY_OUT_OF_MEMORY or YUPITER_MEMORY_OVERFLOW.  No failure
 * terminates the process.
 */
YUPITER_PUBLIC yupiter_memory_status
yupiter_realloc_checked(void *p, size_t size, void **result);

/*
 * The format-checking macro is an internal detail of the two
 * declarations above; it is undefined here so that the public
 * namespace keeps only the documented names.
 */
#undef YUPITER_PRINTF_FORMAT

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* YUPITER_H */
