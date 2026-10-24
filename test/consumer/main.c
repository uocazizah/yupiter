/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - a program that consumes the installed package
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
 * The program the install test builds against an installed yupiter.
 *
 * It uses the two halves of the library a consumer is most likely to
 * reach for - the formatted output and the allocator - and exits
 * non-zero if either of them misbehaves, so a package that installs the
 * wrong header or an archive without the symbols fails the test instead
 * of passing on the strength of having linked.
 *
 * It is an ordinary hosted program on purpose: what is under test here
 * is the package, not the freestanding link, which the rest of the
 * suite already covers.
 *
 * The allocation is deliberately small.  The build accepts an arena of
 * 64 bytes, the whole of which has to cover the block headers as well as
 * the payload, and a consumer that asked for a page would fail the test
 * for a library that was configured for a very small heap rather than
 * for a package that was installed badly.
 */

#include <yupiter.h>

int main(void)
{
    void *p_block;
    int   written;

    p_block = yupiter_malloc(16u);

    if (p_block == NULL)
    {
        return 1;
    }

    yupiter_free(p_block);

    written = yupiter_printf("consumed yupiter\n");

    if (written != 17)
    {
        return 1;
    }

    return 0;
}
