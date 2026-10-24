/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

#include <stddef.h>

#include "yupiter.h"
#include "yupiter_io.hxx"

static int failures;

static void check(int condition, const char *what)
{
    if (condition == 0)
    {
        failures = failures + 1;
        yupiter_fprintf(yupiter_stderr, "FAIL: %s\n", what);
    }
}

int main(void)
{
    char buf[8];
    size_t got;
    int c;

    /* ungetc: one byte of pushback works */
    c = yupiter_getchar();
    check(c == 'X', "first getc");
    check(yupiter_ungetc(c, yupiter_stdin) == c, "ungetc returns c");
    /* second pushback without consuming the first fails */
    check(yupiter_ungetc('Y', yupiter_stdin) == YUPITER_EOF,
          "second ungetc fails");
    check(yupiter_getchar() == c, "pushed byte read back");
    /* pushing EOF fails */
    check(yupiter_ungetc(YUPITER_EOF, yupiter_stdin) == YUPITER_EOF,
          "ungetc EOF fails");

    /* fgets truncation: read into a small buffer */
    if (yupiter_fgets(buf, sizeof(buf), yupiter_stdin) != NULL)
    {
        check(buf[7] == '\0', "fgets NUL terminates");
    }

    /* fread: zero size/nmemb */
    got = yupiter_fread(buf, 0, 5, yupiter_stdin);
    check(got == 0, "fread size 0");
    got = yupiter_fread(buf, 5, 0, yupiter_stdin);
    check(got == 0, "fread nmemb 0");

    /* fread overflow: huge nmemb -> 0, stream untouched */
    got = yupiter_fread(buf, 2, (size_t)-1 / 2 + 1, yupiter_stdin);
    check(got == 0, "fread overflow returns 0");

    /* EOF vs error */
    while (yupiter_getchar() != YUPITER_EOF)
    {
    }
    check(yupiter_feof(yupiter_stdin) == 1, "feof after EOF");
    check(yupiter_ferror(yupiter_stdin) == 0, "no error on EOF");
    yupiter_clearerr(yupiter_stdin);
    check(yupiter_feof(yupiter_stdin) == 0, "clearerr resets eof");

    /* ferror on a bad descriptor */
    {
        static yupiter_FILE bad = { .fd = -1, .unbuffered = 1,
                                    .buffer = { 0 },
                                    .input_buffer = { 0 } };
        check(yupiter_fgetc(&bad) == YUPITER_EOF, "fgetc bad returns EOF");
        check(yupiter_ferror(&bad) == 1, "ferror on bad descriptor");
    }

    yupiter_printf("input_edge: %d failures\n", failures);
    return failures;
}