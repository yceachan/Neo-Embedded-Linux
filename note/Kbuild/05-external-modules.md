---
title: 树外模块 (Out-of-tree) 开发指南
tags: [Kbuild, Module, Makefile, Driver]
update: 2026-02-07

---

# 树外模块 (Out-of-tree) 开发指南

在嵌入式 Linux 开发中，驱动程序通常作为**外部模块**（即不在内核源码树内）进行开发和编译。本文档基于 `Documentation/kbuild/modules.txt` 整理了树外模块的标准构建流程。

## 1. 核心构建命令

构建树外模块的核心在于通知 Kbuild 系统：
1.  **内核源码在哪里** (`-C <path>`)
2.  **当前模块在哪里** (`M=$PWD` 或 `M=<dir>`)

### 标准命令格式
```bash
make -C <内核源码路径> M=<模块源码路径> [target]
```
- `-C <path>`: `make` 会先切换到内核源码目录，读取那里的顶层 Makefile。
- `M=$PWD`: 告知 Kbuild 回到当前目录来编译模块。

### 常用 Target
- `modules`: 默认目标，编译模块 (`.ko`)。
- `modules_install`: 安装模块到 `/lib/modules/<ver>/extra/`。
- `clean`: 清理生成文件。
- `help`: 查看帮助。

## 2. Makefile 编写模板

为了兼容直接执行 `make` 和被 Kbuild 调用，通常使用以下双重判定结构的 Makefile：

```makefile
# ---------------------------------------------------
# Part 1: Kbuild 调用的部分
# ---------------------------------------------------
ifneq ($(KERNELRELEASE),)

# 定义模块名，最终生成 hello_drv.ko
obj-m := hello_drv.o

# 如果模块由多个源文件组成 (hello_drv = main.o + func.o)
# hello_drv-y := main.o func.o

# ---------------------------------------------------
# Part 2: 用户直接执行 make 调用的部分
# ---------------------------------------------------
else

# 定义内核路径 (根据实际情况修改，或通过环境变量传入)
# 对于 IMX6ULL SDK，通常指向构建好的内核目录
KDIR ?= /home/pi/imx/sdk/100ask_imx6ull-sdk/Linux-4.9.88

# 当前目录
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif
```

## 3. 常见进阶场景

### 3.1 引用头文件
如果你的驱动有私有头文件在子目录（例如 `include/`）：
```makefile
# 在 Part 1 中添加
ccflags-y := -I$(src)/include
```
*注意：`-I` 和路径之间不能有空格。*

### 3.2 模块依赖与符号导出
如果 `Module A` 依赖 `Module B` 导出的符号 (`EXPORT_SYMBOL`)：
1.  **编译顺序**：先编译 B，再编译 A。
2.  **Module.symvers**：编译 B 后会生成 `Module.symvers`。编译 A 时，若 A 与 B 在同一目录，Kbuild 会自动读取。
3.  **不同目录**：若在不同目录，需使用 `KBUILD_EXTRA_SYMBOLS` 变量：
    ```bash
    make -C $KDIR M=$PWD KBUILD_EXTRA_SYMBOLS=/path/to/B/Module.symvers modules
    ```

### 3.3 包含二进制 Blob
如果需要将预编译的二进制对象（如 `firmware.o_shipped`）链接进模块：
1.  将文件重命名为 `firmware.o_shipped`。
2.  在 Makefile 中引用去掉后缀的文件名：
    ```makefile
    obj-m := my_driver.o
    my_driver-y := main.o firmware.o
    ```

## 4. IMX6ULL 实战 Tips

在本项目环境 (`/home/pi/imx`) 下开发驱动时：

1.  **交叉编译**：必须指定架构和编译器。
    ```bash
    make -C ~/imx/sdk/100ask_imx6ull-sdk/Linux-4.9.88 \
         M=$PWD \
         ARCH=arm \
         CROSS_COMPILE=arm-linux-gnueabihf- \
         modules
    ```

2.  **编译产物**：
    编译成功后，关注以下文件：
    - `*.ko`: 最终的内核模块文件，拷贝到开发板加载。
    - `*.mod.c`: Kbuild 生成的元数据文件（包含版本信息等）。
    - `Module.symvers`: 导出的符号表。
