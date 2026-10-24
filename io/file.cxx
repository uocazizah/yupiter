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
 * The standard streams and their staging buffer.
 *
 * Every stream owns a fixed-size buffer embedded in the object, so
 * staging output performs no allocation.  Bytes are staged there and
 * handed to the operating system in a single write once the buffer is
 * full, once a newline is staged, or when the caller finishes a
 * complete formatted call.  Flushing eagerly on those three events
 * keeps output ordered without any exit-time hook, which the
 * freestanding startup path does not provide.
 *
 * The stream objects and the pointers to them are "constinit": the
 * freestanding startup path runs no constructors and walks no
 * ".init_array", so anything that would need dynamic initialization
 * must be rejected while the library is compiled instead of failing
 * silently at run time.
 */

#include <cstddef>

#include "yupiter_io.hxx"

namespace yupiter::io
{

namespace
{

/*
 * The three standard streams.  The pointers to them are const because
 * a caller may not rebind stdout to another object; the streams
 * themselves are ordinary mutable objects.
 *
 * stderr is marked unbuffered so that it is never reordered with
 * respect to stdout: every write is pushed to the kernel as soon as it
 * is staged.  stdout keeps a staging buffer.  stdin is marked
 * unbuffered as well, for the opposite reason: nothing writes to it,
 * so a buffer it could never flush would be dead space in the object.
 */
constinit yupiter_FILE yupiter_stream_stdin{
    0,   /* fd            */
    0,   /* error         */
    1,   /* unbuffered    */
    0,   /* eof           */
    0,   /* pushback      */
    0,   /* has_pushback  */
    0,   /* length        */
    {},  /* buffer        */
    0,   /* input_size    */
    0,   /* input_pos     */
    {}   /* input_buffer  */
};

constinit yupiter_FILE yupiter_stream_stdout{
    1,   /* fd            */
    0,   /* error         */
    0,   /* buffered      */
    0,   /* eof           */
    0,   /* pushback      */
    0,   /* has_pushback  */
    0,   /* length        */
    {},  /* buffer        */
    0,   /* input_size    */
    0,   /* input_pos     */
    {}   /* input_buffer  */
};

constinit yupiter_FILE yupiter_stream_stderr{
    2,   /* fd            */
    0,   /* error         */
    1,   /* unbuffered    */
    0,   /* eof           */
    0,   /* pushback      */
    0,   /* has_pushback  */
    0,   /* length        */
    {},  /* buffer        */
    0,   /* input_size    */
    0,   /* input_pos     */
    {}   /* input_buffer  */
};

} /* namespace */

extern "C" YUPITER_PUBLIC constinit yupiter_FILE *const yupiter_stdin =
    &yupiter_stream_stdin;
extern "C" YUPITER_PUBLIC constinit yupiter_FILE *const yupiter_stdout =
    &yupiter_stream_stdout;
extern "C" YUPITER_PUBLIC constinit yupiter_FILE *const yupiter_stderr =
    &yupiter_stream_stderr;

extern "C" int yupiter_stream_flush(yupiter_FILE *stream)
{
    std::size_t length;

    if (stream == nullptr)
    {
        return -1;
    }

    if (stream->error != 0)
    {
        return -1;
    }

    length = stream->length;

    if (length == 0)
    {
        return 0;
    }

    if (yupiter_sys_write(stream->fd, stream->buffer, length) != 0)
    {
        stream->error = 1;
        stream->length = 0;
        return -1;
    }

    stream->length = 0;
    return 0;
}

extern "C" void yupiter_stream_discard(yupiter_FILE *stream)
{
    if (stream == nullptr)
    {
        return;
    }

    stream->length = 0;
}

/*
 * Stage bytes into the stream buffer, flushing whenever it fills or a
 * newline is queued.  Returns 0 on success, -1 on failure.
 *
 * The bytes are copied in blocks rather than one at a time: the free
 * space in the buffer is filled in one go, and only the part that was
 * copied is looked at for the newline that has to trigger a flush.
 *
 * A stream that has nothing staged can skip the staging copy entirely:
 * an unbuffered stream is going to hand the bytes to the kernel anyway,
 * and a payload larger than the buffer would only be copied into it to
 * be flushed again.  Both cases go straight to yupiter_sys_write, which
 * already repeats a short write until the whole payload has been
 * accepted.
 */
extern "C" int yupiter_stream_write(yupiter_FILE *stream, const void *p_buffer,
                                  std::size_t length)
{
    const unsigned char *p_bytes;
    std::size_t          remaining;
    std::size_t          available;
    std::size_t          taken;
    std::size_t          index;
    int                  newline;

    if (stream == nullptr)
    {
        return -1;
    }

    if ((p_buffer == nullptr) && (length != 0))
    {
        stream->error = 1;
        return -1;
    }

    if (stream->error != 0)
    {
        return -1;
    }

    p_bytes = static_cast<const unsigned char *>(p_buffer);
    remaining = length;

    while (remaining > 0)
    {
        /*
         * Nothing is staged, so there is no ordering to preserve: hand
         * the whole remainder to the kernel when the stream is
         * unbuffered or when it would only be copied into the staging
         * buffer and flushed again.  The strict comparison keeps the
         * documented "a full buffer stays staged" behaviour for a
         * buffered payload of exactly YUPITER_BUFFER_SIZE bytes.
         */
        if ((stream->length == 0) &&
            ((stream->unbuffered != 0) ||
             (remaining > YUPITER_BUFFER_SIZE)))
        {
            if (yupiter_sys_write(stream->fd, p_bytes, remaining) != 0)
            {
                stream->error = 1;
                return -1;
            }

            return 0;
        }

        available = YUPITER_BUFFER_SIZE - stream->length;

        if (available == 0)
        {
            if (yupiter_stream_flush(stream) != 0)
            {
                return -1;
            }

            /*
             * The buffer is empty again.  Re-enter the loop so the
             * direct path above can take the rest of a large payload
             * instead of staging and flushing it 256 bytes at a time.
             */
            continue;
        }

        taken = (remaining < available) ? remaining : available;

        static_cast<void>(yupiter_memcpy(&stream->buffer[stream->length],
                                       p_bytes, taken));
        stream->length = stream->length + taken;

        /*
         * Only the bytes that have just been staged can hold a newline
         * that has not been flushed yet: one staged earlier went out
         * with the write that carried it.
         */
        newline = 0;
        for (index = 0; index < taken; index = index + 1)
        {
            if (p_bytes[index] == '\n')
            {
                newline = 1;
                break;
            }
        }

        p_bytes = p_bytes + taken;
        remaining = remaining - taken;

        if (newline != 0)
        {
            if (yupiter_stream_flush(stream) != 0)
            {
                return -1;
            }
        }
    }

    /*
     * An unbuffered stream hands every write to the kernel straight
     * away, so any bytes still staged once the caller is done are
     * pushed out here.  On a buffered stream this is a no-op and the
     * caller decides when to flush.
     */
    if (stream->unbuffered != 0)
    {
        if (yupiter_stream_flush(stream) != 0)
        {
            return -1;
        }
    }

    return 0;
}

extern "C" int yupiter_stream_read(yupiter_FILE *stream, void *p_buffer,
                                 std::size_t length, std::size_t *p_count)
{
    unsigned char       *p_destination;
    std::size_t          remaining;
    std::size_t          produced;

    if (stream == nullptr)
    {
        return -1;
    }

    if ((p_buffer == nullptr) && (length != 0))
    {
        stream->error = 1;
        if (p_count != nullptr)
        {
            *p_count = 0;
        }
        return -1;
    }

    if (stream->error != 0)
    {
        if (p_count != nullptr)
        {
            *p_count = 0;
        }
        return -1;
    }

    p_destination = static_cast<unsigned char *>(p_buffer);
    remaining = length;
    produced = 0;

    while (remaining > 0)
    {
        std::size_t available;

        if (stream->input_pos >= stream->input_size)
        {
            std::size_t got = 0;

            if (stream->eof != 0)
            {
                break;
            }

            if (yupiter_sys_read(stream->fd, stream->input_buffer,
                               YUPITER_BUFFER_SIZE, &got) != 0)
            {
                stream->error = 1;
                if (p_count != nullptr)
                {
                    *p_count = produced;
                }
                return -1;
            }

            if (got == 0)
            {
                stream->eof = 1;
                break;
            }

            stream->input_size = got;
            stream->input_pos = 0;
        }

        available = stream->input_size - stream->input_pos;

        if (available > remaining)
        {
            available = remaining;
        }

        static_cast<void>(yupiter_memcpy(p_destination + produced,
                                       &stream->input_buffer[stream->input_pos],
                                       available));

        stream->input_pos = stream->input_pos + available;
        produced = produced + available;
        remaining = remaining - available;
    }

    if (p_count != nullptr)
    {
        *p_count = produced;
    }

    return 0;
}

extern "C" int yupiter_stream_getc(yupiter_FILE *stream)
{
    int         value;

    if (stream == nullptr)
    {
        return YUPITER_EOF;
    }

    if (stream->has_pushback != 0)
    {
        stream->has_pushback = 0;
        return stream->pushback;
    }

    if (stream->error != 0)
    {
        return YUPITER_EOF;
    }

    if (stream->input_pos < stream->input_size)
    {
        value = static_cast<unsigned char>(
            stream->input_buffer[stream->input_pos]);
        stream->input_pos = stream->input_pos + 1;
        return value;
    }

    if (stream->eof != 0)
    {
        return YUPITER_EOF;
    }

    {
        std::size_t got = 0;

        if (yupiter_sys_read(stream->fd, stream->input_buffer,
                           YUPITER_BUFFER_SIZE, &got) != 0)
        {
            stream->error = 1;
            return YUPITER_EOF;
        }

        if (got == 0)
        {
            stream->eof = 1;
            return YUPITER_EOF;
        }

        stream->input_size = got;
        stream->input_pos = 0;

        value = static_cast<unsigned char>(
            stream->input_buffer[stream->input_pos]);
        stream->input_pos = stream->input_pos + 1;
        return value;
    }
}

extern "C" int yupiter_stream_ungetc(yupiter_FILE *stream, int c)
{
    if (stream == nullptr)
    {
        return YUPITER_EOF;
    }

    if (c == YUPITER_EOF)
    {
        return YUPITER_EOF;
    }

    if (stream->has_pushback != 0)
    {
        return YUPITER_EOF;
    }

    stream->pushback = c;
    stream->has_pushback = 1;
    if (stream->eof != 0)
    {
        stream->eof = 0;
    }

    return c;
}

} /* namespace yupiter::io */
