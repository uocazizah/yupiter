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
 * Minimal internal string and memory helpers.
 *
 * The library is linked with "-nostdlib", so it may not call strlen,
 * memcpy or memset from a host C library: those symbols would either
 * be missing at link time or, worse, silently resolve to a different
 * implementation than the one under test.  The helpers below are
 * therefore defined here and prefixed so that they never collide with
 * a host C library when yupiter is linked into a hosted program.
 *
 * The definitions carry the C linkage the internal header declares, so
 * the C test programs can link them under their unmangled names.
 *
 * None of these functions is part of the public API.
 */

#include <cstddef>

#include "yupiter_io.hxx"

namespace yupiter::io
{

extern "C" std::size_t yupiter_strlen(const char *p_string)
{
    const char *p_cursor;

    if (p_string == nullptr)
    {
        return 0;
    }

    /*
     * A pointer walk lets the compiler use one post-incremented load
     * per byte.  Reading a whole word would be faster still, but it
     * could read past the terminating null and outside the object,
     * which this library does not do.
     */
    p_cursor = p_string;

    while (*p_cursor != '\0')
    {
        p_cursor = p_cursor + 1;
    }

    return static_cast<std::size_t>(p_cursor - p_string);
}

extern "C" void *yupiter_memcpy(void *p_destination, const void *p_source,
                              std::size_t length)
{
    /*
     * Sixteen bytes is the widest block every target this library
     * supports can move with its baseline vector instructions, and it
     * is also the size of one heap block header.  The block is moved
     * through a local byte array so the compiler can combine the byte
     * loads and stores into wider ones without either argument having
     * to be cast to a wider type; the copy stays inside "length" and
     * reads no byte past the end of the source.
     */
    constexpr std::size_t block_size = 16;

    unsigned char       *p_to;
    const unsigned char *p_from;

    if ((p_destination == nullptr) || (p_source == nullptr))
    {
        return p_destination;
    }

    if (length == 0)
    {
        return p_destination;
    }

    if (p_destination == p_source)
    {
        return p_destination;
    }

    p_to = static_cast<unsigned char *>(p_destination);
    p_from = static_cast<const unsigned char *>(p_source);

    while (length >= block_size)
    {
        unsigned char block[block_size];
        std::size_t   index;

        for (index = 0; index < block_size; index = index + 1)
        {
            block[index] = p_from[index];
        }

        for (index = 0; index < block_size; index = index + 1)
        {
            p_to[index] = block[index];
        }

        p_to = p_to + block_size;
        p_from = p_from + block_size;
        length = length - block_size;
    }

    while (length != 0)
    {
        *p_to = *p_from;
        p_to = p_to + 1;
        p_from = p_from + 1;
        length = length - 1;
    }

    return p_destination;
}

extern "C" void *yupiter_memset(void *p_destination, int value,
                              std::size_t length)
{
    constexpr std::size_t block_size = 16;

    unsigned char *p_to;
    unsigned char  byte_value;

    if (p_destination == nullptr)
    {
        return p_destination;
    }

    p_to = static_cast<unsigned char *>(p_destination);
    byte_value = static_cast<unsigned char>(value);

    /*
     * Fill one block once and store it repeatedly.  As in
     * yupiter_memcpy, the local byte array is what lets the compiler
     * turn the loop into a wide store without a cast that would break
     * the aliasing rules.
     */
    if (length >= block_size)
    {
        unsigned char block[block_size];
        std::size_t   index;

        for (index = 0; index < block_size; index = index + 1)
        {
            block[index] = byte_value;
        }

        while (length >= block_size)
        {
            for (index = 0; index < block_size; index = index + 1)
            {
                p_to[index] = block[index];
            }

            p_to = p_to + block_size;
            length = length - block_size;
        }
    }

    while (length != 0)
    {
        *p_to = byte_value;
        p_to = p_to + 1;
        length = length - 1;
    }

    return p_destination;
}

} /* namespace yupiter::io */
