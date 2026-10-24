# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

"""Re-export the yupiter extension module under a package name."""

from yupiter import (
    yupiter_printf,
    yupiter_fprintf,
    yupiter_malloc,
    yupiter_free,
    yupiter_realloc,
    yupiter_malloc_safe,
    yupiter_free_safe,
    yupiter_realloc_safe,
    STDOUT,
    STDERR,
    STDIN,
)

__all__ = [
    "yupiter_printf",
    "yupiter_fprintf",
    "yupiter_malloc",
    "yupiter_free",
    "yupiter_realloc",
    "yupiter_malloc_safe",
    "yupiter_free_safe",
    "yupiter_realloc_safe",
    "STDOUT",
    "STDERR",
    "STDIN",
]
