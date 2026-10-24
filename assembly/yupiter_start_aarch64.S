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
 * Program startup for Linux on aarch64, freestanding build.
 *
 * This replaces the C runtime startup files that a hosted toolchain
 * would normally supply, which is what makes a "-nostdlib" link
 * possible.  There is deliberately no more here than the ABI requires:
 * no constructors, no ".init_array" walk, no thread-local storage and
 * no atexit handling.  Those facilities are documented as unsupported
 * by this release.
 *
 * The kernel enters _start with:
 *
 *   sp      the top of the initial stack
 *   [sp]    argc
 *   sp + 8  argv[0], ..., argv[argc], NULL
 *           envp[0], ..., NULL
 *
 * x29 is the frame pointer and x30 the link register; both are cleared
 * so that a stack unwinder stops cleanly at the bottom of the stack.
 */

    .text
    .align 2

    .global _start
    .type   _start, %function
_start:
    mov     x29, xzr                /* terminate the frame chain      */
    mov     x30, xzr                /* no return address              */

    ldr     x0, [sp]                /* argc                           */
    add     x1, sp, #8              /* argv                           */

    /* envp is argv + argc + 1 pointers past the argv terminator.     */
    add     x2, x1, x0, lsl #3
    add     x2, x2, #8

    bl      main                    /* main(argc, argv, envp)         */

    /*
     * main's return value is already in x0 and becomes the status.
     * yupiter_sys_exit never returns, so this is a tail branch rather
     * than a call: the stack stays 16-byte aligned and x30 still points
     * into this file, which is a harmless guard if a kernel ever did
     * return from the exit system call.
     */
    b       yupiter_sys_exit
    .size   _start, .-_start

/*
 * Mark the stack as non-executable when the section is present.
 */
    .section .note.GNU-stack,"",%progbits
