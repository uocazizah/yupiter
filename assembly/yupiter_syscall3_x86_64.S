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
 * The three-argument system-call trampoline for Linux on x86_64.
 *
 * Written in GNU assembler (GAS) syntax, as required by the project
 * conventions.  The Linux system-call ABI differs from the C calling
 * convention: the number goes in rax rather than rdi, the result comes
 * back in rax, and the fourth argument would go in r10 because
 * "syscall" destroys rcx.  A call that needs at most three arguments
 * never has to touch the fourth register, so it gets its own entry
 * point; that avoids the fourth, fifth and sixth argument moves,
 * including the load from the stack, on every read, write, close and
 * exit.  The six-argument trampoline lives in syscall_x86_64.S.
 *
 * A separate object also keeps the code the library actually uses free
 * of the stack argument that only the six-argument form needs.
 *
 * The C convention allows a function to destroy rax, rcx, rdx, rsi,
 * rdi, r8, r9, r10 and r11; this function touches nothing else, so no
 * explicit save or restore is needed.
 */

    .text
    .align 4

/*
 * long yupiter_syscall3(long number, long arg1, long arg2, long arg3);
 *
 * On entry:  rdi = number, rsi, rdx and rcx = the arguments.
 * On exit:   rax = raw kernel result.
 */
    .global yupiter_syscall3
    .hidden yupiter_syscall3
    .type   yupiter_syscall3, @function
yupiter_syscall3:
    movq    %rdi, %rax              /* system-call number goes in rax */
    movq    %rsi, %rdi              /* shift the arguments into rdi,  */
    movq    %rdx, %rsi              /* rsi and rdx                    */
    movq    %rcx, %rdx
    syscall                         /* invoke the kernel              */
    ret                             /* result is already in rax       */
    .size   yupiter_syscall3, .-yupiter_syscall3

/*
 * Mark the stack as non-executable when the section is present.
 */
    .section .note.GNU-stack,"",@progbits
