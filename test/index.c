/* SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0 */

/*
 * yupiter - smoke test
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
 * The original smoke test of this repository, kept as it was written.
 * Its output is compared with test/expected-index.txt, and
 * test/index.cxx compiles the same program as C++ to prove that the
 * public header works from both languages.
 */

#include <yupiter.h>

int main(void)
{
    const char *name = "Azizah";
    int age = 15;

    yupiter_printf("Hello!\n");
    yupiter_printf("my name is %s\n", name);
    yupiter_printf("I'm %d y/o\n", age);

    return 0;
}
