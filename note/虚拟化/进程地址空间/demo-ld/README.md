---
title: VMA Linking Demo
tags: [demo, linking, vma]
desc: 演示静态链接与动态链接在进程虚拟内存地址空间（VMA）上的区别
update: 2026-04-09
---

# 链接方式对 VMA 布局的影响演示

本目录（`demo-ld`）包含一个用于观察**静态链接**（Static Linking）与**动态链接**（Dynamic Linking）对进程地址空间（VMA）影响的对比实验。

## 1. 实验目的

通过同一个 C 语言主程序（`main.c`）和同一个库（`mylib.c`），分别编译出静态链接版本和动态链接版本。运行后，利用上级目录的 `as_analyzer.py` 工具观察两者的 `/proc/<pid>/maps`，直观感受 ELF 加载器在内存中的行为差异。

## 2. 目录结构

```text
demo-ld/
├── Makefile       # 构建脚本，负责生成 .a, .so 以及对应的可执行文件
├── lib/
│   ├── mylib.c    # 包含 lib_function 和 lib_global_var
│   └── mylib.h
├── src/
│   └── main.c     # 主程序入口，调用 lib_function 并挂起 60 秒等待观察
└── output/        # 编译产物目录（由 make 自动生成）
```

## 3. 运行指南

### 3.1 编译代码
```bash
make clean
make
```
这将在 `output/` 目录下生成四个关键文件：
- `mylib_static.o` / `libmylib.a` (静态库)
- `libmylib.so` (动态库)
- `app_static` (静态链接的主程序)
- `app_shared` (动态链接的主程序)

### 3.2 观察静态链接的 VMA
```bash
./output/app_static &
PID=$!
# 调用上一级目录的分析器
python ../demo/as_analyzer.py $PID
kill $PID
```
**预期现象：** 库的数据和代码被合并到了主程序的 `exe-text` 和 `exe-data` 中。

### 3.3 观察动态链接的 VMA
```bash
./output/app_shared &
PID=$!
# 调用上一级目录的分析器
python ../demo/as_analyzer.py $PID
kill $PID
```
**预期现象：** `libmylib.so` 作为独立文件被映射，拥有自己专属的 `lib-text`、`lib-rodata` 和 `lib-data` 段。
