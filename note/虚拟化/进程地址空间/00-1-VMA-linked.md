---
title: VMA 布局：静态链接 vs 动态链接
tags: [VMA, linking, static, shared, elf]
desc: 观察相同功能代码在静态链接（.a）与动态链接（.so）下 VMA 布局的差异。
update: 2026-04-09
---

# VMA 布局：静态链接 vs 动态链接

> [!note]
> **Ref:** [`./demo-ld/`](./demo-ld/)

本笔记通过 `demo-ld` 实验观察：将一段库代码 (`mylib.c`) 分别以 `.a` (静态) 和 `.so` (动态) 方式链接到主程序时，进程地址空间产生的结构性差异。

## 1. 核心结论

| 维度 | 静态链接 (`.a`) | 动态链接 (`.so`) |
|------|----------------|----------------|
| **VMA 归属** | 库代码合并到 `exe-text`，库数据合并到 `exe-data` | 库拥有独立的 `lib-text` / `lib-rodata` / `lib-data` 段 |
| **文件映射** | 所有库逻辑都在 `app_static` 二进制内 | 映射独立的 `libmylib.so` 文件 |
| **VMA 数量** | 较少（只有主程序 + 系统库） | 较多（每个 .so 增加 3~5 个 VMA） |
| **符号地址** | 库变量地址与 `main` 距离较近（同属 exe 段） | 库变量地址位于共享库区，与 `main` 跨度大 |

## 2. 实验现象对照（High Addr First）

### 2.1 静态链接 `app_static` 全景

在静态链接中，`mylib.o` 的所有内容都被物理地“合并”进了 `app_static` 文件中。

```text
=== address space of PID 67960 (23 VMAs) ===
         start             end    size  perms  class        path
------------------------------------------------------------------------------------------
  7ffd92b74000  7ffd92b76000      8K  r-xp  vdso         [vdso]
  7ffd92b70000  7ffd92b74000     16K  r--p  vvar         [vvar]
  7ffd92ab8000  7ffd92ada000    136K  rw-p  stack        [stack]
  7488159e3000  7488159e5000      8K  rw-p  lib-data     .../ld-linux-x86-64.so.2
  7488159e1000  7488159e3000      8K  r--p  lib-rodata   .../ld-linux-x86-64.so.2
  7488159d5000  7488159e0000     44K  r--p  lib-rodata   .../ld-linux-x86-64.so.2
  7488159ab000  7488159d5000    168K  r-xp  lib-text     .../ld-linux-x86-64.so.2
  7488159a9000  7488159ab000      8K  r--p  lib-rodata   .../ld-linux-x86-64.so.2
  7488159a7000  7488159a9000      8K  rw-p  anon-rw      [anon]
  74881599b000  74881599e000     12K  rw-p  anon-rw      [anon]
  74881581c000  748815829000     52K  rw-p  anon-rw      [anon]
  74881581a000  74881581c000      8K  rw-p  lib-data     .../libc.so.6
  748815816000  74881581a000     16K  r--p  lib-rodata   .../libc.so.6
  748815815000  748815816000      4K  ---p  lib-rodata   .../libc.so.6
  7488157bd000  748815815000    352K  r--p  lib-rodata   .../libc.so.6
  748815628000  7488157bd000      1M  r-xp  lib-text     .../libc.so.6
  748815600000  748815628000    160K  r--p  lib-rodata   .../libc.so.6
  57e0f4bd5000  57e0f4bf6000    132K  rw-p  heap         [heap]
  57e0da4e6000  57e0da4e7000      4K  rw-p  exe-data     .../app_static   ← 包含 lib_global_var
  57e0da4e5000  57e0da4e6000      4K  r--p  exe-rodata   .../app_static
  57e0da4e4000  57e0da4e5000      4K  r--p  exe-rodata   .../app_static
  57e0da4e3000  57e0da4e4000      4K  r-xp  exe-text     .../app_static   ← 包含 main + lib_function
  57e0da4e2000  57e0da4e3000      4K  r--p  exe-rodata   .../app_static
```

### 2.2 动态链接 `app_shared` 全景

在动态链接中，`libmylib.so` 是独立加载的，拥有自己的一套权限保护段。

```text
=== address space of PID 67967 (28 VMAs) ===
         start             end    size  perms  class        path
------------------------------------------------------------------------------------------
  7fff8ef65000  7fff8ef67000      8K  r-xp  vdso         [vdso]
  7fff8ef61000  7fff8ef65000     16K  r--p  vvar         [vvar]
  7fff8eec0000  7fff8eee2000    136K  rw-p  stack        [stack]
  7324823d2000  7324823d4000      8K  rw-p  lib-data     .../ld-linux-x86-64.so.2
  7324823d0000  7324823d2000      8K  r--p  lib-rodata   .../ld-linux-x86-64.so.2
  7324823c4000  7324823cf000     44K  r--p  lib-rodata   .../ld-linux-x86-64.so.2
  73248239a000  7324823c4000    168K  r-xp  lib-text     .../ld-linux-x86-64.so.2
  732482398000  73248239a000      8K  r--p  lib-rodata   .../ld-linux-x86-64.so.2
  732482396000  732482398000      8K  rw-p  anon-rw      [anon]
  732482395000  732482396000      4K  rw-p  lib-data     .../libmylib.so  ← 库变量地址在此
  732482394000  732482395000      4K  r--p  lib-rodata   .../libmylib.so
  732482393000  732482394000      4K  r--p  lib-rodata   .../libmylib.so
  732482392000  732482393000      4K  r-xp  lib-text     .../libmylib.so  ← 库函数代码在此
  732482391000  732482392000      4K  r--p  lib-rodata   .../libmylib.so
  732482391000  732482392000      4K  r--p  lib-rodata   .../libmylib.so
  732482385000  732482388000     12K  rw-p  anon-rw      [anon]
  73248221c000  732482229000     52K  rw-p  anon-rw      [anon]
  73248221a000  73248221c000      8K  rw-p  lib-data     .../libc.so.6
  732482216000  73248221a000     16K  r--p  lib-rodata   .../libc.so.6
  732482215000  732482216000      4K  ---p  lib-rodata   .../libc.so.6
  7324821bd000  732482215000    352K  r--p  lib-rodata   .../libc.so.6
  732482028000  7324821bd000      1M  r-xp  lib-text     .../libc.so.6
  732482000000  732482028000    160K  r--p  lib-rodata   .../libc.so.6
  5ade5751c000  5ade5753d000    132K  rw-p  heap         [heap]
  5ade2b6fb000  5ade2b6fc000      4K  rw-p  exe-data     .../app_shared
  5ade2b6fa000  5ade2b6fb000      4K  r--p  exe-rodata   .../app_shared
  5ade2b6f9000  5ade2b6fa000      4K  r--p  exe-rodata   .../app_shared
  5ade2b6f8000  5ade2b6f9000      4K  r-xp  exe-text     .../app_shared   ← 仅包含 main
  5ade2b6f7000  5ade2b6f8000      4K  r--p  exe-rodata   .../app_shared
```

## 3. 为什么 VMA 布局不同？

### 3.1 静态链接：代码大融合
当你运行 `gcc ... main.o libmylib.a` 时：
1. 链接器 (ld) 把 `main.o` 的 `.text` 和 `mylib.o` 的 `.text` 接在一起，形成一个大的 `.text`。
2. 最终生成的 ELF 文件只有一个 `LOAD (R-X)` 段。
3. 内核加载时，只需为这个文件创建一对 (`r-xp`, `rw-p`) VMA。

### 3.2 动态链接：按需拼图
当你运行 `gcc ... main.o -lmylib` 时：
1. 链接器仅在 `app_shared` 中留下一个"占位符"（即 `PLT/GOT` 表项），不包含库代码。
2. **运行时**，动态链接器 (`ld-linux.so`) 发现依赖，调用 `mmap` 把 `libmylib.so` 映射到内存。
3. 每个 `mmap` 调用都会根据 `.so` 的权限分段创建新的 VMA。

## 4. 观察建议

使用 `as_analyzer.py --locate <addr>` 可以更清晰地看到这种归属感：
- 在静态版中，`lib_global_var` 的地址反查结果是 `exe-data`。
- 在动态版中，反查结果则是 `lib-data`，且 path 指向 `libmylib.so`。
