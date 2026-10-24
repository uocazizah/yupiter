/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - differential formatting test
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
 * The same source is compiled twice: once with -DYUPITER_REFERENCE, when
 * the calls go to the host C library, and once linked against yupiter.
 * CompareTwo.cmake runs both programs and compares their output byte
 * for byte, so a divergence is reported as a failing test on the spot.
 *
 * The golden file of test/format.c is a snapshot taken once, from a
 * reference library that is not present at test time.  This program
 * complements it by keeping a reference library in the comparison: the
 * cases are built at run time, so every combination of flags, width and
 * precision is covered without the file growing with them, and the
 * reference the output is compared against is a live one rather than a
 * file someone could regenerate from yupiter by mistake.
 *
 * Every case prints the value the call returned as well as what it
 * wrote, so the return-count contract is compared too.  The program
 * writes no semicolon and no null byte, which is what lets the
 * comparison report the first differing line rather than only that the
 * two outputs are not equal.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef YUPITER_REFERENCE
#include <stdio.h>
#define PRINT printf
#else
#include <yupiter.h>
#define PRINT yupiter_printf
#endif

/*
 * Calls whose format string deliberately exercises behaviour the C
 * standard leaves undefined go through this pointer, so the compiler's
 * format check does not reject the comparison.
 */
typedef int (*print_function)(const char *, ...);
static print_function volatile print_unchecked = PRINT;

#define PRINT_UNDEFINED(...) print_unchecked(__VA_ARGS__)

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/*
 * A specification with the largest literal width and precision that the
 * sweep builds, plus the brackets, the newline and the terminator.
 */
#define SPEC_BUFFER_SIZE 32

static const char *const flag_sets[] =
{
    "", "+", " ", "-", "-+", "+-", "0", "0+", "#", "#+", "0-", "-0",
    " + ", "+#"
};

static const char *const width_sets[] =
{
    "", "1", "8", "20"
};

static const char *const precision_sets[] =
{
    "", ".0", ".3", ".16"
};

/*
 * Report the value the previous call returned.  Printing it keeps the
 * return contract inside the comparison.
 */
static void report(int status)
{
    PRINT("return=%d\n", status);
}

static size_t append_text(char *p_buffer, size_t position, const char *p_text)
{
    while (*p_text != '\0')
    {
        p_buffer[position] = *p_text;
        position = position + 1;
        p_text = p_text + 1;
    }

    return position;
}

/*
 * Build one specification into "p_buffer" as "[%<flags><width>
 * <precision><conversion>]" followed by a newline.
 */
static void build_spec(char *p_buffer, const char *p_flags,
                       const char *p_width, const char *p_precision,
                       char conversion)
{
    size_t position;

    position = 0;
    p_buffer[position] = '[';
    position = position + 1;
    p_buffer[position] = '%';
    position = position + 1;

    position = append_text(p_buffer, position, p_flags);
    position = append_text(p_buffer, position, p_width);
    position = append_text(p_buffer, position, p_precision);

    p_buffer[position] = conversion;
    position = position + 1;
    p_buffer[position] = ']';
    position = position + 1;
    p_buffer[position] = '\n';
    position = position + 1;
    p_buffer[position] = '\0';
}

/*
 * Every flag, width and precision against the signed conversions, the
 * character conversion and the unsigned ones.
 *
 * The same specification is used for every conversion, so the sweep
 * covers combinations no one would write by hand - a precision on %c,
 * the '#' flag on %d, a zero fill before a left alignment - which is
 * exactly where two implementations are most likely to disagree.
 */
static void sweep_conversions(void)
{
    char   spec[SPEC_BUFFER_SIZE];
    size_t f;
    size_t w;
    size_t p;

    for (f = 0; f < COUNT_OF(flag_sets); f = f + 1)
    {
        for (w = 0; w < COUNT_OF(width_sets); w = w + 1)
        {
            for (p = 0; p < COUNT_OF(precision_sets); p = p + 1)
            {
                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 'd');
                report(PRINT(spec, -42));

                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 'i');
                report(PRINT(spec, 7));

                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 'u');
                report(PRINT(spec, 4294967295u));

                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 'o');
                report(PRINT(spec, 8u));

                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 'x');
                report(PRINT(spec, 255u));

                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 'X');
                report(PRINT(spec, 255u));

                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 'c');
                report(PRINT(spec, 'A'));

                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 's');
                report(PRINT(spec, "hi"));

                build_spec(spec, flag_sets[f], width_sets[w],
                           precision_sets[p], 'p');
                report(PRINT(spec, (void *)0x1234));
            }
        }
    }
}

/*
 * The cases the sweep cannot build: the length modifiers, the width and
 * precision taken from an argument, a null string, a null pointer and a
 * format with no conversion at all.
 */
static void literal_cases(void)
{
    signed char  value_char;
    short        value_short;
    long         value_long;
    long long    value_llong;
    size_t       value_size;
    ptrdiff_t    value_difference;
    intmax_t     value_widest;

    value_char = -5;
    value_short = -300;
    value_long = -123456789L;
    value_llong = -1234567890123LL;
    value_size = 42u;
    value_difference = -42;
    value_widest = -42;

    report(PRINT("[%hhd]\n", value_char));
    report(PRINT("[%hhu]\n", (unsigned char)200u));
    report(PRINT("[%hd]\n", value_short));
    report(PRINT("[%hu]\n", (unsigned short)65535u));
    report(PRINT("[%ld]\n", value_long));
    report(PRINT("[%lu]\n", 4000000000UL));
    report(PRINT("[%lld]\n", value_llong));
    report(PRINT("[%llu][%llo][%llx][%llX]\n",
                 18446744073709551615ULL, 8ULL, 255ULL, 255ULL));
    report(PRINT("[%zd][%zu]\n", value_size, value_size));
    report(PRINT("[%td]\n", value_difference));
    report(PRINT("[%jd]\n", value_widest));

    /* Width and precision taken from the argument list. */
    report(PRINT("[%*d]\n", 8, 42));
    report(PRINT("[%*d]\n", -8, 42));
    report(PRINT("[%*d]\n", 0, 42));
    report(PRINT("[%.*s]\n", 3, "hello"));
    report(PRINT("[%.*d]\n", -1, 42));
    report(PRINT("[%*.*d]\n", 8, 3, -42));

    /* Null strings and null pointers. */
    report(PRINT_UNDEFINED("[%s]\n", (const char *)NULL));
    report(PRINT_UNDEFINED("[%.2s]\n", (const char *)NULL));
    report(PRINT_UNDEFINED("[%.9s]\n", (const char *)NULL));
    report(PRINT_UNDEFINED("[%10s]\n", (const char *)NULL));
    report(PRINT("[%p]\n", (void *)NULL));
    report(PRINT("[%20p]\n", (void *)NULL));

    /* Signs on a pointer, which the reference library prints. */
    report(PRINT_UNDEFINED("[%+p]\n", (void *)0x1234));
    report(PRINT_UNDEFINED("[% p]\n", (void *)0x1234));

    /* No conversion at all, and the literal percent. */
    report(PRINT("no conversions here\n"));
    report(PRINT("[%%][%d]\n", 1));
    report(PRINT_UNDEFINED(""));
}

int main(void)
{
    sweep_conversions();
    literal_cases();

    return 0;
}
