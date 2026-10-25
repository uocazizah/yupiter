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
use pyo3::types::PyBytes;
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
    #[link_name = "yupiter_printf"]
    fn ffi_printf(format: *const c_char, ...) -> c_int;

    #[link_name = "yupiter_fprintf"]
    fn ffi_fprintf(stream: *mut c_void, format: *const c_char, ...) -> c_int;

    #[link_name = "yupiter_malloc"]
    fn ffi_malloc(size: usize) -> *mut c_void;

    #[link_name = "yupiter_free"]
    fn ffi_free(ptr: *mut c_void);

    #[link_name = "yupiter_realloc"]
    fn ffi_realloc(ptr: *mut c_void, size: usize) -> *mut c_void;

    #[link_name = "yupiter_free_checked"]
    fn ffi_free_checked(ptr: *mut c_void) -> c_int;

    #[link_name = "yupiter_realloc_checked"]
    fn ffi_realloc_checked(ptr: *mut c_void, size: usize, result: *mut *mut c_void) -> c_int;

    #[link_name = "yupiter_getchar"]
    fn ffi_getchar() -> c_int;

    #[link_name = "yupiter_fgetc"]
    fn ffi_fgetc(stream: *mut c_void) -> c_int;

    #[link_name = "yupiter_fgets"]
    fn ffi_fgets(s: *mut c_char, n: c_int, stream: *mut c_void) -> *mut c_char;

    #[link_name = "yupiter_fread"]
    fn ffi_fread(ptr: *mut c_void, size: usize, nmemb: usize, stream: *mut c_void) -> usize;

    #[link_name = "yupiter_ungetc"]
    fn ffi_ungetc(c: c_int, stream: *mut c_void) -> c_int;

    #[link_name = "yupiter_feof"]
    fn ffi_feof(stream: *mut c_void) -> c_int;

    #[link_name = "yupiter_ferror"]
    fn ffi_ferror(stream: *mut c_void) -> c_int;

    #[link_name = "yupiter_clearerr"]
    fn ffi_clearerr(stream: *mut c_void);

    #[link_name = "yupiter_fflush"]
    fn ffi_fflush(stream: *mut c_void) -> c_int;

    #[link_name = "yupiter_stdout"]
    static ffi_stdout: *mut c_void;

    #[link_name = "yupiter_stderr"]
    static ffi_stderr: *mut c_void;

    #[link_name = "yupiter_stdin"]
    static ffi_stdin: *mut c_void;
}

// --- Output functions ---

#[pyfunction]
fn yupiter_printf(format: &str) -> Option<i32> {
    let c_str = CString::new(format).ok()?;

    let result = unsafe { ffi_printf(c"%s".as_ptr(), c_str.as_ptr()) };

    if result < 0 {
        None
    } else {
        Some(result)
    }
}

#[pyfunction]
fn yupiter_fprintf(stream: usize, format: &str) -> Option<i32> {
    let c_str = CString::new(format).ok()?;

    let result = unsafe { ffi_fprintf(stream as *mut c_void, c"%s".as_ptr(), c_str.as_ptr()) };

    if result < 0 {
        None
    } else {
        Some(result)
    }
}

// --- Input functions ---

#[pyfunction]
fn yupiter_getchar() -> i32 {
    unsafe { ffi_getchar() }
}

#[pyfunction]
fn yupiter_fgetc(stream: usize) -> i32 {
    unsafe { ffi_fgetc(stream as *mut c_void) }
}

#[pyfunction]
fn yupiter_fgets(stream: usize, n: usize) -> Option<String> {
    if n == 0 {
        return None;
    }
    let mut buf = vec![0 as c_char; n];
    let ptr = unsafe { ffi_fgets(buf.as_mut_ptr(), n as c_int, stream as *mut c_void) };
    if ptr.is_null() {
        return None;
    }
    let cstr = unsafe { std::ffi::CStr::from_ptr(ptr) };
    Some(cstr.to_string_lossy().into_owned())
}

#[pyfunction]
fn yupiter_fread(py: Python<'_>, stream: usize, size: usize, nmemb: usize) -> PyResult<Py<PyBytes>> {
    let total = size.saturating_mul(nmemb);
    let mut buf = vec![0u8; total];
    let count = unsafe { ffi_fread(buf.as_mut_ptr() as *mut c_void, size, nmemb, stream as *mut c_void) };
    buf.truncate(count.saturating_mul(size));
    Ok(PyBytes::new(py, &buf).into())
}

#[pyfunction]
fn yupiter_ungetc(c: i32, stream: usize) -> i32 {
    unsafe { ffi_ungetc(c, stream as *mut c_void) }
}

#[pyfunction]
fn yupiter_feof(stream: usize) -> bool {
    unsafe { ffi_feof(stream as *mut c_void) != 0 }
}

#[pyfunction]
fn yupiter_ferror(stream: usize) -> bool {
    unsafe { ffi_ferror(stream as *mut c_void) != 0 }
}

#[pyfunction]
fn yupiter_clearerr(stream: usize) {
    unsafe { ffi_clearerr(stream as *mut c_void) }
}

#[pyfunction]
fn yupiter_fflush(stream: usize) -> i32 {
    unsafe { ffi_fflush(stream as *mut c_void) }
}

// --- Basic memory functions ---

#[pyfunction]
fn yupiter_malloc(size: usize) -> Option<usize> {
    let ptr = unsafe { ffi_malloc(size) };

    if ptr.is_null() {
        None
    } else {
        Some(ptr as usize)
    }
}

#[pyfunction]
fn yupiter_free(ptr: usize) {
    unsafe { ffi_free(ptr as *mut c_void) };
}

#[pyfunction]
fn yupiter_realloc(ptr: usize, size: usize) -> Option<usize> {
    let result = unsafe { ffi_realloc(ptr as *mut c_void, size) };

    if result.is_null() {
        None
    } else {
        Some(result as usize)
    }
}

// --- Safe memory functions ---

fn allocation_size(count: usize, size: usize) -> PyResult<usize> {
    count
        .checked_mul(size)
        .ok_or_else(|| PyOverflowError::new_err("allocation size overflows"))
}

fn memory_status(status: c_int) -> PyResult<()> {
    match status {
        MEMORY_OK => Ok(()),
        MEMORY_OUT_OF_MEMORY => Err(PyMemoryError::new_err("out of memory")),
        MEMORY_OVERFLOW => Err(PyOverflowError::new_err("allocation size overflows")),
        MEMORY_INVALID_POINTER => Err(PyValueError::new_err("invalid or corrupted heap pointer")),
        _ => Err(PyValueError::new_err("unknown yupiter memory status")),
    }
}

#[pyfunction]
fn yupiter_malloc_safe(count: usize, size: usize) -> PyResult<usize> {
    let total = allocation_size(count, size)?;

    if total == 0 {
        return Ok(0);
    }

    let mut ptr = std::ptr::null_mut();

    let status = unsafe { ffi_realloc_checked(std::ptr::null_mut(), total, &mut ptr) };
    memory_status(status)?;

    unsafe { std::ptr::write_bytes(ptr.cast::<u8>(), 0, total) };

    Ok(ptr as usize)
}

#[pyfunction]
fn yupiter_free_safe(ptr: usize) -> PyResult<()> {
    let status = unsafe { ffi_free_checked(ptr as *mut c_void) };
    memory_status(status)
}

#[pyfunction]
fn yupiter_realloc_safe(ptr: usize, count: usize, size: usize) -> PyResult<usize> {
    let total = allocation_size(count, size)?;

    if ptr == 0 {
        return yupiter_malloc_safe(count, size);
    }

    let mut result = std::ptr::null_mut();

    let status = unsafe { ffi_realloc_checked(ptr as *mut c_void, total, &mut result) };
    memory_status(status)?;

    Ok(result as usize)
}

// --- Module definition ---

#[pymodule]
fn yupiter(m: &Bound<PyModule>) -> PyResult<()> {
    m.add_function(wrap_pyfunction!(yupiter_printf, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_fprintf, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_getchar, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_fgetc, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_fgets, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_fread, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_ungetc, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_feof, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_ferror, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_clearerr, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_fflush, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_malloc, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_free, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_realloc, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_malloc_safe, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_free_safe, m)?)?;
    m.add_function(wrap_pyfunction!(yupiter_realloc_safe, m)?)?;

    m.add("STDOUT", unsafe { ffi_stdout as usize })?;
    m.add("STDERR", unsafe { ffi_stderr as usize })?;
    m.add("STDIN", unsafe { ffi_stdin as usize })?;

    Ok(())
}