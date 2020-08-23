/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright 2020 Casper Meijn <casper@meijn.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

extern crate bindgen;

use std::env;
use std::path::PathBuf;
use std::process::Command;
use std::str;

fn main() {
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap()).join("bindings.rs");

    let mut builder = bindgen::Builder::default()
        .use_core()
        .derive_default(true)
        .ctypes_prefix("cty")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .header("wrapper.h");
    println!("cargo:rerun-if-changed=wrapper.h");

    // If available, set the sysroot as mynewt would do
    if let Ok(cc_path) = env::var("MYNEWT_CC_PATH") {
        let cc_output = Command::new(cc_path)
            .arg("-print-sysroot")
            .output()
            .expect("failed to execute gcc");
        if !cc_output.status.success() {
            panic!("Error: C-compiler failed to provide sysroot");
        }
        let sysroot_path = str::from_utf8(&cc_output.stdout).unwrap().trim();
        builder = builder.clang_arg(format!("--sysroot={}", sysroot_path));
    }

    // If available, set the include directories as mynewt would do
    if let Ok(include_path_list) = env::var("MYNEWT_INCLUDE_PATH") {
        for include_path in include_path_list.split(":") {
            builder = builder.clang_arg("--include-directory=".to_owned() + include_path);
        }
    }
    // If available, set the CFLAGS as mynewt would do
    if let Ok(cflag_list) = env::var("MYNEWT_CFLAGS") {
        for cflag in cflag_list.split(" ") {
            builder = builder.clang_arg(cflag);
        }
    }

    builder
        .generate()
        .map_err(|_| "Failed to generate")
        .unwrap()
        .write_to_file(out_path)
        .map_err(|e| format!("Failed to write output: {}", e))
        .unwrap();
}
