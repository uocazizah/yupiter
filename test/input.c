/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - input layer smoke test
 */

#include <stddef.h>

#include "yupiter.h"

int main(void)
{
    char line[64];
    char buf[16];
    size_t got;
    int c;

    /* fgetc + ungetc */
    c = yupiter_getchar();
    yupiter_printf("first=%d\n", c);
    yupiter_ungetc(c, yupiter_stdin);
    c = yupiter_getchar();
    yupiter_printf("again=%d\n", c);

    /* fgets */
    if (yupiter_fgets(line, sizeof(line), yupiter_stdin) != NULL)
    {
        yupiter_printf("line=%s", line);
    }

    /* fread */
    got = yupiter_fread(buf, 1, sizeof(buf), yupiter_stdin);
    yupiter_printf("fread=%u\n", (unsigned)got);

    /* EOF */
    while (yupiter_getchar() != YUPITER_EOF)
    {
    }
    yupiter_printf("eof=%d err=%d\n", yupiter_feof(yupiter_stdin),
                   yupiter_ferror(yupiter_stdin));

    return 0;
}