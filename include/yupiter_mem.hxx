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

#ifndef YUPITER_MEM_H
#define YUPITER_MEM_H

/*
 * Internal interface of the yupiter heap (mem/heap.cxx).
 *
 * Nothing declared here is part of the public API.  The public
 * allocation functions live in "<yupiter.h>"; this header only
 * carries the sizing knob and the accounting that the tests use to
 * prove that no allocation is left behind.
 *
 * The declarations are wrapped in an "extern "C"" specification so
 * that the C++23 implementation defines them with the same linkage the
 * declarations promise, and the header itself is kept ISO C17 so that
 * the C test programs can include it too: the heap tests use the
 * accounting below to prove that every allocation was released.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Size of the heap arena, in bytes.
 *
 * The arena is a static object, so the size is fixed when the library
 * is compiled: the heap never grows and a program can never leak more
 * than this many bytes into it.  The default is deliberately small -
 * the only library code that allocates is the formatting engine, which
 * needs less than a hundred bytes at a time - and a build may override
 * it with "./configure --heap-size=<bytes>" or, when the library is
 * compiled by other means, with "-DYUPITER_HEAP_SIZE=<bytes>".
 */
#ifndef YUPITER_HEAP_SIZE
#define YUPITER_HEAP_SIZE 4096
#endif

/*
 * Number of payload bytes currently reserved, and number of blocks
 * they are reserved in.  Both are zero once every allocation has been
 * released, which is exactly what the tests assert after exercising
 * the library.
 */
size_t yupiter_heap_live_bytes(void);
size_t yupiter_heap_live_blocks(void);

/*
 * Total size of the arena, in bytes.  A test uses it to ask for an
 * allocation that only the whole arena can serve, which is how the
 * merging of free blocks is proven after everything was released.
 */
size_t yupiter_heap_capacity(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* YUPITER_MEM_H */
