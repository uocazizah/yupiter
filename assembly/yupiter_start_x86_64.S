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
 * Program startup for Linux on x86_64, freestanding build.
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
 *   rsp      the top of the initial stack, aligned to 16 bytes
 *   [rsp]    argc
 *   rsp + 8  argv[0], ..., argv[argc], NULL
 *            envp[0], ..., NULL
 *
 * The System V AMD64 ABI passes the first arguments in registers and
 * requires the stack pointer to be a multiple of 16 immediately before
 * a call, so that the return address makes it 16n + 8 at the entry of
 * the callee.  The stack is aligned here rather than assumed to arrive
 * that way, in the same way the reference start-up files do it.  The
 * frame pointer is cleared so that a stack unwinder stops cleanly at
 * the bottom of the stack, and there is no return address to return
 * to, so _start must not return.
 */

    .text
    .align 4

    .global _start
    .type   _start, @function
_start:
    xorl    %ebp, %ebp              /* terminate the frame chain      */

    movl    (%rsp), %edi            /* argc                           */
    leaq    8(%rsp), %rsi           /* argv                           */

    /* envp is argv + argc + 1 pointers past the argv terminator.     */
    leaq    8(%rsi,%rdi,8), %rdx

    andq    $-16, %rsp              /* align the stack for the call   */
    call    main                    /* main(argc, argv, envp)         */

    /* main's return value is already in eax and becomes the status.  */
    movl    %eax, %edi
    call    yupiter_sys_exit
    jmp     .                       /* unreachable; guards bad kernels */
    .size   _start, .-_start

/*
 * Mark the stack as non-executable when the section is present.
 */
    .section .note.GNU-stack,"",@progbits
