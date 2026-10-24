// SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
// Copyright (C) 2026 Uoc Azizah

//! Build the yupiter library and link the Python extension against it.
//!
//! The binding is not part of the library build, so Cargo invokes the
//! top-level Makefile here.  The shared object is used rather than the
//! static archive: the archive contains the freestanding startup file,
//! whose `_start` symbol would conflict with the Python interpreter.

use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let manifest_dir =
        PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR is not set"));
    let project_root = manifest_dir
        .parent()
        .expect("the Python binding must live below the yupiter root");

    let output = Command::new("make")
        .arg("-C")
        .arg(project_root)
        .output()
        .expect("failed to run make");

    if !output.status.success() {
        panic!("make failed: {}", String::from_utf8_lossy(&output.stderr));
    }

    let build_dir = project_root.join("build");

    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=dylib=yupiter");

    /* Rebuild the extension when the library sources change; Cargo does
     * not know about the Makefile dependencies on its own. */
    for path in [
        "Makefile",
        "config.mk",
        "mk",
        "include",
        "io",
        "mem",
        "assembly",
    ] {
        println!(
            "cargo:rerun-if-changed={}",
            project_root.join(path).display()
        );
    }
}
