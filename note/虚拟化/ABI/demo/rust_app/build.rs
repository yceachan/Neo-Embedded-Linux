use std::process::Command;

fn main() {
    // Rerun if C source changes
    println!("cargo:rerun-if-changed=../clib/hello.c");
    println!("cargo:rerun-if-changed=../clib/hello.h");
    
    let out_dir = std::env::var("OUT_DIR").unwrap();

    // Compile C code manually for demonstration
    Command::new("gcc")
        .args(&["-c", "../clib/hello.c", "-o", &format!("{}/hello.o", out_dir)])
        .status()
        .expect("Failed to compile C file");
    
    // Archive into a static library
    Command::new("ar")
        .args(&["rcs", &format!("{}/libhello.a", out_dir), &format!("{}/hello.o", out_dir)])
        .status()
        .expect("Failed to create static library");

    // Tell cargo where to find the library and its name
    println!("cargo:rustc-link-search=native={}", out_dir);
    println!("cargo:rustc-link-lib=static=hello");
}
