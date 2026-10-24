/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - heap and leak test
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
 * The heap, exercised with small allocations only.
 *
 * The arena is 4096 bytes by default, so anything this test leaks
 * stays a few kilobytes at most, and the accounting in yupiter_mem.hxx
 * turns any leak into a failing check: after every scenario the live
 * byte and block counts must be back at zero.  The exhaustion scenario
 * deliberately fills the arena to prove that a full heap behaves -
 * yupiter_malloc reports it, printf falls back to its stack buffer, and
 * releasing everything restores the whole arena.
 *
 * Each check adds to a counter that becomes the exit status, so the
 * test carries its own verdict and needs no golden file.
 */

#include <stddef.h>
#include <stdint.h>

#include "yupiter_io.hxx"
#include "yupiter_mem.hxx"

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
 * A successful allocation is aligned, counted and released cleanly;
 * a request for nothing is not a request at all.
 */
static void test_allocation_and_release(void)
{
    void  *p_zero;
    void  *p_small;

    p_zero = yupiter_malloc(0);
    check(p_zero == NULL, "malloc of zero bytes returns NULL");

    p_small = yupiter_malloc(1);
    check(p_small != NULL, "malloc of one byte returns a pointer");
    check(yupiter_heap_live_blocks() == 1, "a live block is counted");
    check(yupiter_heap_live_bytes() > 0, "live bytes are counted");

    if (p_small != NULL)
    {
        check((((uintptr_t)p_small) % _Alignof(max_align_t)) == 0,
              "a payload is aligned for any object");
    }

    yupiter_free(p_small);
    check(yupiter_heap_live_blocks() == 0, "release drops the block count");
    check(yupiter_heap_live_bytes() == 0, "release drops the byte count");
}

/*
 * Blocks that are allocated together keep their own contents.
 *
 * How many blocks fit in the arena is the allocator's own business -
 * the test does not know how large a block header is - so instead of
 * computing the answer it asks the heap: it takes as many blocks as it
 * can and keeps going with at least two of them, which is the least a
 * scenario that compares neighbours needs.  The payload is halved when
 * the arena is too small to hold two blocks of that size.
 */
static size_t take_blocks(unsigned char *p_blocks[], size_t wanted,
                          size_t payload)
{
    size_t count;

    for (count = 0; count < wanted; count = count + 1)
    {
        p_blocks[count] = (unsigned char *)yupiter_malloc(payload);

        if (p_blocks[count] == NULL)
        {
            break;
        }
    }

    return count;
}

static void release_blocks(unsigned char *p_blocks[], size_t count)
{
    size_t index;

    for (index = 0; index < count; index = index + 1)
    {
        yupiter_free(p_blocks[index]);
        p_blocks[index] = NULL;
    }
}

static void test_blocks_do_not_overlap(void)
{
    unsigned char *p_blocks[4];
    size_t         payload;
    size_t         count;
    size_t         index;
    size_t         byte;
    int            intact;

    payload = 32;

    for (;;)
    {
        count = take_blocks(p_blocks, 4, payload);

        if (count >= 2)
        {
            break;
        }

        release_blocks(p_blocks, count);

        if (payload <= 16)
        {
            check(0, "the heap serves at least two blocks");
            return;
        }

        payload = payload / 2;
    }

    check(count > 0, "the heap serves allocations");

    for (index = 0; index < count; index = index + 1)
    {
        (void)yupiter_memset(p_blocks[index], (int)(0xA0u + index), payload);
    }

    intact = 1;
    for (index = 0; index < count; index = index + 1)
    {
        for (byte = 0; byte < payload; byte = byte + 1)
        {
            if (p_blocks[index][byte] != (unsigned char)(0xA0u + index))
            {
                intact = 0;
            }
        }
    }

    check(intact != 0, "neighbouring blocks do not overlap");

    release_blocks(p_blocks, count);

    check(yupiter_heap_live_bytes() == 0, "every block was released");
}

/*
 * Pointers that are not the start of a live block are ignored by
 * yupiter_free: the heap is left exactly as it was, including when the
 * same block is released twice.
 */
static void test_invalid_free_is_ignored(void)
{
    void  *p_block;
    size_t before;

    p_block = yupiter_malloc(16);
    check(p_block != NULL, "a block for the invalid frees is available");

    if (p_block == NULL)
    {
        return;
    }

    before = yupiter_heap_live_bytes();

    yupiter_free((char *)p_block + 1);
    yupiter_free((char *)p_block + 16);
    yupiter_free((void *)(uintptr_t)0x1234);

    check(yupiter_heap_live_bytes() == before,
          "an invalid free leaves the heap unchanged");

    yupiter_free(p_block);
    check(yupiter_heap_live_bytes() == 0, "the valid free still works");

    yupiter_free(p_block);
    check(yupiter_heap_live_bytes() == 0, "a double free changes nothing");
    yupiter_free(NULL);
    check(yupiter_heap_live_bytes() == 0, "freeing NULL changes nothing");
}

/*
 * Fill the arena, prove printf still works without it, then release
 * everything: the counters return to zero and one block covering
 * three quarters of the arena proves the free blocks were merged
 * again rather than left as fragments.
 */
static void test_exhaustion_and_release(void)
{
    void  *p_head;
    void  *p_block;
    void  *p_large;
    size_t payload;
    size_t count;
    int    written;

    /*
     * The payload is derived from the arena - a sixteenth of it, and
     * never smaller than the smallest request worth making - so that
     * the scenario fills whichever heap the library was built with
     * instead of assuming room for 64 byte blocks.  The previous head
     * is remembered in the payload itself, so the list of blocks costs
     * no storage of its own and works for any arena size.
     */
    payload = yupiter_heap_capacity() / 16u;

    if (payload < 16u)
    {
        payload = 16u;
    }

    p_head = NULL;
    count = 0;

    for (;;)
    {
        p_block = yupiter_malloc(payload);

        if (p_block == NULL)
        {
            break;
        }

        *(void **)p_block = p_head;
        p_head = p_block;
        count = count + 1;
    }

    check(count > 0, "the heap serves allocations");
    check(yupiter_malloc(payload) == NULL, "an exhausted heap reports NULL");

    written = yupiter_printf("heap exhausted, falling back: %d\n", 42);
    check(written > 0,
          "printf still works when the heap has nothing left");

    while (p_head != NULL)
    {
        p_block = p_head;
        p_head = *(void **)p_head;
        yupiter_free(p_block);
    }

    check(yupiter_heap_live_blocks() == 0, "every block was released");
    check(yupiter_heap_live_bytes() == 0, "every byte was released");

    p_large = yupiter_malloc(yupiter_heap_capacity() -
                             (yupiter_heap_capacity() / 4u));
    check(p_large != NULL, "free blocks are merged back into one");
    yupiter_free(p_large);
    check(yupiter_heap_live_bytes() == 0, "the merged block was released");
}

/*
 * The formatted functions borrow scratch memory and must give it
 * back, whatever they are asked to print.
 */
static void test_printf_releases_its_scratch(void)
{
    int written;

    written = 0;
    written = written + yupiter_printf("[%d][%i][%u]\n", -42, 42, 42u);
    written = written +
              yupiter_printf("[%x][%X][%o][%c]\n", 255u, 255u, 8u, 'Z');
    written = written +
              yupiter_printf("[%10.4d][%-8u][%s]\n", 1234, 99u, "text");
    written = written + yupiter_printf("[%20s][%.3s][%%]\n", "wide", "abcdef");
    written = written +
              yupiter_printf("[%20d][%-20d][%08d][%.4d]\n", 7, 7, 7, 7);
    written = written +
              yupiter_printf("[%p][%s]\n", (void *)0, (const char *)NULL);

    check(written > 0, "the formatted calls produced output");
    check(yupiter_heap_live_blocks() == 0,
          "printf left no block behind");
    check(yupiter_heap_live_bytes() == 0,
          "printf left no byte behind");
}

/*
 * The safe entry point: checked size arithmetic, zero-filled memory
 * and a release that reports nothing because the pointer is valid.
 */
static void test_safe_allocation(void)
{
    unsigned char *p_block;
    size_t         count;
    size_t         total;
    size_t         index;
    int            all_zero;
    void          *p_nothing;

    /*
     * The request is sized from the arena so that it fits whichever
     * heap the library was built with: a quarter of the arena in
     * blocks of 16 bytes, capped at the 64 bytes this scenario has
     * always asked for.
     */
    total = yupiter_heap_capacity() / 4u;
    count = total / 16u;

    if (count == 0)
    {
        count = 1;
        total = 16u;
    }

    if (total > 64u)
    {
        total = 64u;
        count = 4u;
    }

    p_block = yupiter_malloc_safe(count, 16);
    check(p_block != NULL, "malloc_safe returns a pointer");
    check(yupiter_heap_live_bytes() == total,
          "malloc_safe reserves the whole request");

    all_zero = 1;
    if (p_block != NULL)
    {
        for (index = 0; index < total; index = index + 1)
        {
            if (p_block[index] != 0u)
            {
                all_zero = 0;
            }
        }
    }
    check(all_zero != 0, "malloc_safe returns zeroed memory");

    p_nothing = yupiter_malloc_safe(0, 16);
    check(p_nothing == NULL, "a request for nothing allocates nothing");

    yupiter_free_safe(p_block);
    check(yupiter_heap_live_bytes() == 0, "free_safe released the block");
    yupiter_free_safe(NULL);
    check(yupiter_heap_live_bytes() == 0, "free_safe ignores NULL");
}

/*
 * Fill a buffer with a recognisable pattern and compare it again.  The
 * seed differs between scenarios, so a block that returns bytes from
 * another block is visible rather than matching by accident.
 */
static void fill_bytes(unsigned char *p_bytes, size_t count,
                       unsigned char seed)
{
    size_t index;

    for (index = 0; index < count; index = index + 1)
    {
        p_bytes[index] = (unsigned char)(seed + (unsigned char)index);
    }
}

static int bytes_match(const unsigned char *p_bytes, size_t count,
                       unsigned char seed)
{
    size_t index;

    for (index = 0; index < count; index = index + 1)
    {
        if (p_bytes[index] != (unsigned char)(seed + (unsigned char)index))
        {
            return 0;
        }
    }

    return 1;
}

static int bytes_are_zero(const unsigned char *p_bytes, size_t count)
{
    size_t index;

    for (index = 0; index < count; index = index + 1)
    {
        if (p_bytes[index] != 0u)
        {
            return 0;
        }
    }

    return 1;
}

/*
 * Resizing a block: growth into the free tail, shrink out of it, a
 * move when the next block is in the way, the null and zero cases, an
 * invalid pointer, and the safe variant.
 *
 * The sizes come from the arena so the scenarios also run in the
 * smallest supported build.  A shrink or a move needs more room than
 * that build has, so those two parts are skipped there and the rest
 * still proves the contract.
 */
static void test_realloc(void)
{
    unsigned char *p_block;
    unsigned char *p_resized;
    unsigned char *p_first;
    unsigned char *p_second;
    size_t         capacity;
    size_t         alignment;
    size_t         large;
    size_t         before;

    capacity = yupiter_heap_capacity();
    alignment = _Alignof(max_align_t);
    large = ((capacity / 2u) / alignment) * alignment;

    if (large < (2u * alignment))
    {
        large = 2u * alignment;
    }

    p_block = (unsigned char *)yupiter_malloc(16);
    check(p_block != NULL, "realloc: the source block is available");

    if (p_block != NULL)
    {
        fill_bytes(p_block, 16, 1u);
        p_resized = (unsigned char *)yupiter_realloc(p_block, large);
        check(p_resized != NULL, "realloc: a block grows");

        if (p_resized != NULL)
        {
            check(p_resized == p_block, "realloc: growth stays in place");
            check(bytes_match(p_resized, 16, 1u),
                  "realloc: growth keeps the old contents");
            check((yupiter_heap_live_bytes() >= large) &&
                  (yupiter_heap_live_bytes() <= yupiter_heap_capacity()),
                  "realloc: growth is accounted");
            p_block = p_resized;
        }

        p_resized = (unsigned char *)yupiter_realloc(p_block, 0);
        check(p_resized == NULL, "realloc: a size of zero returns NULL");
        check(yupiter_heap_live_bytes() == 0,
              "realloc: a size of zero releases the block");
    }

    p_block = (unsigned char *)yupiter_realloc(NULL, 16);
    check(p_block != NULL, "realloc: a null pointer allocates");
    check(yupiter_heap_live_blocks() == 1,
          "realloc: the new block is counted");
    yupiter_free(p_block);
    check(yupiter_heap_live_bytes() == 0,
          "realloc: the new block is released");

    if (capacity >= 256u)
    {
        p_block = (unsigned char *)yupiter_malloc(64);
        check(p_block != NULL, "realloc: the shrink block is available");

        if (p_block != NULL)
        {
            fill_bytes(p_block, 64, 3u);
            p_resized = (unsigned char *)yupiter_realloc(p_block, 8);
            check(p_resized != NULL, "realloc: a block shrinks");

            if (p_resized != NULL)
            {
                check(p_resized == p_block, "realloc: shrink stays in place");
                check(bytes_match(p_resized, 8, 3u),
                      "realloc: shrink keeps the retained contents");
                check(yupiter_heap_live_bytes() == alignment,
                      "realloc: shrink releases the tail");
                p_block = p_resized;
            }

            yupiter_free(p_block);
            check(yupiter_heap_live_bytes() == 0,
                  "realloc: the shrunk block is released");
        }
    }

    if (capacity >= 512u)
    {
        p_first = (unsigned char *)yupiter_malloc(16);
        p_second = (unsigned char *)yupiter_malloc(16);
        check((p_first != NULL) && (p_second != NULL),
              "realloc: the move blocks are available");

        if ((p_first != NULL) && (p_second != NULL))
        {
            fill_bytes(p_first, 16, 5u);
            p_resized = (unsigned char *)yupiter_realloc(p_first,
                                                       capacity / 2u);
            check(p_resized != NULL, "realloc: an obstructed block moves");

            if (p_resized != NULL)
            {
                check(p_resized != p_first,
                      "realloc: the moved block has a new address");
                check(bytes_match(p_resized, 16, 5u),
                      "realloc: the move keeps the old contents");
                yupiter_free(p_resized);
            }

            yupiter_free(p_second);
            check(yupiter_heap_live_bytes() == 0,
                  "realloc: the move blocks are released");
        }
    }

    p_block = (unsigned char *)yupiter_malloc(16);
    check(p_block != NULL, "realloc: the invalid-pointer block is available");

    if (p_block != NULL)
    {
        before = yupiter_heap_live_bytes();
        check(yupiter_realloc(p_block + 1, 8) == NULL,
              "realloc: an interior pointer is refused");
        check(yupiter_heap_live_bytes() == before,
              "realloc: an invalid pointer leaves the heap unchanged");
        yupiter_free(p_block);
        check(yupiter_heap_live_bytes() == 0,
              "realloc: the invalid-pointer block is released");
    }

    /*
     * A failed move is a failure, not a lost block: fill the arena so
     * the new copy cannot be allocated, then ask for more than the
     * block can grow into.  The original block must still be live and
     * unchanged.
     */
    p_block = (unsigned char *)yupiter_malloc(16);
    check(p_block != NULL, "realloc: the exhausted-heap block is available");

    if (p_block != NULL)
    {
        void *p_head;
        void *p_fill;

        fill_bytes(p_block, 16, 9u);
        p_head = NULL;

        while ((p_fill = yupiter_malloc(16)) != NULL)
        {
            *(void **)p_fill = p_head;
            p_head = p_fill;
        }

        check(yupiter_realloc(p_block, capacity / 2u) == NULL,
              "realloc: an exhausted heap reports failure");
        check(bytes_match(p_block, 16, 9u),
              "realloc: a failed growth leaves the block unchanged");

        while (p_head != NULL)
        {
            p_fill = p_head;
            p_head = *(void **)p_head;
            yupiter_free(p_fill);
        }

        yupiter_free(p_block);
        check(yupiter_heap_live_bytes() == 0,
              "realloc: the exhausted-heap blocks are released");
    }

    p_block = (unsigned char *)yupiter_realloc_safe(NULL, 4, 4);
    check(p_block != NULL, "realloc_safe: a null pointer allocates");

    if (p_block != NULL)
    {
        check(bytes_are_zero(p_block, 16),
              "realloc_safe: a null pointer allocates zeroed memory");
        fill_bytes(p_block, 4, 7u);
        p_resized = (unsigned char *)yupiter_realloc_safe(p_block, 8, 4);
        check(p_resized != NULL, "realloc_safe: a block grows");

        if (p_resized != NULL)
        {
            check(bytes_match(p_resized, 4, 7u),
                  "realloc_safe: growth keeps the old contents");
            p_block = p_resized;
        }

        p_resized = (unsigned char *)yupiter_realloc_safe(p_block, 0, 1);
        check(p_resized == NULL, "realloc_safe: a zero size returns NULL");
        check(yupiter_heap_live_bytes() == 0,
              "realloc_safe: a zero size releases the block");
    }
}

/* The checked interface returns failures without losing the original block. */
static void test_checked_operations(void)
{
    unsigned char *p;
    void *result;
    int foreign;

    check(yupiter_free_checked(NULL) == YUPITER_MEMORY_OK,
          "checked free accepts null");
    check(yupiter_free_checked(&foreign) == YUPITER_MEMORY_INVALID_POINTER,
          "checked free rejects a foreign pointer");
    result = NULL;
    check(yupiter_realloc_checked(&foreign, 0, &result) ==
              YUPITER_MEMORY_INVALID_POINTER,
          "checked zero-size resize rejects a foreign pointer");
    check(result == NULL, "failed checked resize clears result");
    check(yupiter_realloc_checked(NULL, 16, NULL) ==
              YUPITER_MEMORY_INVALID_POINTER,
          "checked resize rejects missing result storage");
    result = &foreign;
    check(yupiter_realloc_checked(NULL, SIZE_MAX, &result) ==
              YUPITER_MEMORY_OVERFLOW,
          "checked allocation classifies alignment overflow");
    check(result == NULL, "overflowing checked allocation clears result");
    result = &foreign;
    check(yupiter_realloc_checked(NULL, yupiter_heap_capacity(), &result) ==
              YUPITER_MEMORY_OUT_OF_MEMORY,
          "checked allocation classifies exhaustion");
    check(result == NULL, "exhausted checked allocation clears result");
    check(yupiter_realloc_checked(NULL, 0, &result) == YUPITER_MEMORY_OK,
          "checked zero-size allocation succeeds");
    check(result == NULL, "checked zero-size allocation returns null");
    check(yupiter_realloc_checked(NULL, 16, &result) == YUPITER_MEMORY_OK,
          "checked resize allocates from null");
    p = (unsigned char *)result;
    if (p != NULL)
    {
        p[0] = 42;
        check(yupiter_free_checked(p + 1) == YUPITER_MEMORY_INVALID_POINTER,
              "checked free rejects an interior pointer");
        check(yupiter_realloc_checked(p, SIZE_MAX, &result) ==
                  YUPITER_MEMORY_OVERFLOW,
              "checked resize classifies alignment overflow");
        check(result == NULL && p[0] == 42,
              "overflow keeps the old allocation intact");
        check(yupiter_realloc_checked(p, yupiter_heap_capacity(), &result) ==
                  YUPITER_MEMORY_OUT_OF_MEMORY,
              "checked resize classifies exhaustion");
        check(result == NULL && p[0] == 42,
              "exhaustion keeps the old allocation intact");
        check(yupiter_realloc_checked(p, 0, &result) == YUPITER_MEMORY_OK,
              "checked zero-size resize releases a block");
        check(result == NULL, "checked zero-size resize returns null");
        check(yupiter_free_checked(p) == YUPITER_MEMORY_INVALID_POINTER,
              "checked free detects double free");
        check(yupiter_realloc_checked(p, 16, &result) ==
                  YUPITER_MEMORY_INVALID_POINTER,
              "checked resize detects a freed block");
    }
    check(yupiter_heap_live_bytes() == 0 && yupiter_heap_live_blocks() == 0,
          "checked operations leak nothing");
}

/* Run allocator scenarios and return the number of failed checks. */
int main(void)
{
    failures = 0;

    test_allocation_and_release();
    test_blocks_do_not_overlap();
    test_invalid_free_is_ignored();
    test_exhaustion_and_release();
    test_printf_releases_its_scratch();
    test_safe_allocation();
    test_realloc();
    test_checked_operations();

    if (failures == 0)
    {
        yupiter_fprintf(yupiter_stderr, "heap: all checks passed\n");
    }

    return failures;
}
