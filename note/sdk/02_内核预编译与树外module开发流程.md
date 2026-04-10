> [构建外部模块 — Linux 内核文档](https://docs.linuxkernel.org.cn/kbuild/modules.html#creating-a-kbuild-file-for-an-external-module)
## 1. 内核配置：`make <board>_defconfig`

该步骤的作用是将针对特定硬件板卡的预设配置（符号）应用到当前内核源码树中。
### 板级 defconfig 存放路径
- **路径**：`arch/arm/configs/`
- **示例**：`arch/arm/configs/100ask_imx6ull_defconfig`
### 文件内容
- 纯文本格式，包含一系列配置宏。
- 它只记录与内核默认值不同的差异化配置，比生成的 `.config` 文件精简得多。
### 构建产物
- **`.config`**：位于内核源码根目录。这是内核编译的核心配置文件，决定了哪些功能被编译进内核（y）、哪些编译为模块（m）、哪些不编译（n）。
---

## 2. module prepare
`make HOSTCFLAGS="-fcommon" modules_prepare`

该步骤主要用于在不完整编译整个内核的情况下，准备好编译**外部驱动模块**（out-of-tree modules）所需的环境。

### 构建产物
- **`include/generated/`**：生成版本头文件（`utsrelease.h`）和自动配置头文件（`autoconf.h`）。
- **`scripts/`**：编译出运行在开发机（Host）上的工具，如 `fixdep`, `modpost` 等，用于处理后续的模块链接。
- **`Module.symvers`**：生成内核符号表，用于模块间的符号解析。

### 参数解析
- **`HOSTCFLAGS`**：传递给宿主机编译器（本地 GCC）的标志。
- **`-fcommon`**：由于 GCC 10+ 默认开启 `-fno-common`（禁止重复定义全局变量），而旧内核（如 4.9.88）脚本中存在重复定义，使用此标志可确保在现代 Linux 发行版（如 Ubuntu 20.04/22.04）上编译通过。

---


## 4.Issue
> [!hint]
>  **错误现象**：`make: *** No rule to make target 'arch/ARM/Makefile'. Stop.``
- **原因**：Linux 内核 Makefile 对 `ARCH` 参数**大小写敏感**。必须使用小写 **`arm`**。
- **解决方法**：
  ```bash
  export ARCH=arm
  ```
---
> [!hint]
>  **错误现象**：mutilpie definte... 
- **原因**：由于 GCC 10+ 默认开启 `-fno-common`（禁止重复定义全局变量），而旧内核（如 4.9.88）脚本中存在重复定义
- **解决方法**：
  ```bash
     make HOSTCFLAGS="-fcommon"
  ```
