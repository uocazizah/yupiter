/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - input layer
 * Copyright (C) 2026 Uoc Azizah
 */

#include <stddef.h>
#include <stdint.h>

#include "yupiter.h"
#include "yupiter_io.h"


YUPITER_PUBLIC int yupiter_getchar(void)
{
    return yupiter_fgetc(yupiter_stdin);
}

YUPITER_PUBLIC int yupiter_fgetc(yupiter_FILE *stream)
{
    return yupiter_stream_getc(stream);
}

YUPITER_PUBLIC char *yupiter_fgets(char *s, int n, yupiter_FILE *stream)
{
    int   index;
    int   value;

    if ((s == NULL) || (n <= 0) || (stream == NULL))
    {
        return NULL;
    }

    index = 0;

    while (index < n - 1)
    {
        value = yupiter_stream_getc(stream);

        if (value == YUPITER_EOF)
        {
            if (index == 0)
            {
                return NULL;
            }
            break;
        }

        s[index] = (char)(value);
        index = index + 1;

        if (value == '\n')
        {
            break;
        }
    }

    s[index] = '\0';
    return s;
}

YUPITER_PUBLIC size_t yupiter_fread(void *ptr, size_t size,
                                    size_t nmemb, yupiter_FILE *stream)
{
    size_t total;
    size_t produced;
    int         overflow;

    if ((ptr == NULL) || (stream == NULL))
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

YUPITER_PUBLIC int yupiter_ungetc(int c, yupiter_FILE *stream)
{
    return yupiter_stream_ungetc(stream, c);
}

YUPITER_PUBLIC int yupiter_feof(yupiter_FILE *stream)
{
    if (stream == NULL)
    {
        return 0;
    }

    return stream->eof;
}

YUPITER_PUBLIC int yupiter_ferror(yupiter_FILE *stream)
{
    if (stream == NULL)
    {
        return 0;
    }

    return stream->error;
}

YUPITER_PUBLIC void yupiter_clearerr(yupiter_FILE *stream)
{
    if (stream == NULL)
    {
        return;
    }

    stream->error = 0;
    stream->eof = 0;
    stream->has_pushback = 0;
}

YUPITER_PUBLIC int yupiter_fflush(yupiter_FILE *stream)
{
    return yupiter_stream_flush(stream);
}

