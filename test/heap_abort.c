/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - safe heap failure test
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
 * The fail-fast half of the safe heap API.
 *
 * One scenario is named on the command line, and each scenario is a
 * mistake the safe functions are documented to refuse: an overflowing
 * request, a request the exhausted heap cannot meet, a double free
 * and a pointer that was never allocated.  Every scenario has to end
 * inside yupiter with its diagnostic and status 134, so this program
 * returns 0 whenever it does not terminate - an unknown scenario name
 * or a failure that went unrecognised therefore fails the test rather
 * than passing silently.  CheckFatal.cmake asserts the status.
 *
 * Only the small allocations the scenarios need are made, so a bug
 * that leaks instead of terminating loses a few kilobytes at most.
 */

#include <stddef.h>
#include <stdint.h>

#include "yupiter_io.hxx"
#include "yupiter_mem.hxx"

/*
 * Compare two null-terminated strings without a string function of
 * this library's own: the internal header declares none.
 */
static int scenario_is(const char *p_scenario, const char *p_expected)
{
    size_t index;

    if (p_scenario == NULL)
    {
        return 0;
    }

    for (index = 0; ; index = index + 1)
    {
        if (p_scenario[index] != p_expected[index])
        {
            return 0;
        }

        if (p_scenario[index] == '\0')
        {
            return 1;
        }
    }
}

/*
 * Fill the arena with 64 byte blocks until the heap has nothing left.
 * The previous head is kept in the payload itself, so the list needs
 * no storage of its own.
 */
static void fill_the_heap(void)
{
    void *p_head;
    void *p_block;

    p_head = NULL;

    for (;;)
    {
        p_block = yupiter_malloc(64);

        if (p_block == NULL)
        {
            break;
        }

        *(void **)p_block = p_head;
        p_head = p_block;
    }
}

int main(int argc, char **argv)
{
    const char *p_scenario;
    void       *p_block;

    if (argc < 2)
    {
        yupiter_fprintf(yupiter_stderr, "usage: heap_abort <scenario>\n");
        return 0;
    }

    p_scenario = argv[1];
    p_block = NULL;

    if (scenario_is(p_scenario, "overflow") != 0)
    {
        /* "count * size" wraps around, which must not be truncated. */
        (void)yupiter_malloc_safe(SIZE_MAX, 2u);
    }
    else if (scenario_is(p_scenario, "oom") != 0)
    {
        fill_the_heap();
        (void)yupiter_malloc_safe(1u, 64u);
    }
    else if (scenario_is(p_scenario, "double_free") != 0)
    {
        p_block = yupiter_malloc(16);

        if (p_block == NULL)
        {
            return 0;
        }

        yupiter_free_safe(p_block);
        yupiter_free_safe(p_block);
    }
    else if (scenario_is(p_scenario, "invalid_pointer") != 0)
    {
        yupiter_free_safe((void *)(uintptr_t)0x1234);
    }
    else if (scenario_is(p_scenario, "realloc_overflow") != 0)
    {
        (void)yupiter_realloc_safe(NULL, SIZE_MAX, 2u);
    }
    else if (scenario_is(p_scenario, "realloc_oom") != 0)
    {
        p_block = yupiter_malloc(16);

        if (p_block == NULL)
        {
            return 0;
        }

        fill_the_heap();
        (void)yupiter_realloc_safe(p_block, 1u, yupiter_heap_capacity());
    }
    else if (scenario_is(p_scenario, "realloc_double_free") != 0)
    {
        p_block = yupiter_malloc(16);

        if (p_block == NULL)
        {
            return 0;
        }

        yupiter_free_safe(p_block);
        (void)yupiter_realloc_safe(p_block, 1u, 16u);
    }
    else if (scenario_is(p_scenario, "realloc_invalid_pointer") != 0)
    {
        (void)yupiter_realloc_safe((void *)(uintptr_t)0x1234, 1u, 16u);
    }
    else
    {
        yupiter_fprintf(yupiter_stderr, "unknown scenario: %s\n", p_scenario);
        return 0;
    }

    /*
     * Every scenario above terminates inside yupiter, so reaching
     * this line means the failure was not recognised.
     */
    yupiter_fprintf(yupiter_stderr, "scenario \"%s\" did not terminate\n",
                  p_scenario);
    return 0;
}
