---
title: 调试与观测工具 — 总索引
tags: [debug, trace, log, gdb, elf, observability, Index]
desc: 用户态/内核态调试与观测工具按子主题归档：log、trace、gdb、elf。
update: 2026-04-08
---


# Debug & Observability KB

按 **观测维度** 而非工具名组织。新增笔记请落入对应子目录。

## 子主题

### `log/` — 日志通道
内核 printk、dmesg、loglevel、kmsg 重定向等。
- [00-printk-loglevel](log/00-printk-loglevel.md) — printk 重定向与 console_loglevel
- [01-klog-ssh-session](log/01-klog-ssh-session.md) — 把 printk 流到当前 SSH pts

### `trace/` — 运行时追踪 / 观测
syscall / library call / kernel tracepoint / eBPF。
- [00-strace](trace/00-strace.md) — strace 速成
- [01-observability-tools](trace/01-observability-tools.md) — ldd / ltrace / pmap / valgrind / eBPF 总览

### `gdb/` — 源码级调试器
- [00-gdb-quickstart](gdb/00-gdb-quickstart.md) — GDB 全景流程

### `elf/` — 二进制静态分析
ELF 节区、DWARF 调试信息、反汇编/反编译。
- [00-elf-debug-info-dwarf](elf/00-elf-debug-info-dwarf.md) — `.debug_*` 与 DWARF
- [01-disassembly-decompilation](elf/01-disassembly-decompilation.md) — 反汇编 → 反编译路径

## 计划占位

- `trace/ftrace.md` — 内核函数追踪
- `trace/perf.md` — perf events / 火焰图
- `log/dynamic-debug.md` — `pr_debug` / `CONFIG_DYNAMIC_DEBUG`
- `gdb/kgdb-imx6ull.md` — 内核远程调试
