/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - raw system-call helper for the short-write test
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
 * A copy of the real three-argument trampoline for one test program.
 *
 * test/shortwrite.c defines "yupiter_syscall3" itself so that it can
 * script the result of a write and prove that yupiter_sys_write repeats
 * a short write.  Every call it does not script - including the final
 * exit_group - is forwarded here, to the real kernel entry.
 *
 * Written in GNU assembler (GAS) syntax, as required by the project
 * conventions.  See assembly/yupiter_syscall3_x86_64.S for the ABI notes.
 */

    .text
    .align 4

/*
 * long yupiter_test_raw_syscall3(long number, long arg1, long arg2,
 *                              long arg3);
 *
 * On entry:  rdi = number, rsi, rdx and rcx = the arguments.
 * On exit:   rax = raw kernel result.
 */
    .global yupiter_test_raw_syscall3
    .type   yupiter_test_raw_syscall3, @function
yupiter_test_raw_syscall3:
    movq    %rdi, %rax              /* system-call number goes in rax */
    movq    %rsi, %rdi              /* shift the arguments into rdi,  */
    movq    %rdx, %rsi              /* rsi and rdx                    */
    movq    %rcx, %rdx
    syscall                         /* invoke the kernel              */
    ret                             /* result is already in rax       */
    .size   yupiter_test_raw_syscall3, .-yupiter_test_raw_syscall3

/*
 * Mark the stack as non-executable when the section is present.
 */
    .section .note.GNU-stack,"",@progbits
