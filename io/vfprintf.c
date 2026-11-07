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
 * The formatting engine behind yupiter_printf and yupiter_fprintf.
 *
 * The parser and the formatter are kept in one translation unit on
 * purpose: the specification has to be fully known before any byte is
 * emitted, and splitting parsing from emission would force the whole
 * specification to be marshalled through a second interface.
 *
 * Everything this file needs from the platform is the stream interface
 * and the heap, declared in "yupiter_io.h" and "<yupiter.h>", so the
 * file compiles unchanged on any target that provides them.
 *
 * The scratch buffers of the formatting engine are borrowed from the
 * yupiter heap for the duration of one formatted call and fall back
 * to stack storage when the heap is exhausted; no other allocation is
 * made here.
 *
 * Unsupported in this release: floating-point conversions, positional
 * arguments ("%1$s"), the "%n" conversion and the "I" / "'" flags.
 * An unrecognised conversion emits nothing and consumes nothing:
 * consuming it would need the type of the argument, which the format
 * never names.  The behaviour of the call is undefined by the standard
 * from that point on; emitting nothing is the least surprising choice
 * here.
 *
 * "yupiter_vfprintf" carries the C linkage the internal header declares,
 * so yupiter_printf and yupiter_fprintf reach it under its unmangled name.
 */

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "yupiter_io.h"



/*
 * Flags of a conversion specification, as a bitmask.
 */
typedef enum flag
{
    FLAG_LEFT  = 0x01U,
    FLAG_PLUS  = 0x02U,
    FLAG_SPACE = 0x04U,
    FLAG_ALT   = 0x08U,
    FLAG_ZERO  = 0x10U
} flag;

/*
 * The specification keeps its flags in one unsigned word, so the two
 * operators below combine a word with a flag and test one.
 */
static bool has_flag(unsigned int flags, flag value)
{
    return (flags & (unsigned int)(value)) != 0U;
}

/*
 * Length modifiers, in the order the parser matches them.
 */
typedef enum length_modifier
{
    LENGTH_MODIFIER_NONE,
    LENGTH_MODIFIER_HH,
    LENGTH_MODIFIER_H,
    LENGTH_MODIFIER_L,
    LENGTH_MODIFIER_LL,
    LENGTH_MODIFIER_Z,
    LENGTH_MODIFIER_T,
    LENGTH_MODIFIER_J
} length_modifier;

/*
 * Worst-case digits of a uintmax_t value written in binary: 64 digits
 * plus the terminating null.  Base 8, 10 and 16 all need fewer, so the
 * buffer is always large enough regardless of the conversion.
 */
#define digit_storage 65

/*
 * Size of the chunk used to stage a run of padding characters, and
 * the size of the scratch block that carries both of these buffers.
 *
 * The scratch block comes from the yupiter heap for the duration of
 * a formatted call (see yupiter_vfprintf) so that the two arrays do not
 * have to live on the stack.  A call that cannot allocate simply uses
 * stack buffers of the same size, so an exhausted heap costs stack
 * space rather than correctness.
 */
#define scratch_chunk 32
#define scratch_size (digit_storage + scratch_chunk)

/*
 * The stream, the running character count and the sticky failure flag
 * of one formatted call.  All output helpers take this so that a
 * single failure stops the whole call without further system calls.
 *
 * "p_scratch" is the scratch_size byte block borrowed for the
 * call: the first digit_storage bytes are the digit storage of
 * output_number, the remaining scratch_chunk bytes are the
 * padding chunk of output_repeat.  The two regions are never needed
 * at the same address, so one block serves both.  A null pointer
 * means the heap had no block to spare and every helper falls back to
 * its own stack buffer.
 *
 * "p_chunk" is that padding chunk - the tail of the scratch block, or
 * the chunk_storage array in the frame of yupiter_vfprintf when the heap
 * had nothing to spare - and "chunk_value" is the character it holds,
 * so that a run of padding is filled once for the whole call instead
 * of once per conversion.  Zero is its "not filled yet" value, which
 * no padding character can be: padding is only ever a space or a
 * zero.
 */
typedef struct yupiter_output
{
    yupiter_FILE *stream;
    int         count;
    bool        failed;
    char       *p_scratch;
    char       *p_chunk;
    char        chunk_value;
    bool        scratch_from_heap;
} yupiter_output;

/*
 * One parsed conversion specification.
 */
typedef struct yupiter_spec
{
    unsigned int    flags;
    int             width;
    int             precision;
    length_modifier length;
    char            conversion;
} yupiter_spec;

/*
 * Give the borrowed scratch block back.  Every path out of
 * yupiter_vfprintf calls this, so a formatted call can never leave the
 * heap holding a block.
 */
static void output_release_scratch(yupiter_output *p_output)
{
    if (p_output->scratch_from_heap)
    {
        yupiter_free(p_output->p_scratch);
        p_output->p_scratch = NULL;
        p_output->scratch_from_heap = false;
    }
}

/*
 * Emit "length" bytes, or record the failure.  The count is clamped to
 * INT_MAX before anything is written so that the documented return
 * type can never overflow.
 */
static void output_write(yupiter_output *p_output, const char *p_bytes,
                  size_t length)
{
    if (p_output->failed)
    {
        return;
    }

    if (length == 0)
    {
        return;
    }

    if (length > (size_t)(INT_MAX - p_output->count))
    {
        p_output->failed = true;
        return;
    }

    if (yupiter_stream_write(p_output->stream, p_bytes, length) != 0)
    {
        p_output->failed = true;
        return;
    }

    p_output->count = p_output->count + (int)(length);
}

/*
 * Emit "count" copies of one character.  The bytes are staged in
 * chunks so that a wide field is a handful of system calls rather
 * than one call per byte.
 *
 * The chunk is prepared once per padding character for the whole
 * call: a conversion that pads with spaces after another one reuses
 * the spaces that are already there, and only a change of character -
 * which happens when a conversion pads with zeros - costs a refill.
 */
static void output_repeat(yupiter_output *p_output, char value, size_t count)
{
    size_t index;

    if (count == 0)
    {
        return;
    }

    if (p_output->chunk_value != value)
    {
        for (index = 0; index < scratch_chunk; index = index + 1)
        {
            p_output->p_chunk[index] = value;
        }

        p_output->chunk_value = value;
    }

    while (count >= scratch_chunk)
    {
        output_write(p_output, p_output->p_chunk, scratch_chunk);
        count = count - scratch_chunk;
    }

    if (count > 0)
    {
        output_write(p_output, p_output->p_chunk, count);
    }
}

/*
 * Emit "length" bytes of "p_text" padded to "width" columns.
 *
 * Negative or zero width means "no padding".  The '-' flag aligns to
 * the left, which turns the padding into trailing spaces.
 */
static void output_field(yupiter_output *p_output, const char *p_text,
                  size_t length, const yupiter_spec *p_spec)
{
    size_t padding;
    size_t field_width;
    bool        left;

    padding = 0;
    left = has_flag(p_spec->flags, FLAG_LEFT);

    if ((p_spec->width > 0) &&
        ((size_t)(p_spec->width) > length))
    {
        padding = (size_t)(p_spec->width) - length;
    }

    /*
     * Same guard as in output_number: a field that cannot fit in the
     * return value fails before it is written.
     */
    field_width = length + padding;

    if (field_width >= (size_t)(INT_MAX - p_output->count))
    {
        p_output->failed = true;
        return;
    }

    if (!left)
    {
        output_repeat(p_output, ' ', padding);
    }

    output_write(p_output, p_text, length);

    if (left)
    {
        output_repeat(p_output, ' ', padding);
    }
}

/*
 * Render "value" into "p_storage" from the end backwards and return
 * the address of the first digit, storing its length in "*p_length".
 *
 * The caller supplies the storage because the digit count depends on
 * the base.  At least digit_storage bytes must be available.
 * Division and modulus by 8, 10 and 16 are compiled to ordinary
 * arithmetic on aarch64, so no compiler runtime helper is required.
 *
 * A null terminator is written in front of the digits and the length
 * is reported rather than recovered by scanning: the storage is raw
 * scratch memory reused by every conversion, so counting its contents
 * would read past the digits into whatever the previous call left
 * there.
 */
static const char lower_digits[] = "0123456789abcdef";
static const char upper_digits[] = "0123456789ABCDEF";

static const char *render_integer(uintmax_t value, unsigned int base,
                                     bool uppercase, char *p_storage,
                                     size_t storage_size,
                                     size_t *p_length)
{
    const char *p_digits;
    size_t index;

    p_digits = uppercase ? upper_digits : lower_digits;
    index = storage_size;

    if (value == 0)
    {
        index = index - 1;
        p_storage[index] = '0';
    }
    else
    {
        while (value != 0)
        {
            index = index - 1;
            p_storage[index] = p_digits[value % base];
            value = value / base;
        }
    }

    index = index - 1;
    p_storage[index] = '\0';

    *p_length = (storage_size - 1) - index;

    return p_storage + index + 1;
}

/*
 * Emit an integer that has already been reduced to a magnitude and a
 * sign, in the requested base.
 *
 * The layout follows the standard: an optional sign, an optional
 * prefix ("0x" or a leading "0" for octal), the zeros demanded by the
 * precision, the digits themselves, and finally the field padding.
 * Zero padding from the '0' flag is merged with the precision zeros
 * and placed after the sign, which is what makes "%-8.4d" and
 * "%08.4d" differ only in alignment.
 */
static void output_number(yupiter_output *p_output, const yupiter_spec *p_spec,
                   uintmax_t magnitude, bool negative,
                   unsigned int base, bool uppercase)
{
    char        fallback_storage[digit_storage];
    char       *p_storage;
    char        sign;
    const char *p_digits;
    const char *p_prefix;
    size_t digit_count;
    size_t zero_count;
    size_t sign_count;
    size_t prefix_count;
    size_t total;
    size_t padding;
    size_t field_width;
    bool        left;
    bool        zero_pad;
    bool        allow_sign;

    /*
     * The digits are rendered into the head of the scratch block when
     * the heap provided one, and into a stack array of the same size
     * when it did not.  The padding chunk of output_repeat lives past
     * the end of this region, so the two never overlap.
     */
    p_storage = (p_output->p_scratch != NULL) ? p_output->p_scratch
                                                 : fallback_storage;

    if ((p_spec->precision == 0) && (magnitude == 0))
    {
        p_digits = "";
        digit_count = 0;
    }
    else
    {
        p_digits = render_integer(magnitude, base, uppercase, p_storage,
                                  digit_storage, &digit_count);
    }

    /*
     * The precision is a minimum digit count, so it only ever adds
     * leading zeros.  A negative precision means "none" and is
     * represented by -1, which never satisfies the comparison.
     */
    zero_count = 0;
    if ((p_spec->precision >= 0) &&
        ((size_t)(p_spec->precision) > digit_count))
    {
        zero_count = (size_t)(p_spec->precision) - digit_count;
    }

    /*
     * The '+' and ' ' flags are specified for the signed conversions
     * and for floating point, both of which reach this function as 'd'
     * or 'i'.  This engine also accepts them for %p, following the
     * reference C library, where the sign is placed in front of the
     * "0x" prefix: "%+p" of a pointer prints "+0x1234".  For %u, %o,
     * %x and %X the flags are ignored, so "%+u" prints the value
     * unchanged rather than prefixing it.
     *
     * A null pointer has no digits to carry a sign: the reference C
     * library prints "(nil)" for it, and so does this engine, reached
     * before this function is.
     */
    sign_count = 0;
    sign = ' ';
    allow_sign = (p_spec->conversion == 'd') ||
                 (p_spec->conversion == 'i') ||
                 (p_spec->conversion == 'p');

    if (negative)
    {
        sign = '-';
        sign_count = 1;
    }
    else if (allow_sign && has_flag(p_spec->flags, FLAG_PLUS))
    {
        sign = '+';
        sign_count = 1;
    }
    else if (allow_sign && has_flag(p_spec->flags, FLAG_SPACE))
    {
        sign = ' ';
        sign_count = 1;
    }

    /*
     * Prefixes.  "0x" is only useful for a non-zero value, and the
     * octal '#' prefix only has to appear when no zero would be
     * emitted anyway - either from the precision or from the digits.
     */
    p_prefix = "";
    prefix_count = 0;

    if ((p_spec->conversion == 'x') || (p_spec->conversion == 'X'))
    {
        if (has_flag(p_spec->flags, FLAG_ALT) && (magnitude != 0))
        {
            p_prefix = uppercase ? "0X" : "0x";
            prefix_count = 2;
        }
    }
    else if (p_spec->conversion == 'p')
    {
        p_prefix = "0x";
        prefix_count = 2;
    }
    else if (p_spec->conversion == 'o')
    {
        if (has_flag(p_spec->flags, FLAG_ALT) && (zero_count == 0) &&
            ((digit_count == 0) || (p_digits[0] != '0')))
        {
            p_prefix = "0";
            prefix_count = 1;
        }
    }

    total = sign_count + prefix_count + zero_count + digit_count;

    padding = 0;
    if ((p_spec->width > 0) &&
        ((size_t)(p_spec->width) > total))
    {
        padding = (size_t)(p_spec->width) - total;
    }

    /*
     * Reject a field that cannot fit in the documented return value
     * before a single byte of it is written.  Without this a width of
     * INT_MAX would push gigabytes at the kernel and only then fail,
     * which is both slow and useless: the call is going to report -1
     * either way.
     */
    field_width = total + padding;

    if (field_width >= (size_t)(INT_MAX - p_output->count))
    {
        p_output->failed = true;
        return;
    }

    left = has_flag(p_spec->flags, FLAG_LEFT);

    /*
     * The '0' flag pads with zeros, but only when no precision was
     * given and the field is not left-aligned.  In that case the
     * width padding and the precision zeros form a single run.
     */
    zero_pad = false;
    if (!left && has_flag(p_spec->flags, FLAG_ZERO) &&
        (p_spec->precision < 0))
    {
        zero_pad = true;
    }

    if (!left)
    {
        if (zero_pad)
        {
            zero_count = zero_count + padding;
            padding = 0;
        }
        else
        {
            output_repeat(p_output, ' ', padding);
        }
    }

    if (sign_count != 0)
    {
        output_write(p_output, &sign, sign_count);
    }

    if (prefix_count != 0)
    {
        output_write(p_output, p_prefix, prefix_count);
    }

    if (zero_count != 0)
    {
        output_repeat(p_output, '0', zero_count);
    }

    if (digit_count != 0)
    {
        output_write(p_output, p_digits, digit_count);
    }

    if (left && (padding != 0))
    {
        output_repeat(p_output, ' ', padding);
    }
}

/*
 * Read a decimal field, advancing "*pp_format" past the digits and
 * storing the value in "*p_value".
 *
 * A field wider than INT_MAX cannot be represented and the standard
 * leaves the result undefined there.  Rather than silently clamping -
 * which would turn "%9999999999d" into a plausible but enormous field
 * - the overflow is reported so that the caller can fail the whole
 * call, which is what every reference implementation examined here
 * does.  The accumulator is still clamped internally so that a run of
 * hundreds of digits cannot wrap it.
 *
 * Returns 0 on success and -1 when the digits did not fit.
 */
static int parse_decimal(const char **pp_format, int *p_value)
{
    const char   *p_cursor;
    unsigned long accumulator;
    int           overflow;

    p_cursor = *pp_format;
    accumulator = 0;
    overflow = 0;

    while ((*p_cursor >= '0') && (*p_cursor <= '9'))
    {
        accumulator = (accumulator * 10UL) +
                      (unsigned long)(*p_cursor - '0');

        if (accumulator > (unsigned long)(INT_MAX))
        {
            overflow = 1;
            accumulator = (unsigned long)(INT_MAX);
        }

        p_cursor = p_cursor + 1;
    }

    *pp_format = p_cursor;
    *p_value = (int)(accumulator);

    return (overflow != 0) ? -1 : 0;
}

static bool is_flag_character(char value)
{
    return (value == '-') || (value == '+') || (value == ' ') ||
           (value == '#') || (value == '0');
}

static bool is_digit_character(char value)
{
    return (value >= '0') && (value <= '9');
}

/*
 * Reduce a signed argument to a magnitude and a sign flag.
 *
 * "value + 1" is evaluated before the negation so that the most
 * negative value of the type does not overflow when negated.
 */
static void normalize_signed(intmax_t value,
                                uintmax_t *p_magnitude,
                                bool *p_negative)
{
    if (value < 0)
    {
        *p_negative = true;
        *p_magnitude = (uintmax_t)(-(value + 1)) +
                       (uintmax_t)(1);
    }
    else
    {
        *p_negative = false;
        *p_magnitude = (uintmax_t)(value);
    }
}

/*
 * The parser and the number renderer are useful at compile time too:
 * an implementation-defined detail such as the position of the null
 * terminator can then be checked by the compiler instead of only by a
 * run-time test.
 */









int yupiter_vfprintf(yupiter_FILE *stream, const char *format,
                              va_list args)
{
    yupiter_output   output;
    yupiter_spec     spec;
    const char    *p_format;
    char           chunk_storage[scratch_chunk];
    size_t    run_length;
    uintmax_t magnitude;
    bool           negative;
    int            value;

    if ((stream == NULL) || (format == NULL))
    {
        return -1;
    }

    output.stream = stream;
    output.count = 0;
    output.failed = false;

    /*
     * Borrow the scratch block for the whole call.  A null result is
     * not an error: the output helpers fall back to stack buffers of
     * the same size, so an exhausted heap costs stack space rather
     * than a failed conversion.
     */
    output.p_scratch = (char *)(yupiter_malloc(scratch_size));
    output.scratch_from_heap = (output.p_scratch != NULL);

    /*
     * The padding chunk lives in the borrowed block when there is one,
     * and in the frame of this call otherwise, so that it survives for
     * as long as the call does and no helper has to prepare it again.
     */
    output.p_chunk = (output.p_scratch != NULL)
                         ? output.p_scratch + digit_storage
                         : chunk_storage;
    output.chunk_value = 0;

    p_format = format;
    magnitude = 0;
    negative = false;

    while (*p_format != '\0')
    {
        if (*p_format != '%')
        {
            run_length = 0;
            while ((p_format[run_length] != '\0') &&
                   (p_format[run_length] != '%'))
            {
                run_length = run_length + 1;
            }

            output_write(&output, p_format, run_length);
            p_format = p_format + run_length;
            continue;
        }

        /* Step over the introducing '%'. */
        p_format = p_format + 1;

        if (*p_format == '\0')
        {
            break;
        }

        spec.flags = 0U;
        spec.width = 0;
        spec.precision = -1;
        spec.length = LENGTH_MODIFIER_NONE;
        spec.conversion = '\0';

        /* Flags: any order, and any number of repetitions. */
        while (is_flag_character(*p_format))
        {
            switch (*p_format)
            {
                case '-':
                    spec.flags = spec.flags | FLAG_LEFT;
                    break;
                case '+':
                    spec.flags = spec.flags | FLAG_PLUS;
                    break;
                case ' ':
                    spec.flags = spec.flags | FLAG_SPACE;
                    break;
                case '#':
                    spec.flags = spec.flags | FLAG_ALT;
                    break;
                default:
                    spec.flags = spec.flags | FLAG_ZERO;
                    break;
            }

            p_format = p_format + 1;
        }

        /* Width: either '*' (an argument) or decimal digits. */
        if (*p_format == '*')
        {
            value = va_arg(args, int);
            p_format = p_format + 1;

            if (value < 0)
            {
                /*
                 * A negative width is the '-' flag with a positive
                 * width.  INT_MIN has no positive counterpart, so it
                 * saturates rather than overflowing on negation.
                 */
                spec.flags = spec.flags | FLAG_LEFT;
                spec.width = (value == INT_MIN) ? INT_MAX : -value;
            }
            else
            {
                spec.width = value;
            }
        }
        else if (is_digit_character(*p_format))
        {
            if (parse_decimal(&p_format, &spec.width) != 0)
            {
                output_release_scratch(&output);
                return -1;
            }
        }

        /* Precision: a '.' always introduces one, empty means zero. */
        if (*p_format == '.')
        {
            p_format = p_format + 1;

            if (*p_format == '*')
            {
                value = va_arg(args, int);
                p_format = p_format + 1;

                if (value >= 0)
                {
                    spec.precision = value;
                }
            }
            else
            {
                if (parse_decimal(&p_format, &spec.precision) != 0)
                {
                    output_release_scratch(&output);
                    return -1;
                }
            }
        }

        /* Length modifier: two-character forms are matched first. */
        if (*p_format == 'h')
        {
            p_format = p_format + 1;

            if (*p_format == 'h')
            {
                spec.length = LENGTH_MODIFIER_HH;
                p_format = p_format + 1;
            }
            else
            {
                spec.length = LENGTH_MODIFIER_H;
            }
        }
        else if (*p_format == 'l')
        {
            p_format = p_format + 1;

            if (*p_format == 'l')
            {
                spec.length = LENGTH_MODIFIER_LL;
                p_format = p_format + 1;
            }
            else
            {
                spec.length = LENGTH_MODIFIER_L;
            }
        }
        else if (*p_format == 'z')
        {
            spec.length = LENGTH_MODIFIER_Z;
            p_format = p_format + 1;
        }
        else if (*p_format == 't')
        {
            spec.length = LENGTH_MODIFIER_T;
            p_format = p_format + 1;
        }
        else if (*p_format == 'j')
        {
            spec.length = LENGTH_MODIFIER_J;
            p_format = p_format + 1;
        }

        if (*p_format == '\0')
        {
            break;
        }

        spec.conversion = *p_format;
        p_format = p_format + 1;

        switch (spec.conversion)
        {
            case 'd':
            case 'i':
                /*
                 * The 'h' and 'hh' modifiers read an int because of
                 * the default argument promotions, then truncate to
                 * the narrow type exactly as the standard requires.
                 */
                switch (spec.length)
                {
                    case LENGTH_MODIFIER_HH:
                        normalize_signed(
                            (intmax_t)(
                                (signed char)(va_arg(args, int))),
                            &magnitude, &negative);
                        break;
                    case LENGTH_MODIFIER_H:
                        normalize_signed(
                            (intmax_t)(
                                (short)(va_arg(args, int))),
                            &magnitude, &negative);
                        break;
                    case LENGTH_MODIFIER_L:
                        normalize_signed(
                            (intmax_t)(va_arg(args, long)),
                            &magnitude, &negative);
                        break;
                    case LENGTH_MODIFIER_LL:
                        normalize_signed(
                            (intmax_t)(
                                va_arg(args, long long)),
                            &magnitude, &negative);
                        break;
                    case LENGTH_MODIFIER_Z:
                        normalize_signed(
                            (intmax_t)(va_arg(args, long)),
                            &magnitude, &negative);
                        break;
                    case LENGTH_MODIFIER_T:
                        normalize_signed(
                            (intmax_t)(
                                va_arg(args, ptrdiff_t)),
                            &magnitude, &negative);
                        break;
                    case LENGTH_MODIFIER_J:
                        normalize_signed(va_arg(args, intmax_t),
                                         &magnitude, &negative);
                        break;
                    default:
                        normalize_signed(
                            (intmax_t)(va_arg(args, int)),
                            &magnitude, &negative);
                        break;
                }

                spec.conversion = 'd';
                output_number(&output, &spec, magnitude, negative, 10U,
                              false);
                break;

            case 'u':
            case 'o':
            case 'x':
            case 'X':
                negative = false;

                switch (spec.length)
                {
                    case LENGTH_MODIFIER_HH:
                        magnitude = (uintmax_t)(
                            (unsigned char)(
                                va_arg(args, unsigned int)));
                        break;
                    case LENGTH_MODIFIER_H:
                        magnitude = (uintmax_t)(
                            (unsigned short)(
                                va_arg(args, unsigned int)));
                        break;
                    case LENGTH_MODIFIER_L:
                        magnitude = (uintmax_t)(
                            va_arg(args, unsigned long));
                        break;
                    case LENGTH_MODIFIER_LL:
                        magnitude = (uintmax_t)(
                            va_arg(args, unsigned long long));
                        break;
                    case LENGTH_MODIFIER_Z:
                        magnitude = (uintmax_t)(
                            va_arg(args, size_t));
                        break;
                    case LENGTH_MODIFIER_T:
                        magnitude = (uintmax_t)(
                            va_arg(args, ptrdiff_t));
                        break;
                    case LENGTH_MODIFIER_J:
                        magnitude =
                            va_arg(args, uintmax_t);
                        break;
                    default:
                        magnitude = (uintmax_t)(
                            va_arg(args, unsigned int));
                        break;
                }

                if (spec.conversion == 'u')
                {
                    output_number(&output, &spec, magnitude, false, 10U,
                                  false);
                }
                else if (spec.conversion == 'o')
                {
                    output_number(&output, &spec, magnitude, false, 8U,
                                  false);
                }
                else if (spec.conversion == 'X')
                {
                    output_number(&output, &spec, magnitude, false, 16U,
                                  true);
                }
                else
                {
                    output_number(&output, &spec, magnitude, false, 16U,
                                  false);
                }
                break;

            case 'p':
            {
                const void *p_pointer;

                p_pointer = va_arg(args, const void *);

                if (p_pointer == NULL)
                {
                    output_field(&output, "(nil)", 5, &spec);
                }
                else
                {
                    spec.conversion = 'p';
                    magnitude = (uintmax_t)(
                        (uintptr_t)(p_pointer));
                    negative = false;
                    output_number(&output, &spec, magnitude, negative,
                                  16U, false);
                }
                break;
            }

            case 'c':
            {
                char character;

                character = (char)(va_arg(args, int));
                output_field(&output, &character, 1, &spec);
                break;
            }

            case 's':
            {
                const char *p_string;
                size_t length;

                p_string = va_arg(args, const char *);

                if (p_string == NULL)
                {
                    /*
                     * Passing a null pointer to %s is undefined
                     * behaviour, but "(null)" is what every
                     * implementation a program is likely to encounter
                     * prints, so matching it keeps output portable.
                     * A precision narrower than "(null)" yields an
                     * empty field rather than a truncated one, which
                     * is the behaviour of the reference
                     * implementation.
                     */
                    if ((spec.precision >= 0) &&
                        (spec.precision <
                         (int)(sizeof "(null)" - 1)))
                    {
                        p_string = "";
                        length = 0;
                    }
                    else
                    {
                        p_string = "(null)";
                        length = sizeof "(null)" - 1;
                    }
                }
                else
                {
                    /*
                     * A precision caps the number of bytes shown, so a
                     * field narrower than the string never walks past
                     * the last byte it can show: the string is scanned
                     * up to the limit and no further.  Without a
                     * precision the whole string is measured.
                     */
                    length = 0;

                    if (spec.precision < 0)
                    {
                        length = yupiter_strlen(p_string);
                    }
                    else
                    {
                        while ((length <
                                (size_t)(spec.precision)) &&
                               (p_string[length] != '\0'))
                        {
                            length = length + 1;
                        }
                    }
                }

                output_field(&output, p_string, length, &spec);
                break;
            }

            case '%':
                output_write(&output, "%", 1);
                break;

            default:
                /* Unsupported conversion: consume nothing, emit nothing. */
                break;
        }
    }

    output_release_scratch(&output);

    if (output.failed)
    {
        return -1;
    }

    return output.count;
}

