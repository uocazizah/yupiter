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

#include <cstdarg>

#include "yupiter_io.hxx"

namespace yupiter::io
{

extern "C" YUPITER_PUBLIC int yupiter_printf(const char *format, ...)
{
    va_list args;
    int     count;

    if (format == nullptr)
    {
        return -1;
    }

    va_start(args, format);
    count = yupiter_vfprintf(yupiter_stdout, format, args);
    va_end(args);

    /*
     * A failed call discards whatever it had staged so that half of a
     * conversion never reaches the output.  A successful call is
     * flushed here rather than at process exit, because the
     * freestanding startup path installs no exit handler and a buffer
     * left dirty would never be written.
     */
    if (count < 0)
    {
        yupiter_stream_discard(yupiter_stdout);
        return -1;
    }

    if (yupiter_stream_flush(yupiter_stdout) != 0)
    {
        return -1;
    }

    return count;
}

} /* namespace yupiter::io */
