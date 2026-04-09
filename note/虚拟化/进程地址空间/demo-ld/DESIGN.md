---
title: Design of VMA Linking Demo
tags: [design, architecture, elf, loader]
desc: 解析 VMA 链接演示实验的架构设计与实现原理
update: 2026-04-09
---

# Design: VMA Linking Experiment

> [!note]
> **Ref:** 详见核心结论笔记 [`../00-1-VMA-linked.md`](../00-1-VMA-linked.md)

本实验的架构设计旨在通过最小化（Minimalist）的 C 代码实现，隔离出唯一的变量——**链接方式**，以此验证操作系统 ELF 加载器（Loader）和动态链接器（Dynamic Linker）的运行时行为。

## 1. 架构目标

1. **唯一控制变量**：确保静态链接与动态链接调用完全相同的源代码，仅在 `Makefile` 构建环节产生分歧。
2. **零依赖验证**：不需要依赖复杂的第三方库即可呈现出 VMA 映射的本质（合并段 vs 独立内存映射文件）。
3. **可观测性**：程序必须能在初始化后挂起（`sleep`），以便外部 Python 脚本 (`as_analyzer.py`) 能够抓取 `/proc/[pid]/maps` 快照。

## 2. 模块设计

### 2.1 靶子库：`lib/mylib.c`
为了在内存空间中形成清晰的指纹，库中设计了两个元素：
- `lib_function()`: `.text` 段的代表。
- `lib_global_var`: 初始化为 `0x12345678`，代表 `.data` 段的全局变量，且在代码中会主动打印其地址，用于反查对比。

### 2.2 主程序：`src/main.c`
核心职责：
- 调用库函数以防止符号被链接器优化阶段裁剪（Dead Code Elimination）。
- 打印自身 `PID`，方便无缝对接自动化脚本。
- `sleep(60)`：为观察 `procfs` 预留足够的时间窗口。

### 2.3 构建管线：`Makefile`
在构建设计上，做了两项关键决策：
1. **统一路径输出**：所有的二进制产物统一输出到 `output/`，保证源码目录的干净。
2. **硬编码 RPATH (`-Wl,-rpath,./output`)**：动态链接版本在生成时指定了 `-rpath`，这样 `app_shared` 可以在运行时直接从相对路径 `output/` 找到 `libmylib.so`，而无需用户手动设置 `LD_LIBRARY_PATH`，降低了实验复现的门槛。

## 3. 理论映射关系

根据 Linux ELF 加载模型：
- **`app_static` 场景**：由 `ld` (静态链接器) 在编译期完成。库的 ELF Section 会根据权限（PROT_READ, PROT_EXEC 等）被重组并打包进主程序的 ELF Segment 中。加载时表现为同一个 `[path/to/app_static]` 的连续 VMA。
- **`app_shared` 场景**：由 `ld-linux.so` (动态加载器) 在运行期完成。利用 `mmap(MAP_PRIVATE)` 将 `libmylib.so` 文件映射到进程的匿名高地址区域，由于共享库同样需要满足 W^X 原则，因此会为其单独开辟多段具有不同权限的 VMA。
