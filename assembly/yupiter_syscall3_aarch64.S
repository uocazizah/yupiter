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
 * The three-argument system-call trampoline for Linux on aarch64.
 *
 * Written in GNU assembler (GAS) syntax, as required by the project
 * conventions.  The Linux system-call ABI for aarch64 differs from the
 * ordinary C calling convention: the system-call number is carried in
 * x8 rather than x0, and the result is returned in x0.  Arguments are
 * passed in the same registers (x0..x5) that the C convention uses,
 * which means the trampoline only has to shift every argument one
 * register to the right and move the number into place.
 *
 * A call that needs at most three arguments gets its own entry point;
 * shifting three arguments instead of six saves three moves on every
 * read, write, close and exit without changing the kernel ABI or the
 * portable code above this layer.  The six-argument trampoline lives
 * in syscall_aarch64.S.
 *
 * Registers x19..x30 are preserved across the call as the C calling
 * convention requires; this function touches only the argument
 * registers, the result register and the status register, so no
 * explicit save or restore is needed.
 */

    .text
    .align 2

/*
 * long yupiter_syscall3(long number, long arg1, long arg2, long arg3);
 *
 * On entry:  x0 = number, x1..x3 = arguments.
 * On exit:   x0 = raw kernel result.
 */
    .global yupiter_syscall3
    .hidden yupiter_syscall3
    .type   yupiter_syscall3, %function
yupiter_syscall3:
    mov     x8, x0          /* system-call number goes in x8          */
    mov     x0, x1          /* shift the arguments into x0 .. x2      */
    mov     x1, x2
    mov     x2, x3
    svc     #0              /* invoke the kernel                      */
    ret                     /* result is already in x0                */
    .size   yupiter_syscall3, .-yupiter_syscall3

/*
 * Mark the stack as non-executable when the section is present.
 */
    .section .note.GNU-stack,"",%progbits
