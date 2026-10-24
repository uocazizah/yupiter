/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - input layer
 * Copyright (C) 2026 Uoc Azizah
 */

#include <cstddef>
#include <cstdint>

#include "yupiter.h"
#include "yupiter_io.hxx"

namespace yupiter::io
{

extern "C" int yupiter_getchar(void)
{
    return yupiter_fgetc(yupiter_stdin);
}

extern "C" int yupiter_fgetc(yupiter_FILE *stream)
{
    return yupiter_stream_getc(stream);
}

extern "C" char *yupiter_fgets(char *s, int n, yupiter_FILE *stream)
{
    int   index;
    int   value;

    if ((s == nullptr) || (n <= 0) || (stream == nullptr))
    {
        return nullptr;
    }

    index = 0;

    while (index < n - 1)
    {
        value = yupiter_stream_getc(stream);

        if (value == YUPITER_EOF)
        {
            if (index == 0)
            {
                return nullptr;
            }
            break;
        }

        s[index] = static_cast<char>(value);
        index = index + 1;

        if (value == '\n')
        {
            break;
        }
    }

    s[index] = '\0';
    return s;
}

extern "C" std::size_t yupiter_fread(void *ptr, std::size_t size,
                                    std::size_t nmemb, yupiter_FILE *stream)
{
    std::size_t total;
    std::size_t produced;
    int         overflow;

    if ((ptr == nullptr) || (stream == nullptr))
    {
        return 0;
    }

    if ((size == 0) || (nmemb == 0))
    {
        return 0;
    }

    overflow = 0;
    total = size * nmemb;

    if (total / size != nmemb)
    {
        overflow = 1;
    }

    if (overflow != 0)
    {
        return 0;
    }

    if (yupiter_stream_read(stream, ptr, total, &produced) != 0)
    {
        return 0;
    }

    return produced / size;
}

extern "C" int yupiter_ungetc(int c, yupiter_FILE *stream)
{
    return yupiter_stream_ungetc(stream, c);
}

extern "C" int yupiter_feof(yupiter_FILE *stream)
{
    if (stream == nullptr)
    {
        return 0;
    }

    return stream->eof;
}

extern "C" int yupiter_ferror(yupiter_FILE *stream)
{
    if (stream == nullptr)
    {
        return 0;
    }

    return stream->error;
}

extern "C" void yupiter_clearerr(yupiter_FILE *stream)
{
    if (stream == nullptr)
    {
        return;
    }

    stream->error = 0;
    stream->eof = 0;
    stream->has_pushback = 0;
}

extern "C" int yupiter_fflush(yupiter_FILE *stream)
{
    return yupiter_stream_flush(stream);
}

} /* namespace yupiter::io */