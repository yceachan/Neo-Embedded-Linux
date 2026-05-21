use std::fs;

fn main() {
    println!("Hello, TSPI! (from Rust)");
    println!("compile-time ARCH = {}", std::env::consts::ARCH);
    println!("compile-time OS   = {}", std::env::consts::OS);

    if let Ok(host) = fs::read_to_string("/etc/hostname") {
        println!("runtime hostname  = {}", host.trim());
    }
    if let Ok(model) = fs::read_to_string("/proc/device-tree/model") {
        println!("runtime model     = {}", model.trim_end_matches('\0'));
    }
}
