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
 * The six-argument system-call trampoline for Linux on x86_64.
 *
 * Written in GNU assembler (GAS) syntax, as required by the project
 * conventions.  The Linux system-call ABI for x86_64 differs from the
 * ordinary C calling convention in more than one way: the system-call
 * number is carried in rax rather than rdi, the result is returned in
 * rax, the fourth argument goes in r10 because the "syscall"
 * instruction destroys rcx - the register the C convention uses for
 * that argument - and the sixth argument is passed on the stack.  The
 * trampoline therefore has to permute the argument registers as well
 * as move the number into place.
 *
 * The six-argument entry point remains for a future call that needs
 * more registers.  The three-argument entry point that read, write,
 * close and exit use is in syscall3_x86_64.S: it avoids the fourth,
 * fifth and sixth argument moves, including the load from the stack,
 * on every system call.
 *
 * The C convention allows a function to destroy rax, rcx, rdx, rsi,
 * rdi, r8, r9, r10 and r11; this function touches nothing else, so no
 * explicit save or restore is needed.
 */

    .text
    .align 4

/*
 * long yupiter_syscall(long number, long arg1, long arg2, long arg3,
 *                    long arg4, long arg5, long arg6);
 *
 * On entry:  rdi = number, rsi, rdx, rcx, r8 and r9 = the arguments
 *            one to five, the sixth at 8(%rsp).
 * On exit:   rax = raw kernel result.
 */
    .global yupiter_syscall
    .hidden yupiter_syscall
    .type   yupiter_syscall, @function
yupiter_syscall:
    movq    %r8, %r10               /* arg4 goes in r10, not rcx      */
    movq    %r9, %r8                /* arg5 goes in r8                */
    movq    8(%rsp), %r9            /* arg6 comes from the stack      */

    movq    %rdi, %rax              /* system-call number goes in rax */
    movq    %rsi, %rdi              /* shift the arguments into rdi,  */
    movq    %rdx, %rsi              /* rsi, rdx and r10               */
    movq    %rcx, %rdx
    syscall                         /* invoke the kernel              */
    ret                             /* result is already in rax       */
    .size   yupiter_syscall, .-yupiter_syscall

/*
 * Mark the stack as non-executable when the section is present.
 */
    .section .note.GNU-stack,"",@progbits
