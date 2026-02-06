---
title: Kbuild Makefile 核心语法基础
tags: [Kbuild, Makefile, Syntax, obj-y, obj-m]
update: 2026-02-07

---

# Kbuild Makefile 核心语法基础

内核 Makefile 与普通 Makefile 的最大区别在于：**它不是定义如何编译文件，而是定义要构建什么（Goal definitions）**。Kbuild 系统负责处理具体的编译器调用和依赖管理。

## 1. 核心目标定义 (Goal Definitions)

### 1.1 `obj-y` (Built-in Object)
定义要编译进内核镜像 (`vmlinux`) 的目标文件。

```makefile
# 简单的例子
obj-y += foo.o
```
*   **含义**：Kbuild 会寻找 `foo.c` 或 `foo.S`，编译成 `foo.o`。
*   **链接**：该目录下的所有 `obj-y` 文件会被打包成一个 `built-in.o`，供上层目录链接。

### 1.2 `obj-m` (Module Object)
定义要编译成可加载模块 (`.ko`) 的目标文件。

```makefile
# 基于配置选项的动态定义（推荐）
obj-$(CONFIG_MY_DRIVER) += my_driver.o
```
*   如果 `CONFIG_MY_DRIVER=y`，则加入 `obj-y`（编译进内核）。
*   如果 `CONFIG_MY_DRIVER=m`，则加入 `obj-m`（编译为模块）。

### 1.3 `lib-y` (Library)
定义要编译成库文件 (`lib.a`) 的目标。
```makefile
lib-y := delay.o
```
*   该目录下的 `lib-y` 对象会被打包成 `lib.a`。
*   注意：如果一个对象同时出现在 `obj-y` 和 `lib-y` 中，它不会被放入库中（因为已经可访问了）。

## 2. 复合对象 (Composite Objects)

如果一个模块由多个源文件组成，需要使用 `<module_name>-y` 语法。

```makefile
# 定义模块名
obj-$(CONFIG_EXT2_FS) += ext2.o

# 定义模块包含的源文件列表
ext2-y := balloc.o dir.o file.o ialloc.o \
          inode.o ioctl.o namei.o super.o symlink.o
```
*   **原理**：Kbuild 会先编译 `ext2-y` 中的所有文件，然后通过 `$(LD) -r` 将它们合并成 `ext2.o`。

## 3. 递归构建 (Descending)

Makefile 只负责当前目录的构建。要让 Kbuild 进入子目录，需要将子目录名加入 `obj-y` 或 `obj-m`。

```makefile
# fs/Makefile
obj-$(CONFIG_EXT2_FS) += ext2/
```
*   如果 `CONFIG_EXT2_FS` 为 `y` 或 `m`，Kbuild 就会进入 `ext2/` 目录处理其中的 Makefile。
*   这是一个非常高效的机制，确保只有被选中的子系统才会被遍历。

## 4. 特殊规则 (Special Rules)

当标准 Kbuild 机制无法满足需求（如生成代码、处理特定格式）时，可以使用类似标准 Make 的规则，但要注意路径引用。

*   `$(src)`: 指向 Makefile 所在的源码目录（相对路径）。
*   `$(obj)`: 指向目标文件生成的目录（相对路径）。

```makefile
# 示例：自定义生成头文件
$(obj)/generated.h: $(src)/template.h
    $(call if_changed,gen_header)
```

## 5. 总结

| 变量 | 作用 | 典型场景 |
| :--- | :--- | :--- |
| `obj-y` | 编译进内核镜像 | 核心子系统、必须启动的驱动 |
| `obj-m` | 编译为模块 | 可插拔驱动、非必要功能 |
| `foo-y` | 定义复合对象的组件 | 多文件组成的复杂驱动 |
| `obj-$(CONFIG_FOO)` | 根据配置自动切换 y/m | 绝大多数内核 Makefile 的写法 |

```