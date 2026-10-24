# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

"""Test the yupiter Python extension module.

Build and install the wheel before running the test; the module is
imported from the active Python installation, and ctypes is used only to
inspect the native pointers it returns.

Run with: python test/test_python_binding.py
"""
import ctypes
import subprocess
import sys

import yupiter


def _run_python(code):
    """Run binding calls in a child with captured output and a limit."""
    return subprocess.run(
        [sys.executable, "-c", "import yupiter; " + code],
        capture_output=True,
        text=True,
        timeout=10,
    )


def _expect_error(error_type, callable_):
    """Return the expected exception message, failing if the call succeeds."""
    try:
        callable_()
    except error_type as exc:
        return str(exc)
    raise AssertionError(f"expected {error_type.__name__}")


# --- Module and constants ---


def test_import():
    assert yupiter is not None
    assert hasattr(yupiter, "yupiter_printf")
    assert hasattr(yupiter, "yupiter_malloc")
    assert hasattr(yupiter, "yupiter_malloc_safe")
    assert hasattr(yupiter, "yupiter_free_safe")
    assert hasattr(yupiter, "yupiter_realloc_safe")


def test_module_all():
    assert yupiter.__all__ == [
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


def test_stream_constants():
    assert isinstance(yupiter.STDOUT, int)
    assert isinstance(yupiter.STDERR, int)
    assert isinstance(yupiter.STDIN, int)
    assert yupiter.STDOUT > 0
    assert yupiter.STDERR > 0
    assert yupiter.STDIN > 0
    assert len({yupiter.STDOUT, yupiter.STDERR, yupiter.STDIN}) == 3


# --- Output functions ---


def test_printf():
    result = yupiter.yupiter_printf("hello test\n")
    assert result is not None
    assert result > 0


def test_printf_empty():
    assert yupiter.yupiter_printf("") == 0


def test_printf_return_count():
    text = "abc"
    assert yupiter.yupiter_printf(text + "\n") == len(text) + 1


def test_fprintf_stdout():
    result = yupiter.yupiter_fprintf(yupiter.STDOUT, "fprintf stdout\n")
    assert result is not None
    assert result > 0


def test_fprintf_stderr():
    result = yupiter.yupiter_fprintf(yupiter.STDERR, "fprintf stderr\n")
    assert result is not None
    assert result > 0


def test_fprintf_empty():
    assert yupiter.yupiter_fprintf(yupiter.STDOUT, "") == 0
    assert yupiter.yupiter_fprintf(yupiter.STDERR, "") == 0


def test_fprintf_return_count():
    text = "xyz"
    assert yupiter.yupiter_fprintf(yupiter.STDOUT, text) == len(text)


# --- Basic memory functions ---


def test_malloc_free():
    ptr = yupiter.yupiter_malloc(64)
    assert isinstance(ptr, int)
    assert ptr > 0
    yupiter.yupiter_free(ptr)


def test_malloc_zero():
    assert yupiter.yupiter_malloc(0) is None


def test_malloc_alignment():
    ptr = yupiter.yupiter_malloc(1)
    assert ptr is not None
    assert ptr % 8 == 0
    yupiter.yupiter_free(ptr)


def test_malloc_multiple_distinct():
    first = yupiter.yupiter_malloc(16)
    second = yupiter.yupiter_malloc(16)
    assert first is not None
    assert second is not None
    assert first != second
    yupiter.yupiter_free(second)
    yupiter.yupiter_free(first)


def test_free_null():
    yupiter.yupiter_free(0)


def test_free_invalid_ignored():
    yupiter.yupiter_free(12345)
    ptr = yupiter.yupiter_malloc(16)
    assert ptr is not None
    yupiter.yupiter_free(ptr + 1)  # interior pointer is ignored
    yupiter.yupiter_free(ptr)


def test_realloc():
    ptr = yupiter.yupiter_malloc(32)
    assert ptr > 0
    new_ptr = yupiter.yupiter_realloc(ptr, 64)
    assert new_ptr is not None
    assert new_ptr > 0
    yupiter.yupiter_free(new_ptr)


def test_realloc_null_pointer():
    ptr = yupiter.yupiter_realloc(0, 16)
    assert ptr is not None
    assert ptr > 0
    yupiter.yupiter_free(ptr)


def test_realloc_to_zero():
    ptr = yupiter.yupiter_malloc(8)
    assert ptr is not None
    assert yupiter.yupiter_realloc(ptr, 0) is None


def test_realloc_zero_from_null():
    assert yupiter.yupiter_realloc(0, 0) is None


def test_realloc_invalid_pointer():
    assert yupiter.yupiter_realloc(12345, 8) is None


def test_realloc_preserves_contents():
    ptr = yupiter.yupiter_malloc(16)
    assert ptr is not None
    ctypes.memmove(ptr, b"HELLO", 5)
    new_ptr = yupiter.yupiter_realloc(ptr, 32)
    assert new_ptr is not None
    assert new_ptr > 0
    assert ctypes.string_at(new_ptr, 5) == b"HELLO"
    yupiter.yupiter_free(new_ptr)


def test_zero_allocation():
    ptr = yupiter.yupiter_malloc(0)
    assert ptr is None


# --- Safe memory functions ---


def test_malloc_safe():
    ptr = yupiter.yupiter_malloc_safe(2, 4)
    assert ptr > 0
    yupiter.yupiter_free_safe(ptr)


def test_malloc_safe_zero():
    assert yupiter.yupiter_malloc_safe(0, 4) == 0
    assert yupiter.yupiter_malloc_safe(2, 0) == 0


def test_malloc_safe_zero_filled():
    size = 16
    ptr = yupiter.yupiter_malloc_safe(4, 4)
    assert ptr > 0
    assert ptr % 8 == 0
    assert ctypes.string_at(ptr, size) == b"\x00" * size
    yupiter.yupiter_free_safe(ptr)


def test_malloc_safe_overflow():
    """Multiplication overflow raises the standard Python OverflowError."""
    message = _expect_error(
        OverflowError,
        lambda: yupiter.yupiter_malloc_safe(1 << 63, 2),
    )
    assert "overflow" in message


def test_malloc_safe_alignment_overflow():
    """Alignment overflow raises OverflowError when the product fits."""
    size_max = ctypes.c_size_t(-1).value
    _expect_error(
        OverflowError,
        lambda: yupiter.yupiter_malloc_safe(1, size_max),
    )


def test_malloc_safe_oom():
    """Exhaustion raises MemoryError and leaves the allocator usable."""
    _expect_error(MemoryError, lambda: yupiter.yupiter_malloc_safe(1, 100000))
    test_malloc_safe()


def test_free_safe_null():
    yupiter.yupiter_free_safe(0)


def test_free_safe_valid():
    ptr = yupiter.yupiter_malloc(8)
    assert ptr is not None
    yupiter.yupiter_free_safe(ptr)


def test_free_safe_invalid():
    """A foreign pointer raises ValueError without damaging later calls."""
    _expect_error(ValueError, lambda: yupiter.yupiter_free_safe(12345))
    test_malloc_safe()


def test_realloc_safe():
    ptr = yupiter.yupiter_malloc_safe(1, 8)
    assert ptr > 0
    new_ptr = yupiter.yupiter_realloc_safe(ptr, 2, 8)
    assert new_ptr > 0
    yupiter.yupiter_free_safe(new_ptr)


def test_realloc_safe_zero():
    assert yupiter.yupiter_realloc_safe(0, 0, 0) == 0
    ptr = yupiter.yupiter_malloc(8)
    assert ptr is not None
    assert yupiter.yupiter_realloc_safe(ptr, 0, 0) == 0


def test_realloc_safe_overflow():
    """A null-pointer resize detects overflow in count * size."""
    message = _expect_error(
        OverflowError,
        lambda: yupiter.yupiter_realloc_safe(0, 1 << 63, 2)
    )
    assert "overflow" in message


def test_realloc_safe_alignment_overflow():
    """Null and live resizes report alignment overflow without data loss."""
    size_max = ctypes.c_size_t(-1).value
    _expect_error(
        OverflowError,
        lambda: yupiter.yupiter_realloc_safe(0, 1, size_max),
    )
    ptr = yupiter.yupiter_malloc_safe(1, 8)
    ctypes.memmove(ptr, b"SURVIVED", 8)
    try:
        _expect_error(
            OverflowError,
            lambda: yupiter.yupiter_realloc_safe(ptr, 1, size_max),
        )
        assert ctypes.string_at(ptr, 8) == b"SURVIVED"
    finally:
        yupiter.yupiter_free_safe(ptr)


def test_realloc_safe_invalid():
    """Foreign pointers raise ValueError even for a zero-sized request."""
    _expect_error(
        ValueError,
        lambda: yupiter.yupiter_realloc_safe(12345, 1, 4),
    )
    _expect_error(
        ValueError,
        lambda: yupiter.yupiter_realloc_safe(12345, 0, 4),
    )
    test_malloc_safe()


def test_realloc_safe_oom():
    """A failed resize raises MemoryError and preserves the original bytes."""
    ptr = yupiter.yupiter_malloc_safe(1, 8)
    ctypes.memmove(ptr, b"SURVIVED", 8)
    try:
        _expect_error(
            MemoryError,
            lambda: yupiter.yupiter_realloc_safe(ptr, 1, 100000),
        )
        assert ctypes.string_at(ptr, 8) == b"SURVIVED"
    finally:
        yupiter.yupiter_free_safe(ptr)


def test_safe_invalid_pointers():
    """Checked calls reject interior and freed pointers without corruption."""
    ptr = yupiter.yupiter_malloc(16)
    assert ptr is not None
    _expect_error(ValueError, lambda: yupiter.yupiter_free_safe(ptr + 1))
    _expect_error(
        ValueError,
        lambda: yupiter.yupiter_realloc_safe(ptr + 1, 1, 8),
    )
    yupiter.yupiter_free_safe(ptr)
    _expect_error(ValueError, lambda: yupiter.yupiter_free_safe(ptr))
    _expect_error(
        ValueError,
        lambda: yupiter.yupiter_realloc_safe(ptr, 1, 8),
    )
    test_malloc_safe()


def test_realloc_safe_zero_product():
    """Either zero factor releases live blocks and returns 0 for null."""
    for count, size in [(0, 4), (2, 0)]:
        assert yupiter.yupiter_realloc_safe(0, count, size) == 0
        ptr = yupiter.yupiter_malloc(8)
        assert ptr is not None
        assert yupiter.yupiter_realloc_safe(ptr, count, size) == 0
        _expect_error(ValueError, lambda: yupiter.yupiter_free_safe(ptr))


def test_output_is_literal():
    """Percent directives and UTF-8 text print literally, counting bytes."""
    # Run in a child so a regression cannot crash or flood the test runner.
    text = "100% literal: %s %p %08x %n %% %*s %999999999d café\n"
    streams = [
        (None, "stdout"),
        ("STDOUT", "stdout"),
        ("STDERR", "stderr"),
    ]
    for stream, output in streams:
        if stream is None:
            call = f"yupiter.yupiter_printf({text!r})"
        else:
            call = f"yupiter.yupiter_fprintf(yupiter.{stream}, {text!r})"
        expected = len(text.encode("utf-8"))
        result = _run_python(f"assert {call} == {expected}")
        assert result.returncode == 0, result.stderr
        assert getattr(result, output) == text
        other = "stderr" if output == "stdout" else "stdout"
        assert getattr(result, other) == ""


def test_output_embedded_null():
    """Output wrappers reject embedded nulls by returning None."""
    assert yupiter.yupiter_printf("before\0after") is None
    assert yupiter.yupiter_fprintf(yupiter.STDOUT, "before\0after") is None


def test_realloc_safe_preserves_contents():
    ptr = yupiter.yupiter_malloc_safe(2, 4)
    assert ptr > 0
    ctypes.memmove(ptr, b"DATA", 4)
    new_ptr = yupiter.yupiter_realloc_safe(ptr, 4, 4)
    assert new_ptr > 0
    assert ctypes.string_at(new_ptr, 4) == b"DATA"
    yupiter.yupiter_free_safe(new_ptr)


def test_realloc_safe_from_null():
    ptr = yupiter.yupiter_realloc_safe(0, 2, 4)
    assert ptr > 0
    assert ctypes.string_at(ptr, 8) == b"\x00" * 8
    yupiter.yupiter_free_safe(ptr)


if __name__ == "__main__":
    test_import()
    test_module_all()
    test_stream_constants()
    test_printf()
    test_printf_empty()
    test_printf_return_count()
    test_fprintf_stdout()
    test_fprintf_stderr()
    test_fprintf_empty()
    test_fprintf_return_count()
    test_malloc_free()
    test_malloc_zero()
    test_malloc_alignment()
    test_malloc_multiple_distinct()
    test_free_null()
    test_free_invalid_ignored()
    test_realloc()
    test_realloc_null_pointer()
    test_realloc_to_zero()
    test_realloc_zero_from_null()
    test_realloc_invalid_pointer()
    test_realloc_preserves_contents()
    test_zero_allocation()
    test_malloc_safe()
    test_malloc_safe_zero()
    test_malloc_safe_zero_filled()
    test_malloc_safe_overflow()
    test_malloc_safe_alignment_overflow()
    test_malloc_safe_oom()
    test_free_safe_null()
    test_free_safe_valid()
    test_free_safe_invalid()
    test_realloc_safe()
    test_realloc_safe_zero()
    test_realloc_safe_overflow()
    test_realloc_safe_alignment_overflow()
    test_realloc_safe_invalid()
    test_realloc_safe_oom()
    test_realloc_safe_preserves_contents()
    test_realloc_safe_from_null()
    test_safe_invalid_pointers()
    test_realloc_safe_zero_product()
    test_output_is_literal()
    test_output_embedded_null()
    print("All tests passed.")
