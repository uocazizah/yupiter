/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

#include <stddef.h>

#include "yupiter.h"

int main(void)
{
    static char buf[16384];
    size_t got;
    size_t i;

    got = yupiter_fread(buf, 1, 8190, yupiter_stdin);

    if (got != 8190)
    {
        yupiter_fprintf(yupiter_stderr, "FAIL: got %u not 8192\n",
                        (unsigned)got);
        return 1;
    }

    for (i = 0; i < 8190; i = i + 1)
    {
        if (buf[i] != (char)('0' + (i % 10)))
        {
            yupiter_fprintf(yupiter_stderr, "FAIL: byte %u wrong\n",
                            (unsigned)i);
            return 1;
        }
    }

    yupiter_printf("input_large: ok\n");
    return 0;
}