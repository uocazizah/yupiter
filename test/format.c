/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - formatting engine test
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
 * Output of every supported conversion, flag, field and length
 * modifier, together with the value each call reports.
 *
 * The golden file was produced from this very source by a reference C
 * library rather than by yupiter itself, so the comparison is a real
 * check and not a snapshot of whatever the implementation happens to
 * do.  Regenerate it with a hosted compiler:
 *
 *     cc -DYUPITER_REFERENCE -w test/format.c -o reference
 *     ./reference > test/expected-format.txt
 *
 * Only conversions yupiter implements appear here; floating point and
 * positional arguments are deliberately absent.
 *
 * A good part of this file pins down conversions that the reference C
 * library accepts but the C standard leaves undefined - the '0' flag
 * with a string, a precision on %p, a '+' on an unsigned conversion, a
 * '#' on %d.  The compiler's format check is stricter than the
 * interface and would reject the very cases under test, so those calls
 * are made through a function pointer: the attribute belongs to the
 * declared function, not to the pointer type, and the rest of the file
 * keeps the compile-time check.  The cases whose reason is not obvious
 * say so where they are.
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#ifdef YUPITER_REFERENCE
#include <stdio.h>
#define PRINTF printf
#else
#include <yupiter.h>
#define PRINTF yupiter_printf
#endif

/*
 * Calls whose format string deliberately exercises behaviour the C
 * standard leaves undefined go through this pointer.  "PRINTF" keeps
 * the compiler's argument check for every defined conversion.
 */
typedef int (*print_function)(const char *, ...);
static print_function volatile print_unchecked = PRINTF;

#define PRINTF_UNDEFINED(...) print_unchecked(__VA_ARGS__)

/*
 * Report the value the previous call returned.  Printing it keeps the
 * return contract covered by the golden comparison.
 */
static void report(int status)
{
    PRINTF("return=%d\n", status);
}

int main(void)
{
    /* Signed and unsigned decimal, octal and hexadecimal. */
    report(PRINTF("[%d]\n", 42));
    report(PRINTF("[%i]\n", -42));
    report(PRINTF("[%d]\n", 0));
    report(PRINTF("[%d]\n", INT_MIN));
    report(PRINTF("[%u]\n", 0u));
    report(PRINTF("[%u]\n", UINT_MAX));
    report(PRINTF("[%o]\n", 8u));
    report(PRINTF("[%o]\n", UINT_MAX));
    report(PRINTF("[%x]\n", 255u));
    report(PRINTF("[%X]\n", 255u));
    report(PRINTF("[%x]\n", UINT_MAX));

    /* Characters and strings. */
    report(PRINTF("[%c]\n", 'A'));
    report(PRINTF("[%5c]\n", 'A'));
    report(PRINTF("[%-5c]\n", 'A'));
    report(PRINTF("[%s]\n", "hello"));
    report(PRINTF("[%5s]\n", "hi"));
    report(PRINTF("[%-5s]\n", "hi"));
    report(PRINTF_UNDEFINED("[%05s]\n", "hi"));
    report(PRINTF("[%.2s]\n", "hello"));
    report(PRINTF("[%-10.2s]\n", "hello"));
    report(PRINTF("[%10s]\n", "yupiter"));

    /* A null string argument, with and without a precision. */
    report(PRINTF_UNDEFINED("[%s]\n", NULL));
    report(PRINTF_UNDEFINED("[%.2s]\n", NULL));
    report(PRINTF_UNDEFINED("[%.6s]\n", NULL));
    report(PRINTF_UNDEFINED("[%.10s]\n", NULL));
    report(PRINTF_UNDEFINED("[%10s]\n", NULL));

    /* Percent sign and null pointer. */
    report(PRINTF("[%%]\n"));
    report(PRINTF("[%p]\n", NULL));
    report(PRINTF("[%20p]\n", NULL));
    report(PRINTF("[%-20p]\n", NULL));
    report(PRINTF_UNDEFINED("[%.3p]\n", NULL));

    /*
     * The sign flags on a pointer, which the reference C library prints
     * in front of the "0x" prefix and which this library reproduces.  A
     * null pointer has no digits to carry a sign and stays "(nil)".
     *
     * "-Wformat" rejects '+' and ' ' on a pointer conversion, because
     * the C standard does not define them there, so these calls use
     * PRINTF_UNDEFINED.
     */
    report(PRINTF_UNDEFINED("[%+p]\n", (void *)0x1234));
    report(PRINTF_UNDEFINED("[% p]\n", (void *)0x1234));
    report(PRINTF_UNDEFINED("[%+32p]\n", (void *)0x1234));
    report(PRINTF_UNDEFINED("[%+-32p]\n", (void *)0x1234));
    report(PRINTF_UNDEFINED("[%+p]\n", NULL));
    report(PRINTF_UNDEFINED("[% p]\n", NULL));

    /* Sign flags, which only ever apply to signed conversions. */
    report(PRINTF("[%+d]\n", 42));
    report(PRINTF("[% d]\n", 42));
    report(PRINTF("[%+d]\n", -42));
    report(PRINTF("[%+5d]\n", 42));
    report(PRINTF("[%+08d]\n", 42));
    report(PRINTF("[% 08d]\n", 42));
    report(PRINTF("[%08d]\n", -42));
    report(PRINTF("[%-+8.4d]\n", 42));
    report(PRINTF_UNDEFINED("[%+u]\n", 5u));
    report(PRINTF_UNDEFINED("[% u]\n", 5u));
    report(PRINTF_UNDEFINED("[%+x]\n", 255u));
    report(PRINTF_UNDEFINED("[%+X]\n", 255u));

    /* The alternate form. */
    report(PRINTF("[%#x]\n", 255u));
    report(PRINTF("[%#X]\n", 255u));
    report(PRINTF("[%#x]\n", 0u));
    report(PRINTF("[%#.3x]\n", 0u));
    report(PRINTF("[%#.2x]\n", 10u));
    report(PRINTF("[%#5.3x]\n", 255u));
    report(PRINTF("[%#o]\n", 8u));
    report(PRINTF("[%#o]\n", 64u));
    report(PRINTF("[%#o]\n", 0u));
    report(PRINTF("[%#.0o]\n", 0u));
    report(PRINTF("[%#.1o]\n", 0u));
    report(PRINTF("[%#.4o]\n", 8u));
    report(PRINTF("[%#5o]\n", 8u));
    report(PRINTF_UNDEFINED("[%#d]\n", 42));

    /* Width and precision. */
    report(PRINTF("[%5d]\n", 42));
    report(PRINTF("[%-5d]\n", 42));
    report(PRINTF("[%05d]\n", 42));
    report(PRINTF("[%.5d]\n", 42));
    report(PRINTF("[%.0d]\n", 0));
    report(PRINTF("[%+.0d]\n", 0));
    report(PRINTF("[% .0d]\n", 0));
    report(PRINTF("[%5.0d]\n", 0));
    report(PRINTF("[%.2d]\n", 0));
    report(PRINTF("[%.2d]\n", 100));
    report(PRINTF_UNDEFINED("[%08.4d]\n", 42));
    report(PRINTF_UNDEFINED("[%08.4d]\n", -42));
    report(PRINTF("[%-8.4d]\n", 42));
    report(PRINTF("[%8.4d]\n", -42));
    report(PRINTF("[%5.3d]\n", -42));
    report(PRINTF_UNDEFINED("[%05.3d]\n", 0));
    report(PRINTF_UNDEFINED("[%0-8.4d]\n", 42));

    /* Width and precision supplied as arguments. */
    report(PRINTF("[%*d|]\n", -5, 42));
    report(PRINTF("[%*d|]\n", 8, 42));
    report(PRINTF("[%*d|]\n", 0, 42));
    report(PRINTF("[%.*s|]\n", 3, "hello"));
    report(PRINTF("[%.*d|]\n", -1, 42));
    report(PRINTF("[%*.*d|]\n", 8, 3, -42));
    report(PRINTF("[%*.*d|]\n", -8, 3, 42));

    /* Length modifiers, including the narrow ones that truncate. */
    report(PRINTF("[%hhd]\n", 300));
    report(PRINTF("[%hhu]\n", -1));
    report(PRINTF("[%hd]\n", 70000));
    report(PRINTF("[%hu]\n", -1));
    report(PRINTF("[%ld]\n", -123456789L));
    report(PRINTF("[%lu]\n", 123456789UL));
    report(PRINTF("[%lld]\n", LLONG_MIN));
    report(PRINTF("[%llu]\n", ULLONG_MAX));
    report(PRINTF("[%llx]\n", ULLONG_MAX));
    report(PRINTF("[%llx]\n", 0xdeadbeefcafeULL));
    report(PRINTF("[%zu]\n", (size_t)-1));
    report(PRINTF("[%td]\n", (ptrdiff_t)-1));
    report(PRINTF("[%jd]\n", (intmax_t)INTMAX_MIN));
    report(PRINTF("[%ju]\n", (uintmax_t)UINTMAX_MAX));

    /* Several conversions in one call, and a run of plain text. */
    report(PRINTF("%s is %d years old\n", "Azizah", 15));
    report(PRINTF("plain text without conversions at all\n"));
    report(PRINTF("mixed %c %s %d %u %x %X %o %p %% %5.2d %-5.2s|]\n",
                  'x', "str", -1, 1u, 4095u, 4095u, 64u, NULL, 7, "ab"));

    return 0;
}
