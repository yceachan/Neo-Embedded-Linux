---
title: Consult of VMA Linking Demo
tags: [faq, troubleshooting, gcc, fpic]
desc: 针对链接方式实验中常见问题（FAQ）的解答与探讨
update: 2026-04-09
---

# Consult: Linking Demo FAQ

本页面解答了在编写和运行 `demo-ld` 实验时可能遇到的一些工程层面的疑问。

## Q1: 为什么编译 `libmylib.so` 时必须加上 `-fPIC` 标志？
**A:** `-fPIC` (Position Independent Code，位置无关代码) 是生成现代共享库的基础。
在动态链接场景中，同一个 `.so` 文件可能会被不同的进程加载到其虚拟内存空间中**完全不同的基础地址**上。如果使用了 `-fPIC`，代码中对于数据段和函数调用的相对寻址会通过 Global Offset Table (GOT) 和 Procedure Linkage Table (PLT) 间接完成，从而使得整个 `.text` 段不需要进行重定位即可在不同进程间实现真正的**物理页共享**。如果没有这个标志，链接器可能报错或者强制生成只能在固定地址运行的代码。

## Q2: `Makefile` 里的 `-Wl,-rpath,./output` 是干什么用的？如果不加会怎样？
**A:** 这个编译参数的作用是告诉动态链接器在程序运行时，去哪里寻找 `.so` 文件。
如果不加这个参数，当你执行 `./output/app_shared` 时，系统默认只会去标准路径（如 `/lib`，`/usr/lib`）以及由环境变量 `LD_LIBRARY_PATH` 指定的路径去寻找 `libmylib.so`。实验将会报错类似于 `error while loading shared libraries: libmylib.so: cannot open shared object file: No such file or directory`。
使用 `-rpath` 将查找路径硬编码到 ELF 文件中，是为了让 Demo 真正做到“开箱即用”。

## Q3: 为什么 `as_analyzer.py` 的分析结果中，一个 `.so` 会被映射出 3~5 个 VMA？
**A:** 这与内核现代的安全机制（W^X 原则）有关。
虽然在磁盘上它是一个完整的 `.so` 文件，但操作系统在加载它时，会根据 ELF 的 Section Header 赋予不同区域不同的权限：
1. **只读数据段 (`r--p`)**: 比如 `.rodata`，用于存放字符串字面量等。
2. **代码段 (`r-xp`)**: 比如 `.text`，需要可执行权限，但不能被写入。
3. **可写数据段 (`rw-p`)**: 比如 `.data` 和 `.bss`，用于存放非常量全局变量，必须可写但不能执行。

正是这种细粒度的权限划分，导致在 `/proc/<pid>/maps` 中看到同一个 `.so` 裂变成了多个 VMA。

## Q4: 怎么判断一个全局变量到底是进了主程序的 VMA 还是独立 `.so` 的 VMA？
**A:** 在我们的代码中，特意使用 `printf` 输出了 `lib_global_var` 的地址：
```c
printf("[mylib] %s (lib_global_var addr: %p)\n", msg, (void*)&lib_global_var);
```
通过观察终端输出的指针十六进制值，然后对照 `as_analyzer.py` 输出的 maps 表格查找它落在哪个 `start - end` 范围内。
- 若是**静态链接**，该地址一定落在以 `.../app_static` 结尾且权限为 `rw-p` 的行内。
- 若是**动态链接**，该地址则落在以 `.../libmylib.so` 结尾且权限为 `rw-p` 的行内。
这构成了本实验"自验证"逻辑的最核心闭环。
