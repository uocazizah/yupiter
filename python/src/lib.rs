// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
// Copyright (C) 2026 Uoc Azizah

//! PyO3 bindings for the yupiter C17 interface.
//!
//! This module is a thin wrapper around the shared object.  It does not
//! reimplement allocation or formatting: every operation goes through the
//! C symbols declared below.  Pointer values are exposed as integers
//! because the layout of `yupiter_FILE` is private to the library.

use pyo3::exceptions::{PyMemoryError, PyOverflowError, PyValueError};
use pyo3::prelude::*;
use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_void};

/* Values of the yupiter_memory_status enumeration in <yupiter.h>. */
const MEMORY_OK: c_int = 0;
const MEMORY_INVALID_POINTER: c_int = 1;
const MEMORY_OUT_OF_MEMORY: c_int = 2;
const MEMORY_OVERFLOW: c_int = 3;

/* The C interface.  `link_name` keeps the Rust names readable while the
 * linker sees the unmangled yupiter symbols. */
extern "C" {
    /// Write a formatted string to standard output.
    ///
    /// The variadic arguments must match the format string.  The binding
    /// always passes a constant "%s" format and a preformatted argument.
    #[link_name = "yupiter_printf"]
    fn ffi_printf(format: *const c_char, ...) -> c_int;

    /// Write a formatted string to a stream.
    #[link_name = "yupiter_fprintf"]
    fn ffi_fprintf(stream: *mut c_void, format: *const c_char, ...) -> c_int;

    /// Allocate `size` uninitialised bytes.
    #[link_name = "yupiter_malloc"]
    fn ffi_malloc(size: usize) -> *mut c_void;

    /// Release a block obtained from one of the allocation functions.
    #[link_name = "yupiter_free"]
    fn ffi_free(ptr: *mut c_void);

    /// Resize a live block, preserving its contents.
    #[link_name = "yupiter_realloc"]
    fn ffi_realloc(ptr: *mut c_void, size: usize) -> *mut c_void;

    /// Release a block and return a status instead of terminating.
    #[link_name = "yupiter_free_checked"]
    fn ffi_free_checked(ptr: *mut c_void) -> c_int;

    /// Resize a block with explicit result storage and status.
    #[link_name = "yupiter_realloc_checked"]
    fn ffi_realloc_checked(ptr: *mut c_void, size: usize, result: *mut *mut c_void) -> c_int;

    /// The standard output stream exported by the library.
    #[link_name = "yupiter_stdout"]
    static ffi_stdout: *mut c_void;

    /// The standard error stream exported by the library.
    #[link_name = "yupiter_stderr"]
    static ffi_stderr: *mut c_void;

    /// The standard input stream exported by the library.
    #[link_name = "yupiter_stdin"]
    static ffi_stdin: *mut c_void;
}

// --- Output functions ---

/// Print text literally to standard output.
///
/// Returns the number of bytes written, or `None` when the text contains
/// an embedded null byte or the underlying write fails.
#[pyfunction]
fn yupiter_printf(format: &str) -> Option<i32> {
    let c_str = CString::new(format).ok()?;

    // SAFETY: both pointers refer to NUL-terminated C strings that stay
    // alive for the duration of the call, and "%s" matches the argument.
    let result = unsafe { ffi_printf(c"%s".as_ptr(), c_str.as_ptr()) };

    if result < 0 {
        None
    } else {
        Some(result)
    }
}

/// Print text literally to a stream.
///
/// `stream` must be one of the exported stream constants.  Returns the
/// number of bytes written, or `None` when the text contains an embedded
/// null byte or the underlying write fails.
#[pyfunction]
fn yupiter_fprintf(stream: usize, format: &str) -> Option<i32> {
    let c_str = CString::new(format).ok()?;

    // SAFETY: the stream constant is a valid yupiter_FILE pointer, and
    // both format strings are NUL-terminated and match the "%s" format.
    let result = unsafe { ffi_fprintf(stream as *mut c_void, c"%s".as_ptr(), c_str.as_ptr()) };

    if result < 0 {
        None
    } else {
        Some(result)
    }
}

// --- Basic memory functions ---

/// Allocate `size` bytes and return the address as an integer.
///
/// Returns `None` when the allocator cannot satisfy the request.  A size
/// of 0 returns `None`, matching the native interface.
#[pyfunction]
fn yupiter_malloc(size: usize) -> Option<usize> {
    // SAFETY: the allocator has no preconditions on the requested size.
    let ptr = unsafe { ffi_malloc(size) };

    if ptr.is_null() {
        None
    } else {
        Some(ptr as usize)
    }
}

/// Release a block returned by `yupiter_malloc` or `yupiter_realloc`.
///
/// Like the native function, an unrecognised pointer is ignored.  Use
/// `yupiter_free_safe` when invalid pointers must be reported.
#[pyfunction]
fn yupiter_free(ptr: usize) {
    // SAFETY: the native free function accepts null and ignores pointers
    // that are not the start of a live block.
    unsafe { ffi_free(ptr as *mut c_void) };
}

/// Resize a live block and return the new address as an integer.
///
/// A null pointer allocates, and a size of 0 releases the block and
/// returns `None`.  Returns `None` on failure, leaving the original block
/// unchanged.
#[pyfunction]
fn yupiter_realloc(ptr: usize, size: usize) -> Option<usize> {
    // SAFETY: the native function accepts null and validates the pointer
    // before it modifies the heap.
    let result = unsafe { ffi_realloc(ptr as *mut c_void, size) };

    if result.is_null() {
        None
    } else {
        Some(result as usize)
    }
}

// --- Safe memory functions ---

/// Compute `count * size`, raising `OverflowError` if the product exceeds
/// the range of `usize`.
fn allocation_size(count: usize, size: usize) -> PyResult<usize> {
    count
        .checked_mul(size)
        .ok_or_else(|| PyOverflowError::new_err("allocation size overflows"))
}

/// Translate a native `yupiter_memory_status` value into a Python result.
fn memory_status(status: c_int) -> PyResult<()> {
    match status {
        MEMORY_OK => Ok(()),
        MEMORY_OUT_OF_MEMORY => Err(PyMemoryError::new_err("out of memory")),
        MEMORY_OVERFLOW => Err(PyOverflowError::new_err("allocation size overflows")),
        MEMORY_INVALID_POINTER => Err(PyValueError::new_err("invalid or corrupted heap pointer")),
        _ => Err(PyValueError::new_err("unknown yupiter memory status")),
    }
}

/// Allocate `count * size` zero-filled bytes.
///
/// A zero-sized request returns 0.  Raises `OverflowError` when the
/// multiplication or the allocator alignment overflows, and `MemoryError`
/// when the heap is exhausted.
#[pyfunction]
fn yupiter_malloc_safe(count: usize, size: usize) -> PyResult<usize> {
    let total = allocation_size(count, size)?;

    if total == 0 {
        return Ok(0);
    }

    let mut ptr = std::ptr::null_mut();

    // SAFETY: `ptr` is writable storage outside the heap, and the checked
    // allocator validates the request before writing the result.
    let status = unsafe { ffi_realloc_checked(std::ptr::null_mut(), total, &mut ptr) };
    memory_status(status)?;

    // SAFETY: a successful allocation of `total` bytes is live and
    // writable, and the allocator returned a non-null pointer because
    // `total` is not zero.
    unsafe { std::ptr::write_bytes(ptr.cast::<u8>(), 0, total) };

    Ok(ptr as usize)
}

/// Release a live allocation or accept a null pointer (0).
///
/// Raises `ValueError` for an invalid, freed or corrupted pointer instead
/// of terminating the interpreter.
#[pyfunction]
fn yupiter_free_safe(ptr: usize) -> PyResult<()> {
    // SAFETY: the checked release validates the pointer without writing
    // to it unless the block is live and recognised.
    let status = unsafe { ffi_free_checked(ptr as *mut c_void) };
    memory_status(status)
}

/// Resize a block to `count * size` bytes.
///
/// A null pointer allocates zero-filled bytes, and a zero-sized request
/// releases the block and returns 0.  Failures preserve live blocks and
/// raise `OverflowError`, `MemoryError` or `ValueError` according to the
/// cause.
#[pyfunction]
fn yupiter_realloc_safe(ptr: usize, count: usize, size: usize) -> PyResult<usize> {
    let total = allocation_size(count, size)?;

    if ptr == 0 {
        return yupiter_malloc_safe(count, size);
    }

    let mut result = std::ptr::null_mut();

    // SAFETY: `result` is writable storage outside the heap, and the
    // checked resize validates both the pointer and the requested size.
    let status = unsafe { ffi_realloc_checked(ptr as *mut c_void, total, &mut result) };
    memory_status(status)?;

    Ok(result as usize)
}

// --- Module definition ---

/// Python bindings for the yupiter C library.
#[pymodule]
fn yupiter(m: &Bound<PyModule>) -> PyResult<()> {
    m.add_function(wrap_pyfunction!(yupiter_printf, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_fprintf, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_malloc, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_free, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_realloc, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_malloc_safe, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_free_safe, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_realloc_safe, m)?)?;

    // SAFETY: the three exported stream pointers are constinit objects
    // that stay valid for the lifetime of the process.
    m.add("STDOUT", unsafe { ffi_stdout as usize })?;
    m.add("STDERR", unsafe { ffi_stderr as usize })?;
    m.add("STDIN", unsafe { ffi_stdin as usize })?;

    Ok(())
}
