---
title: Kbuild 编译选项控制
tags: [Kbuild, Flags, CFLAGS, LDFLAGS, Compiler]
update: 2026-02-07

---

# Kbuild 编译选项控制

在内核开发中，经常需要针对特定目录或文件添加编译器标志（如头文件搜索路径、宏定义、警告控制）。Kbuild 提供了一套标准的变量体系来实现这一目标。

## 1. 目录级选项 (当前 Makefile)

以下变量仅对当前 Makefile 中的目标生效：

### 1.1 `ccflags-y` (C Compiler Flags)
用于指定 C 编译器的选项。替代了已弃用的 `EXTRA_CFLAGS`。

```makefile
# 添加头文件搜索路径 (-I)
# 注意：-I 与路径之间不能有空格
ccflags-y := -I$(src)/include

# 定义宏
ccflags-$(CONFIG_DEBUG_INFO) += -DDEBUG_MY_DRIVER
```

### 1.2 `ldflags-y` (Linker Flags)
用于指定链接器的选项。

```makefile
# 指定自定义的链接脚本
ldflags-y += -T $(src)/custom.lds
```

### 1.3 `asflags-y` (Assembler Flags)
用于指定汇编器的选项。

## 2. 递归子目录选项 (`subdir-*`)

如果希望选项不仅对当前目录生效，还自动传递给所有子目录，使用 `subdir-` 前缀。

```makefile
# 对当前目录及所有子目录开启 -Werror
subdir-ccflags-y := -Werror
```

## 3. 文件级选项 (Per-file Flags)

针对特定源文件设置选项。变量名为 `CFLAGS_<文件名.o>`。

```makefile
# 仅针对 main.c (编译生成的 main.o) 定义宏
CFLAGS_main.o := -DENABLE_SPECIAL_FEATURE

# 仅针对 lowlevel.S 使用特定的 CPU 架构参数
AFLAGS_lowlevel.o := -Wa,-mcpu=cortex-a7
```

## 4. 智能检测编译器能力

不同版本的 GCC/Clang 支持的参数可能不同。Kbuild 提供了一系列函数来动态检测编译器能力，避免构建失败。

### 4.1 `cc-option`
检查编译器是否支持某选项，支持则返回该选项，否则返回备选值（可选）。

```makefile
# 如果编译器支持 -Wno-maybe-uninitialized 则使用，否则不使用
ccflags-y += $(call cc-option,-Wno-maybe-uninitialized)

# 如果支持 -march=armv7-a 则使用，否则回退到 -march=armv6
ccflags-y += $(call cc-option,-march=armv7-a,-march=armv6)
```

### 4.2 `cc-disable-warning`
专门用于禁用警告的辅助函数。

```makefile
# 安全地禁用 unused-variable 警告
ccflags-y += $(call cc-disable-warning, unused-variable)
```

### 4.3 `as-option` / `ld-option`
类似于 `cc-option`，分别用于检测汇编器和链接器的选项支持情况。

## 5. 常见实战场景

### 场景一：添加多个私有头文件目录
```makefile
# 引用 drivers/my_drv/include 和 drivers/my_drv/arch/arm/include
ccflags-y := -I$(src)/include \
             -I$(src)/arch/arm/include
```

### 场景二：根据内核版本条件编译
```makefile
# 检查 GCC 版本，如果 >= 4.0 则添加特定优化
cflags-y += $(shell \
    if [ $(cc-version) -ge 0400 ] ; then \
        echo "-O3"; \
    fi ;)
```

## 6. 总结表

| 变量名 | 作用范围 | 用途 | 备注 |
| :--- | :--- | :--- | :--- |
| `ccflags-y` | 当前目录 | C 编译选项 | 推荐使用，替代 EXTRA_CFLAGS |
| `subdir-ccflags-y` | 当前及子目录 | C 编译选项 | 慎用，影响范围大 |
| `CFLAGS_foo.o` | 单个文件 | C 编译选项 | 精确控制 |
| `ldflags-y` | 当前目录 | 链接选项 | 常用于指定 LDS 文件 |

```