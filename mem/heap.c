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
 * The yupiter dynamic memory allocator.
 *
 * The heap is a static arena of YUPITER_HEAP_SIZE bytes.  It needs no
 * system call, no operating system and no C library, which is what
 * lets this allocator live in a freestanding build, and it makes the
 * worst case leak bounded: a program that loses every pointer it owns
 * can lose at most YUPITER_HEAP_SIZE bytes, and the accounting below
 * still reports them.
 *
 * Layout.  The arena is one contiguous run of blocks.  Every block
 * starts with a header - a payload size, a state and a magic value -
 * followed by its payload.  The payload size is always a multiple of
 * the fundamental alignment, and the header is rounded up to that
 * alignment too, so a payload returned by yupiter_malloc is suitably
 * aligned for any object.  Allocation scans the run for the first
 * free block that fits and splits it when the leftover is large
 * enough to hold another block; release marks the block free and
 * merges it with its free neighbours, so the arena returns to a
 * single block after everything is released.
 *
 * Invariant.  No two adjacent blocks are free.  Every operation that
 * changes the list keeps that true: a split leaves the tail free next
 * to a block that was in use, and a release absorbs the free blocks on
 * both sides of the one being released.  A release therefore has at
 * most one free neighbour on each side and can merge in the same pass
 * that found the block, instead of walking the arena a second time.
 *
 * Validation.  yupiter_free and yupiter_free_safe classify a pointer
 * before touching anything: outside the arena or misaligned, not the
 * start of a block, already free (a double free), or a block whose
 * header does not add up.  The walk that confirms "start of a block"
 * is what rejects an interior pointer: only offsets the block list
 * actually steps over are accepted.
 *
 * Strict aliasing.  The arena is a declared "unsigned char" array, so
 * inside this file it is only ever read and written through byte
 * lvalues; the block headers are moved in and out with yupiter_memcpy
 * rather than by casting the storage to a structure pointer.  What a
 * caller does with the payload it receives is the caller's own
 * effective type, exactly as with any other allocator.
 *
 * The public entry points carry the C linkage the headers declare, so
 * a C program links them under their unmangled names.
 *
 * Not thread-safe: yupiter creates no threads and provides no
 * synchronisation.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "yupiter_io.h"
#include "yupiter_mem.h"



/* Size of the arena, as a value the compiler can use in a type. */
#define yupiter_heap_size ((size_t)(YUPITER_HEAP_SIZE))

/*
 * Payloads and headers are multiples of this alignment, which is the
 * strictest one the C library of the target can require.
 */
#define yupiter_heap_alignment ((size_t)_Alignof(max_align_t))

/*
 * The arena size arrives as a preprocessor constant.  Reject a value
 * that is not a positive number of bytes, or that does not fit in a
 * size_t on the target, here rather than letting a truncated
 * value reach the allocator.  The CMake guard rejects the same values
 * when the project is configured; these assertions are what protects a
 * build that bypasses CMake.
 */
_Static_assert(YUPITER_HEAP_SIZE > 0,
              "YUPITER_HEAP_SIZE must be a positive number of bytes");
_Static_assert((uintmax_t)(YUPITER_HEAP_SIZE) <=
                  (uintmax_t)(SIZE_MAX),
              "YUPITER_HEAP_SIZE does not fit in a size_t on this target");

/*
 * Magic marking bytes as a header this allocator wrote, and the state
 * that tells a live block from a free one, which is how a double free
 * is told apart from a pointer that was never allocated.
 */
#define yupiter_heap_magic ((uint32_t)0x59555049U) /* "YUPI" */

typedef enum block_state
{
    BLOCK_STATE_FREE = 0U,
    BLOCK_STATE_USED = 1U
} block_state;

/*
 * One block header.
 *
 * "payload" is the number of usable bytes after the header, always a
 * multiple of yupiter_heap_alignment.  sizeof of this structure is
 * rounded up to that alignment before it is used as an offset, so the
 * field layout may change without breaking the alignment of a
 * payload.  The state is a fixed-width enumeration for the same
 * reason: the header is written into raw arena bytes and has to mean
 * the same thing on every target.
 */
typedef struct yupiter_heap_block
{
    size_t  payload;
    block_state  state;
    uint32_t magic;
} yupiter_heap_block;

/*
 * The header size actually used in the arena: sizeof the structure
 * rounded up to the fundamental alignment, so the payload that follows
 * it is aligned for any object.  Computing it here makes the minimum
 * arena size a compile-time check instead of arithmetic that runs
 * before the first allocation.
 */
#define yupiter_heap_header ((((sizeof(yupiter_heap_block) + (yupiter_heap_alignment - 1U)) / yupiter_heap_alignment) * yupiter_heap_alignment))

_Static_assert((yupiter_heap_header % yupiter_heap_alignment) == 0,
              "the heap header must keep payloads aligned");
_Static_assert(yupiter_heap_header >= sizeof(yupiter_heap_block),
              "the rounded heap header cannot be smaller than its fields");
_Static_assert(
    yupiter_heap_size >= (yupiter_heap_header + yupiter_heap_alignment),
    "YUPITER_HEAP_SIZE must hold one block header and one aligned "
    "payload unit");

/*
 * Exit status of the fail-fast paths of the safe functions.  134 is
 * the conventional status of an aborted process (128 + SIGABRT), so a
 * caller that watches the status of a child sees a familiar number.
 */
#define yupiter_heap_fatal_status 134

/*
 * Classification of a pointer handed to yupiter_heap_release.
 */
typedef enum release_result
{
    RELEASE_RESULT_ACCEPT,
    RELEASE_RESULT_FOREIGN,
    RELEASE_RESULT_INTERIOR,
    RELEASE_RESULT_DOUBLE_FREE,
    RELEASE_RESULT_CORRUPT
} release_result;

/*
 * Result of a resize.  The first five values have the same meaning as
 * in a release, which is what lets a failed resize report itself the
 * same way; the last two are specific to growing a block.
 */
typedef enum resize_status
{
    RESIZE_STATUS_ACCEPT,
    RESIZE_STATUS_FOREIGN,
    RESIZE_STATUS_INTERIOR,
    RESIZE_STATUS_DOUBLE_FREE,
    RESIZE_STATUS_CORRUPT,
    RESIZE_STATUS_OUT_OF_MEMORY,
    RESIZE_STATUS_OVERFLOW
} resize_status;

/*
 * Result of the "previous block" lookup when the block being looked
 * for is the first one in the arena.  A real block start is always
 * smaller than the arena, so this value cannot be mistaken for one.
 */
#define yupiter_heap_no_previous ((size_t)(-1))

/*
 * The arena itself.  The alignment specifier makes the first payload
 * - which starts one rounded-up header into the arena - suitably
 * aligned for any object.  "constinit" holds it to the other
 * requirement of the freestanding startup path: the arena is
 * zero-initialized while the program is loaded, so nothing has to run
 * before the heap is usable, and an initializer that would change
 * that is rejected while the library is compiled instead of failing
 * silently at run time.
 */
_Alignas(max_align_t) unsigned char
    yupiter_heap_arena[yupiter_heap_size];

bool        yupiter_heap_ready;
size_t yupiter_heap_used_bytes;
size_t yupiter_heap_used_blocks;

/*
 * Round "size" up to a multiple of the alignment.  Returns 0 on
 * success and -1 when the rounded value would wrap around, which is
 * the overflow case a huge request hits.
 */
static int yupiter_heap_align_size(size_t size,
                                   size_t *p_aligned)
{
    size_t pad;

    pad = size % yupiter_heap_alignment;

    if (pad == 0)
    {
        *p_aligned = size;
        return 0;
    }

    pad = yupiter_heap_alignment - pad;

    if (size > (SIZE_MAX - pad))
    {
        return -1;
    }

    *p_aligned = size + pad;
    return 0;
}



/*
 * Move one header between the arena and a local structure.  See the
 * strict-aliasing note at the top of this file.
 */
static void yupiter_heap_block_load(size_t offset, yupiter_heap_block *p_block)
{
    (void)(
        yupiter_memcpy(p_block, &yupiter_heap_arena[offset], sizeof *p_block));
}

static void yupiter_heap_block_store(size_t offset,
                            const yupiter_heap_block *p_block)
{
    (void)(
        yupiter_memcpy(&yupiter_heap_arena[offset], p_block, sizeof *p_block));
}

/*
 * Turn the arena into one free block the first time the heap is used.
 */
static void yupiter_heap_initialise(void)
{
    yupiter_heap_block first;

    if (yupiter_heap_ready)
    {
        return;
    }

    first.payload = yupiter_heap_size - yupiter_heap_header;
    first.state = BLOCK_STATE_FREE;
    first.magic = yupiter_heap_magic;

    yupiter_heap_block_store(0, &first);

    yupiter_heap_ready = true;
}

/*
 * Find the block that starts at "offset" on the current list.
 *
 * Returns 0 and stores the block in "*p_block" when it is there, and
 * -1 otherwise, which includes a list whose size fields do not add up
 * - the walk stops instead of reading past the arena.
 *
 * The start of the block before the one found is reported in
 * "*p_previous", or yupiter_heap_no_previous when the block is the first
 * one.  Reporting it here is what lets a release merge with the block
 * before without walking the arena a second time; every block the walk
 * steps over has its size fields validated before they are trusted.
 */
static int yupiter_heap_find_block(size_t offset, yupiter_heap_block *p_block,
                          size_t *p_previous)
{
    size_t cursor;
    size_t previous;

    cursor = 0;
    previous = yupiter_heap_no_previous;

    while (cursor < yupiter_heap_size)
    {
        yupiter_heap_block block;

        if (cursor > (yupiter_heap_size - yupiter_heap_header))
        {
            return -1;
        }

        yupiter_heap_block_load(cursor, &block);

        if (((block.payload % yupiter_heap_alignment) != 0) ||
            (block.payload > (yupiter_heap_size - cursor -
                              yupiter_heap_header)))
        {
            return -1;
        }

        if (cursor == offset)
        {
            *p_block = block;
            *p_previous = previous;
            return 0;
        }

        previous = cursor;
        cursor = cursor + yupiter_heap_header + block.payload;
    }

    return -1;
}

/*
 * Find the live block whose payload starts at "p".
 *
 * On success, stores the offset of the block header in "*p_block_offset"
 * and the header in "*p_block", and returns RELEASE_RESULT_ACCEPT.
 * Every other result classifies the pointer the same way the release
 * path does, so a resize can report an invalid pointer with the same
 * wording; the arena is not modified.
 */
static release_result yupiter_heap_locate(void *p, size_t *p_block_offset,
                                 yupiter_heap_block *p_block)
{
    uintptr_t   address;
    uintptr_t   base;
    size_t      block_offset;
    size_t      previous_offset;
    yupiter_heap_block block;

    address = (uintptr_t)(p);
    base = (uintptr_t)(yupiter_heap_arena);

    if ((address < (base + (uintptr_t)(yupiter_heap_header))) ||
        (address >= (base + (uintptr_t)(yupiter_heap_size))))
    {
        return RELEASE_RESULT_FOREIGN;
    }

    block_offset = (size_t)(address - base) -
                   yupiter_heap_header;

    if ((block_offset % yupiter_heap_alignment) != 0)
    {
        return RELEASE_RESULT_FOREIGN;
    }

    if (yupiter_heap_find_block(block_offset, &block, &previous_offset) != 0)
    {
        yupiter_heap_block_load(block_offset, &block);

        if ((block.magic == yupiter_heap_magic) &&
            (block.state == BLOCK_STATE_FREE))
        {
            return RELEASE_RESULT_DOUBLE_FREE;
        }

        return RELEASE_RESULT_INTERIOR;
    }

    if (block.magic != yupiter_heap_magic)
    {
        return RELEASE_RESULT_CORRUPT;
    }

    if (block.state == BLOCK_STATE_FREE)
    {
        return RELEASE_RESULT_DOUBLE_FREE;
    }

    if (block.state != BLOCK_STATE_USED)
    {
        return RELEASE_RESULT_CORRUPT;
    }

    if (block.payload >
        (yupiter_heap_size - block_offset - yupiter_heap_header))
    {
        return RELEASE_RESULT_CORRUPT;
    }

    *p_block_offset = block_offset;
    *p_block = block;
    return RELEASE_RESULT_ACCEPT;
}

/*
 * Release the block that "p" points at the start of.
 *
 * Returns RELEASE_RESULT_ACCEPT when the block was released (also for
 * a null pointer, which releases nothing), and one of the
 * classification values otherwise without having modified anything.
 *
 * The block list is walked once: the walk that finds the block also
 * reports the one before it, and the blocks after it begin where the
 * merged one ends, so the free neighbours are merged in the same pass.
 * Everything is validated before the first byte is written, so a list
 * that does not add up is reported with the arena still untouched.
 */
static release_result yupiter_heap_release(void *p)
{
    uintptr_t   address;
    uintptr_t   base;
    size_t      offset;
    size_t      block_offset;
    size_t      previous_offset;
    size_t      next_offset;
    size_t      released_payload;
    size_t      merged_payload;
    bool             merge_previous;
    yupiter_heap_block block;

    if (p == NULL)
    {
        return RELEASE_RESULT_ACCEPT;
    }

    address = (uintptr_t)(p);
    base = (uintptr_t)(yupiter_heap_arena);

    /*
     * The range check is done on addresses before any pointer
     * arithmetic, so a pointer from outside the arena never produces
     * a difference between unrelated pointers.
     */
    if ((address < (base + (uintptr_t)(yupiter_heap_header))) ||
        (address >= (base + (uintptr_t)(yupiter_heap_size))))
    {
        return RELEASE_RESULT_FOREIGN;
    }

    offset = (size_t)(address - base);
    block_offset = offset - yupiter_heap_header;

    if ((block_offset % yupiter_heap_alignment) != 0)
    {
        return RELEASE_RESULT_FOREIGN;
    }

    if (yupiter_heap_find_block(block_offset, &block, &previous_offset) != 0)
    {
        /*
         * No block starts here.  A header that a merge absorbed still
         * reads as free, which is how the double free of a block that
         * is no longer on the list is recognised; every other case is
         * a pointer into the middle of a block.
         */
        yupiter_heap_block_load(block_offset, &block);

        if ((block.magic == yupiter_heap_magic) &&
            (block.state == BLOCK_STATE_FREE))
        {
            return RELEASE_RESULT_DOUBLE_FREE;
        }

        return RELEASE_RESULT_INTERIOR;
    }

    if (block.magic != yupiter_heap_magic)
    {
        return RELEASE_RESULT_CORRUPT;
    }

    if (block.state == BLOCK_STATE_FREE)
    {
        return RELEASE_RESULT_DOUBLE_FREE;
    }

    if (block.state != BLOCK_STATE_USED)
    {
        return RELEASE_RESULT_CORRUPT;
    }

    if (block.payload >
        (yupiter_heap_size - block_offset - yupiter_heap_header))
    {
        return RELEASE_RESULT_CORRUPT;
    }

    released_payload = block.payload;
    merged_payload = block.payload;
    merge_previous = false;

    /*
     * The free block before this one, when there is one.  It is the
     * block the walk stepped over, so its size fields were validated
     * on the way; a header that has lost its magic, or a state that is
     * neither free nor in use, means the list has lost its shape.
     */
    if (previous_offset != yupiter_heap_no_previous)
    {
        yupiter_heap_block previous;

        yupiter_heap_block_load(previous_offset, &previous);

        if (previous.magic != yupiter_heap_magic)
        {
            return RELEASE_RESULT_CORRUPT;
        }

        if (previous.state != BLOCK_STATE_USED)
        {
            if (previous.state != BLOCK_STATE_FREE)
            {
                return RELEASE_RESULT_CORRUPT;
            }

            merge_previous = true;
            merged_payload = merged_payload + yupiter_heap_header +
                             previous.payload;
        }
    }

    /*
     * The free blocks after this one.  They begin where the released
     * block ends - the absorbed block before it reaches further back,
     * not further forward - so the scan starts from the released
     * payload.  The invariant of the list says at most one free block
     * can be there, but the loop leaves the list merged even if some
     * earlier fault left a run behind, and every block it considers is
     * validated before its size is trusted.
     */
    next_offset = block_offset + yupiter_heap_header + released_payload;

    while (next_offset < yupiter_heap_size)
    {
        yupiter_heap_block next;

        if (next_offset > (yupiter_heap_size - yupiter_heap_header))
        {
            return RELEASE_RESULT_CORRUPT;
        }

        yupiter_heap_block_load(next_offset, &next);

        if ((next.magic != yupiter_heap_magic) ||
            ((next.state != BLOCK_STATE_FREE) &&
             (next.state != BLOCK_STATE_USED)) ||
            ((next.payload % yupiter_heap_alignment) != 0) ||
            (next.payload > (yupiter_heap_size - next_offset -
                             yupiter_heap_header)))
        {
            return RELEASE_RESULT_CORRUPT;
        }

        if (next.state == BLOCK_STATE_USED)
        {
            break;
        }

        merged_payload = merged_payload + yupiter_heap_header + next.payload;
        next_offset = next_offset + yupiter_heap_header + next.payload;
    }

    /*
     * Everything adds up, so the arena may now be changed.  The
     * released block is marked free on its own first: when the block
     * before it is absorbed, its header stays behind as the stale
     * bytes that make a later double free of the same pointer
     * recognisable instead of looking like a pointer into a payload.
     */
    block.payload = released_payload;
    block.state = BLOCK_STATE_FREE;
    yupiter_heap_block_store(block_offset, &block);

    if (merge_previous)
    {
        block.payload = merged_payload;
        yupiter_heap_block_store(previous_offset, &block);
    }
    else if (merged_payload != released_payload)
    {
        block.payload = merged_payload;
        yupiter_heap_block_store(block_offset, &block);
    }

    /*
     * The accounting drops the payload of the released block alone: a
     * free block that was absorbed never had an entry in it.
     */
    yupiter_heap_used_bytes = yupiter_heap_used_bytes - released_payload;
    yupiter_heap_used_blocks = yupiter_heap_used_blocks - 1U;

    return RELEASE_RESULT_ACCEPT;
}

/*
 * Translate a release classification into the resize equivalent.  The
 * two enumerations share the meaning of the pointer faults, so the
 * mapping has no policy of its own beyond the extra resize results.
 */
static resize_status yupiter_heap_resize_status(release_result result)
{
    switch (result)
    {
        case RELEASE_RESULT_ACCEPT:
            return RESIZE_STATUS_ACCEPT;
        case RELEASE_RESULT_FOREIGN:
            return RESIZE_STATUS_FOREIGN;
        case RELEASE_RESULT_INTERIOR:
            return RESIZE_STATUS_INTERIOR;
        case RELEASE_RESULT_DOUBLE_FREE:
            return RESIZE_STATUS_DOUBLE_FREE;
        case RELEASE_RESULT_CORRUPT:
            return RESIZE_STATUS_CORRUPT;
        default:
            return RESIZE_STATUS_CORRUPT;
    }
}

/*
 * Resize the block whose payload starts at "p".
 *
 * A null "p" allocates, and a size of 0 releases, exactly as the
 * public functions document.  Otherwise the block is located first;
 * when it can grow into the free blocks that follow, or shrink and
 * leave a free tail, it stays where it is, and only a block that
 * cannot be resized in place is copied into a fresh allocation and
 * released.  Nothing is written until the block list around the
 * resized block has been validated.
 *
 * "*pp_result" is the new pointer on success and null on every
 * failure.  Alignment overflow is reported for both null and live
 * pointers.
 */
static resize_status yupiter_heap_resize(void *p, size_t size, void **pp_result)
{
    size_t      block_offset;
    size_t      requested;
    size_t      old_payload;
    size_t      cursor;
    size_t      available;
    size_t      spare;
    size_t      tail_offset;
    yupiter_heap_block block;
    yupiter_heap_block tail;
    release_result   located;
    release_result   released;
    void            *p_result;

    *pp_result = NULL;

    if (p == NULL)
    {
        if (size == 0)
        {
            return RESIZE_STATUS_ACCEPT;
        }

        if (yupiter_heap_align_size(size, &requested) != 0)
        {
            return RESIZE_STATUS_OVERFLOW;
        }

        p_result = yupiter_malloc(size);

        if (p_result == NULL)
        {
            return RESIZE_STATUS_OUT_OF_MEMORY;
        }

        *pp_result = p_result;
        return RESIZE_STATUS_ACCEPT;
    }

    yupiter_heap_initialise();

    located = yupiter_heap_locate(p, &block_offset, &block);

    if (located != RELEASE_RESULT_ACCEPT)
    {
        return yupiter_heap_resize_status(located);
    }

    old_payload = block.payload;

    if (size == 0)
    {
        released = yupiter_heap_release(p);
        return yupiter_heap_resize_status(released);
    }

    if (yupiter_heap_align_size(size, &requested) != 0)
    {
        return RESIZE_STATUS_OVERFLOW;
    }

    if (requested > (yupiter_heap_size - yupiter_heap_header))
    {
        return RESIZE_STATUS_OUT_OF_MEMORY;
    }

    /*
     * Shrinking.  A leftover too small to hold another block stays in
     * the block; a larger one becomes a free tail.  The tail absorbs
     * any free block that follows, because a split must not leave two
     * free blocks next to each other.
     */
    if (requested <= old_payload)
    {
        spare = old_payload - requested;

        if (spare < (yupiter_heap_header + yupiter_heap_alignment))
        {
            *pp_result =
                &yupiter_heap_arena[block_offset + yupiter_heap_header];
            return RESIZE_STATUS_ACCEPT;
        }

        tail.payload = spare - yupiter_heap_header;
        tail.state = BLOCK_STATE_FREE;
        tail.magic = yupiter_heap_magic;
        tail_offset = block_offset + yupiter_heap_header + requested;

        cursor = block_offset + yupiter_heap_header + old_payload;

        while (cursor < yupiter_heap_size)
        {
            yupiter_heap_block next;

            if (cursor > (yupiter_heap_size - yupiter_heap_header))
            {
                return RESIZE_STATUS_CORRUPT;
            }

            yupiter_heap_block_load(cursor, &next);

            if ((next.magic != yupiter_heap_magic) ||
                ((next.state != BLOCK_STATE_FREE) &&
                 (next.state != BLOCK_STATE_USED)) ||
                ((next.payload % yupiter_heap_alignment) != 0) ||
                (next.payload > (yupiter_heap_size - cursor -
                                 yupiter_heap_header)))
            {
                return RESIZE_STATUS_CORRUPT;
            }

            if (next.state == BLOCK_STATE_USED)
            {
                break;
            }

            tail.payload = tail.payload + yupiter_heap_header + next.payload;
            cursor = cursor + yupiter_heap_header + next.payload;
        }

        block.payload = requested;
        block.state = BLOCK_STATE_USED;
        yupiter_heap_block_store(block_offset, &block);
        yupiter_heap_block_store(tail_offset, &tail);

        yupiter_heap_used_bytes = yupiter_heap_used_bytes -
                                (old_payload - requested);
        *pp_result = &yupiter_heap_arena[block_offset + yupiter_heap_header];
        return RESIZE_STATUS_ACCEPT;
    }

    /*
     * Growing.  Absorb every free block that follows; when the result
     * is still too small the block has to move.
     */
    available = old_payload;
    cursor = block_offset + yupiter_heap_header + old_payload;

    while (cursor < yupiter_heap_size)
    {
        yupiter_heap_block next;

        if (cursor > (yupiter_heap_size - yupiter_heap_header))
        {
            return RESIZE_STATUS_CORRUPT;
        }

        yupiter_heap_block_load(cursor, &next);

        if ((next.magic != yupiter_heap_magic) ||
            ((next.state != BLOCK_STATE_FREE) &&
             (next.state != BLOCK_STATE_USED)) ||
            ((next.payload % yupiter_heap_alignment) != 0) ||
            (next.payload > (yupiter_heap_size - cursor -
                             yupiter_heap_header)))
        {
            return RESIZE_STATUS_CORRUPT;
        }

        if (next.state == BLOCK_STATE_USED)
        {
            break;
        }

        available = available + yupiter_heap_header + next.payload;
        cursor = cursor + yupiter_heap_header + next.payload;
    }

    if (available >= requested)
    {
        spare = available - requested;

        if (spare >= (yupiter_heap_header + yupiter_heap_alignment))
        {
            tail.payload = spare - yupiter_heap_header;
            tail.state = BLOCK_STATE_FREE;
            tail.magic = yupiter_heap_magic;
            tail_offset = block_offset + yupiter_heap_header + requested;

            block.payload = requested;
            block.state = BLOCK_STATE_USED;
            yupiter_heap_block_store(block_offset, &block);
            yupiter_heap_block_store(tail_offset, &tail);
        }
        else
        {
            block.payload = available;
            block.state = BLOCK_STATE_USED;
            yupiter_heap_block_store(block_offset, &block);
        }

        yupiter_heap_used_bytes = yupiter_heap_used_bytes +
                                (block.payload - old_payload);
        *pp_result = &yupiter_heap_arena[block_offset + yupiter_heap_header];
        return RESIZE_STATUS_ACCEPT;
    }

    p_result = yupiter_malloc(size);

    if (p_result == NULL)
    {
        return RESIZE_STATUS_OUT_OF_MEMORY;
    }

    (void)(yupiter_memcpy(p_result, p,
                                   (old_payload < size) ? old_payload : size));

    /*
     * The new copy is useless if the old block cannot be released.  The
     * release validates the list before it writes anything, so a failure
     * leaves the original live; release the copy as well and report the
     * failure instead of returning a pointer while the old block still
     * owns its accounting.
     */
    released = yupiter_heap_release(p);

    if (released != RELEASE_RESULT_ACCEPT)
    {
        (void)(yupiter_heap_release(p_result));
        return yupiter_heap_resize_status(released);
    }

    *pp_result = p_result;
    return RESIZE_STATUS_ACCEPT;
}

/*
 * Report an unrecoverable heap failure and terminate the process.
 *
 * The diagnostic goes to yupiter_stderr, which is unbuffered, so it
 * reaches the operating system before the exit system call.
 */
_Noreturn static void yupiter_heap_fatal(const char *p_function,
                                   const char *p_reason)
{
    (void)(yupiter_fprintf(yupiter_stderr, "yupiter: %s: %s\n",
                                    p_function, p_reason));

    yupiter_sys_exit(yupiter_heap_fatal_status);
}

/*
 * Human-readable form of a classification value.
 */
static const char *yupiter_heap_explain(release_result result)
{
    switch (result)
    {
        case RELEASE_RESULT_FOREIGN:
            return "pointer is outside the yupiter heap";
        case RELEASE_RESULT_INTERIOR:
            return "pointer is not the start of a heap block";
        case RELEASE_RESULT_DOUBLE_FREE:
            return "block is already free (double free)";
        default:
            return "heap block is corrupted";
    }
}

/*
 * Human-readable form of a resize failure.  The foreign, interior,
 * double-free and corrupt values have the same meaning as in a release,
 * so their wording comes from the same place; the other two are
 * specific to a resize.
 */
static const char *yupiter_heap_explain_resize(resize_status status)
{
    switch (status)
    {
        case RESIZE_STATUS_FOREIGN:
            return yupiter_heap_explain(RELEASE_RESULT_FOREIGN);
        case RESIZE_STATUS_INTERIOR:
            return yupiter_heap_explain(RELEASE_RESULT_INTERIOR);
        case RESIZE_STATUS_DOUBLE_FREE:
            return yupiter_heap_explain(RELEASE_RESULT_DOUBLE_FREE);
        case RESIZE_STATUS_OUT_OF_MEMORY:
            return "out of memory";
        case RESIZE_STATUS_OVERFLOW:
            return "allocation size overflows";
        case RESIZE_STATUS_CORRUPT:
            return yupiter_heap_explain(RELEASE_RESULT_CORRUPT);
        default:
            return yupiter_heap_explain(RELEASE_RESULT_CORRUPT);
    }
}


YUPITER_PUBLIC void *yupiter_malloc(size_t size)
{
    size_t requested;
    size_t cursor;

    if (size == 0)
    {
        return NULL;
    }

    yupiter_heap_initialise();

    if (yupiter_heap_align_size(size, &requested) != 0)
    {
        return NULL;
    }

    if (requested > (yupiter_heap_size - yupiter_heap_header))
    {
        return NULL;
    }

    cursor = 0;

    while (cursor < yupiter_heap_size)
    {
        yupiter_heap_block block;

        if (cursor > (yupiter_heap_size - yupiter_heap_header))
        {
            return NULL;
        }

        yupiter_heap_block_load(cursor, &block);

        if (((block.payload % yupiter_heap_alignment) != 0) ||
            (block.payload > (yupiter_heap_size - cursor -
                              yupiter_heap_header)))
        {
            return NULL;
        }

        if ((block.state == BLOCK_STATE_FREE) &&
            (block.payload >= requested))
        {
            size_t spare;

            spare = block.payload - requested;
            block.state = BLOCK_STATE_USED;

            if (spare >= (yupiter_heap_header + yupiter_heap_alignment))
            {
                yupiter_heap_block tail;

                tail.payload = spare - yupiter_heap_header;
                tail.state = BLOCK_STATE_FREE;
                tail.magic = yupiter_heap_magic;

                block.payload = requested;

                yupiter_heap_block_store(cursor, &block);
                yupiter_heap_block_store(cursor + yupiter_heap_header +
                                       requested, &tail);
            }
            else
            {
                /* The leftover is too small to hold a block: keep it. */
                yupiter_heap_block_store(cursor, &block);
            }

            yupiter_heap_used_bytes = yupiter_heap_used_bytes + block.payload;
            yupiter_heap_used_blocks = yupiter_heap_used_blocks + 1U;

            return &yupiter_heap_arena[cursor + yupiter_heap_header];
        }

        cursor = cursor + yupiter_heap_header + block.payload;
    }

    return NULL;
}

YUPITER_PUBLIC void yupiter_free(void *p)
{
    if (p == NULL)
    {
        return;
    }

    yupiter_heap_initialise();

    /*
     * The unsafe entry point is forgiving by design: a pointer it
     * does not recognise is ignored rather than acted upon, and the
     * safe entry point below is the one that reports it.
     */
    (void)(yupiter_heap_release(p));
}

YUPITER_PUBLIC void *yupiter_malloc_safe(size_t count,
                                                     size_t size)
{
    void       *p;
    size_t total;

    if ((count != 0) && (size > (SIZE_MAX / count)))
    {
        yupiter_heap_fatal("yupiter_malloc_safe", "allocation size overflows");
    }

    total = count * size;
    p = NULL;

    if (total != 0)
    {
        p = yupiter_malloc(total);

        if (p == NULL)
        {
            yupiter_heap_fatal("yupiter_malloc_safe", "out of memory");
        }

        (void)(yupiter_memset(p, 0, total));
    }

    return p;
}

YUPITER_PUBLIC void yupiter_free_safe(void *p)
{
    release_result result;

    if (p == NULL)
    {
        return;
    }

    yupiter_heap_initialise();

    result = yupiter_heap_release(p);

    if (result == RELEASE_RESULT_ACCEPT)
    {
        return;
    }

    yupiter_heap_fatal("yupiter_free_safe", yupiter_heap_explain(result));
}

YUPITER_PUBLIC void *yupiter_realloc(void *p, size_t size)
{
    resize_status status;
    void         *p_result;

    status = yupiter_heap_resize(p, size, &p_result);

    if (status != RESIZE_STATUS_ACCEPT)
    {
        return NULL;
    }

    return p_result;
}

YUPITER_PUBLIC void *yupiter_realloc_safe(void *p, size_t count,
                                    size_t size)
{
    resize_status status;
    size_t   total;
    void         *p_result;

    if ((count != 0) && (size > (SIZE_MAX / count)))
    {
        yupiter_heap_fatal("yupiter_realloc_safe", "allocation size overflows");
    }

    /*
     * A null pointer is a fresh allocation, so the zero fill that
     * yupiter_malloc_safe promises applies here as well.  A resize of an
     * existing block does not zero the bytes it adds, because the
     * allocator does not keep the requested size of a block; the
     * contract in <yupiter.h> says so.
     */
    if (p == NULL)
    {
        return yupiter_malloc_safe(count, size);
    }

    total = count * size;
    status = yupiter_heap_resize(p, total, &p_result);

    if (status != RESIZE_STATUS_ACCEPT)
    {
        yupiter_heap_fatal("yupiter_realloc_safe",
                         yupiter_heap_explain_resize(status));
    }

    return p_result;
}

/*
 * Release a live block without invoking the fatal handler.  Null succeeds;
 * foreign, interior, freed or corrupt pointers return INVALID_POINTER.
 */
YUPITER_PUBLIC yupiter_memory_status yupiter_free_checked(void *p)
{
    if (p == NULL)
    {
        return YUPITER_MEMORY_OK;
    }

    yupiter_heap_initialise();
    return (yupiter_heap_release(p) == RELEASE_RESULT_ACCEPT)
               ? YUPITER_MEMORY_OK : YUPITER_MEMORY_INVALID_POINTER;
}

/*
 * Resize with explicit status, clearing *result on failure and preserving
 * the original block.  Null allocates, zero size releases, and alignment
 * overflow is distinct from exhaustion.  Result storage must be writable
 * and outside the heap; missing storage returns INVALID_POINTER.
 */
YUPITER_PUBLIC yupiter_memory_status
    yupiter_realloc_checked(void *p, size_t size, void **result)
{
    if (result == NULL)
    {
        return YUPITER_MEMORY_INVALID_POINTER;
    }

    switch (yupiter_heap_resize(p, size, result))
    {
        case RESIZE_STATUS_ACCEPT:
            return YUPITER_MEMORY_OK;
        case RESIZE_STATUS_OUT_OF_MEMORY:
            return YUPITER_MEMORY_OUT_OF_MEMORY;
        case RESIZE_STATUS_OVERFLOW:
            return YUPITER_MEMORY_OVERFLOW;
        default:
            return YUPITER_MEMORY_INVALID_POINTER;
    }
}

size_t yupiter_heap_live_bytes(void)
{
    return yupiter_heap_used_bytes;
}

size_t yupiter_heap_live_blocks(void)
{
    return yupiter_heap_used_blocks;
}

size_t yupiter_heap_capacity(void)
{
    return yupiter_heap_size;
}

