---
title: Demo ELF地址空间解析
tags: [ELF, ABI, Memory-Layout, PIE, Rust]
desc: 深入解析 C/Rust 混合编程生成的 ELF 可执行文件在操作系统层面的内存映射与地址空间布局。
update: 2026-04-06

---


# Demo ELF地址空间解析

> [!note]
> **Ref:** 
> - 本地代码与产物: `note/ABI/demo/release/rust_app`
> - Linux ELF & ABI 规范

当我们使用跨语言混合编程（如 Rust + C 静态库）并以 Release 模式编译打包后，最终产出的是一个单一的 ELF 格式可执行文件。通过 `readelf -l` 和 `size` 工具，我们可以透视这个二进制文件被 Linux 内核加载到内存时的地址空间映射（Segments 布局）。

## 1. 核心安全机制概览

解析该 ELF 头部信息，可以发现现代工具链（Rustc + GCC）默认启用的底层安全机制：

- **PIE (Position-Independent Executable)**：ELF 类型显示为 `DYN`，这意味着程序代码是位置无关的。每次运行，操作系统会通过 ASLR（地址空间布局随机化）将其映射到不同的物理和虚拟内存基址，防止固定地址的 ROP 攻击。
- **NX Bit (非执行栈)**：通过 `GNU_STACK` 段的权限仅为 `RW`（无 `E` Execute 权限），确保了进程栈空间的数据不可执行，直接阻断了基础的栈溢出执行 Shellcode 攻击。
- **RELRO (Relocation Read-Only)**：动态链接过程结束后，`.got` 等重定位表区域会立即被内核锁死为只读（`GNU_RELRO` 段），防止 GOT 表劫持。

## 2. 内存映射分段详解 (Segments mapping)

一个进程的地址空间由多个具有不同读写执行权限的段（Segments）组成，每个 Segment 内部包含了一个或多个 Section（节）。

### 2.1 只读数据与元数据 (Read-Only Data)
- **权限**: `R` (Read)
- **包含节**: `.interp`, `.rodata`, `.dynsym`, `.eh_frame`
- **解析**: 
  - `.interp` 指明了动态链接器（如 `/lib64/ld-linux-x86-64.so.2`）的路径。即使我们的 C 代码是静态链接的，Rust 默认仍会动态链接底层的 `libc` 等基础设施。
  - `.rodata` 存放了所有硬编码的字符串（如 Rust 中的 `"[Rust App]..."` 和 C 中的 `"[C Library]..."`）。跨语言的常量都在这里统一存放。

### 2.2 代码执行段 (Executable Code)
- **权限**: `R E` (Read + Execute)
- **包含节**: `.text`, `.plt`, `.init`, `.fini`
- **解析**: 
  - **混合编程的归宿**：这里是核心的机器指令区。**Rust 的 `main` 函数代码与 C 静态库里的 `process_data` 代码，在链接阶段被彻底打散并重新合并，共同存在于这个单一的 `.text` 节中**。在 CPU 看来，它们不再区分语言，只是连续可执行的机器码。

### 2.3 可读写数据区 (Data & BSS)
- **权限**: `RW` (Read + Write)
- **包含节**: `.data`, `.bss`
- **解析**: 
  - `.data` 存放已初始化的全局/静态变量。
  - `.bss` 存放未初始化的全局/静态变量。这部分在 ELF 文件中不占用实际的磁盘物理空间（表现为文件大小远小于进程空间的分配要求），只有在内核加载进程时，才会在内存中分配空间并清零。

### 2.4 线程本地存储 (TLS)
- **权限**: `R` / `RW`
- **包含节**: `.tdata`, `.tbss`
- **解析**: Rust 的标准库严重依赖线程本地存储（如恐慌上下文环境、线程自身状态）。即使是单线程应用，也会建立该段空间。

## 3. 跨语言内存的统一性

通过地址空间的解析，揭示了混合编程 ABI 的最深层逻辑：
1. **单一的栈区与堆区**：无论是运行在 Rust 上下文还是跳转入 C 上下文，**它们共享同一个系统栈 (Stack) 和堆 (Heap)**。C 的局部变量和 Rust 的局部变量在同一个栈内存中压栈/出栈。
2. **段的无差别合并**：链接器（Linker）不关心 `.o` 目标文件是用什么高级语言编译出来的。它只认 Section 属性。C 产生的 `.text` 与 Rust 产生的 `.text` 被完美缝合，这就是不同语言能够进行“零开销”互相调用的物理基础。
