// 声明外部 C 函数，注意参数类型映射：C 的 int 对应 Rust 的 i32
extern "C" {
    fn process_data(a: i32, b: i32) -> i32;
}

fn main() {
    println!("[Rust App] Starting ABI parameter passing demo...");
    
    let x: i32 = 15;
    let y: i32 = 27;
    
    // 跨语言调用
    unsafe {
        let result = process_data(x, y);
        println!("[Rust App] Received result from C: {}", result);
    }
}
