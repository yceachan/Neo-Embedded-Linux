# C/Rust 混合编程 - ABI Demo

本 Demo 演示了如何使用 Rust 调用 C 语言编写的静态库，并传递参数与返回值。

## 1. 结构说明

- `clib/`: C 语言库源码 (使用 `gcc` 编译并通过 `ar` 打包为 `.a` 静态库)。
- `rust_app/`: Rust 主程序 (通过 `build.rs` 自动编译并链接 C 库)。
- `release/`: 构建完成后的发布产物。

## 2. 核心操作命令

### 2.1 一键构建并运行 (Debug 模式)
用于日常开发调试，包含符号信息。
```bash
cd rust_app && cargo run
```

### 2.2 一键构建优化版 (Release 模式)
生成体积最小、运行速度最快的二进制文件。
```bash
cd rust_app && cargo build --release
```

### 2.3 一键打包发布 (Package)
清理、构建并生成包含独立 ELF 程序的压缩包。
```bash
# 在 demo 根目录下执行 (note/ABI/demo/)
rm -rf release/ && mkdir -p release/ && \
cd rust_app && cargo build --release && \
cp target/release/rust_app ../release/ && \
cd ../release/ && tar -czvf rust_app_release.tar.gz rust_app
```

## 3. ABI 关键技术要点

- **符号匹配**: 使用 `extern "C"` 块在 Rust 中声明函数，强制遵循 C 调用约定。
- **构建自动化**: `rust_app/build.rs` 承担了“编译器粘合剂”的角色，它会自动检测 C 源码变化并重新生成静态库。
- **零开销**: 参数传递通过物理寄存器（如 EDI, ESI）直接进行，无内存拷贝开销。
- **跨语言链接**: 链接器将 `.text` 段合并，生成的最终 ELF 是完全独立的二进制。

## 4. 依赖项

- `gcc`: 用于编译 C 代码。
- `ar`: 用于打包静态库。
- `rustc / cargo`: Rust 编译套件。
