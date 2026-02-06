---
title: Kbuild 架构与构建原理
tags: [Kbuild, Architecture, vmlinux, built-in.o]
update: 2026-02-07

---

# Kbuild 架构与构建原理

Kbuild 是一个递归构建系统，其核心任务是将成千上万个源文件组织成两个最终产物：驻留内核镜像 (`vmlinux`) 和可加载模块 (`modules`)。

## 1. 构建流程概览

1.  **配置阶段**：读取 `.config`，生成 `include/generated/autoconf.h`。
2.  **递归编译**：从顶层 Makefile 开始，深入各子目录。
3.  **链接 built-in.o**：每个子目录下的 `obj-y` 被合并为当前目录的 `built-in.o`。
4.  **生成 vmlinux**：顶层 Makefile 将所有顶级目录的 `built-in.o` 链接成 `vmlinux`。
5.  **模块后处理 (MODPOST)**：处理模块符号版本控制 (`Module.symvers`)。

## 2. `built-in.o` 的生成机制

Kbuild 在每个子目录下执行类似如下的逻辑：
1.  编译 `obj-y` 列表中的所有 `.c` / `.S` 文件。
2.  如果 `obj-y` 包含子目录（如 `obj-y += subdir/`），则进入子目录递归执行。
3.  最终调用链接器 (`ld -r`) 将当前目录下的所有 `.o` 和子目录的 `built-in.o` 合并为一个大的 `built-in.o`。

这种机制确保了顶层链接器只需要关心顶层目录（如 `drivers/built-in.o`, `fs/built-in.o`），而不需要知道底层的成千上万个文件。

## 3. `vmlinux` 的链接顺序

`vmlinux` 的最终布局由顶层 Makefile 和架构相关的链接脚本 (`vmlinux.lds`) 共同决定。

### 关键变量
*   `KBUILD_VMLINUX_INIT`: 指定链接在最前面的对象（通常是汇编启动代码）。
*   `KBUILD_VMLINUX_MAIN`: 指定内核的主体部分。

典型的链接顺序如下：
1.  `arch/$(ARCH)/kernel/head.o` (入口点)
2.  `init/built-in.o`
3.  `usr/built-in.o`
4.  `arch/$(ARCH)/kernel/built-in.o`
5.  ... 其他核心子系统 ...
6.  `drivers/built-in.o`

## 4. 关键环境变量

Kbuild 允许通过环境变量或命令行参数微调构建行为：

| 变量 | 作用 | 示例 |
| :--- | :--- | :--- |
| `ARCH` | 指定目标架构 | `make ARCH=arm` |
| `CROSS_COMPILE` | 交叉编译器前缀 | `make CROSS_COMPILE=arm-linux-gnueabihf-` |
| `KBUILD_VERBOSE` | 控制输出详细程度 | `make V=1` (显示完整命令) |
| `KBUILD_OUTPUT` | 指定构建产物目录 | `make O=../build` (保持源码干净) |
| `KBUILD_EXTRA_SYMBOLS` | 引入外部模块符号 | 用于模块间依赖 |
| `INSTALL_MOD_PATH` | 模块安装路径前缀 | `make INSTALL_MOD_PATH=/rootfs modules_install` |

## 5. 构建产物说明

*   **`vmlinux`**: 原始的 ELF 格式内核镜像（包含符号表，巨大）。
*   **`Image` / `zImage` / `uImage`**: 经过 strip、压缩、添加 u-boot 头部的最终启动镜像（架构相关）。
*   **`modules.order`**: 记录模块的构建顺序，用于 `modprobe` 处理依赖。
*   **`modules.builtin`**: 记录哪些模块被编译进了内核（即 `obj-y` 里的驱动），防止 `modprobe` 报错。
