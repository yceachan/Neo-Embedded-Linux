---
title: Kconfig 语法详解
tags: [Kconfig, Syntax, Menu, Dependency]
update: 2026-02-07

---

# Kconfig 语法详解

Kconfig 语言定义了内核配置菜单的结构（如 `make menuconfig` 看到的界面）。它决定了哪些选项可见、哪些可以被选中，以及它们之间的依赖关系。

## 1. 基本配置项 (Config Entry)

最基础的构建块是 `config` 关键字。

```kconfig
config MODVERSIONS
	bool "Set version information on all module symbols"
	depends on MODULES
	help
	  Usually, modules have to be recompiled whenever you switch to a new
	  kernel.  ...
```

### 1.1 属性详解

*   **类型 (Type)**: 每个 config 必须有一个类型。
    *   `bool`: 布尔值 (y/n)。通常用于“是否启用某功能”。
    *   `tristate`: 三态 (y/m/n)。通常用于驱动程序（支持编译进内核、编译为模块、不编译）。
    *   `string`: 字符串。
    *   `hex`: 十六进制数。
    *   `int`: 整数。

*   **提示文案 (Prompt)**: 跟在类型后面的字符串，或者单独使用 `prompt` 关键字。只有带提示的选项才会显示在菜单中。

*   **默认值 (Default)**:
    ```kconfig
    default y
    ```

*   **帮助信息 (Help)**: 以 `help` 或 `---help---` 开头，缩进结束。

## 2. 依赖关系 (Dependencies)

### 2.1 正向依赖 (`depends on`)
只有当依赖条件满足时，当前选项才可见（或可选中）。

```kconfig
config FOO
    bool "Foo feature"
    depends on BAR && BAZ
```
*   如果 `BAR` 或 `BAZ` 为 `n`，则 `FOO` 不可见。
*   如果 `BAR` 为 `m`，且 `FOO` 是 `tristate`，则 `FOO` 最多只能选 `m`。

### 2.2 反向依赖 (`select`)
强制开启另一个选项。

```kconfig
config USB_GADGET
    select USB_COMMON
```
*   **注意**：`select` 会无视目标选项的 `depends on` 依赖，极其霸道。
*   **最佳实践**：只用于选中那些没有依赖的库功能（library-like symbols）。

## 3. 菜单结构 (Menu Structure)

### 3.1 显式菜单 (`menu` ... `endmenu`)
```kconfig
menu "Network device support"
    depends on NET

config NETDEVICES
    ...

endmenu
```

### 3.2 隐式菜单
如果一个选项依赖于前一个选项，它会自动成为子菜单项。

## 4. Choice 互斥组

用于“多选一”的场景。

```kconfig
choice
    prompt "ARM system type"
    default ARCH_VERSATILE

config ARCH_VERSATILE
    bool "Versatile"

config ARCH_IMX6ULL
    bool "Freescale i.MX6ULL"

endchoice
```

## 5. 常见实战模式

### 5.1 模块独有 (`depends on m`)
限制某个组件只能编译为模块，不能编译进内核。

```kconfig
config FOO
    depends on BAR && m
```

### 5.2 隐藏选项 (无 Prompt)
用于内部逻辑控制，用户不可见。

```kconfig
config HAVE_I2C
    bool
    # 没有提示文本，menuconfig 中不可见
```
通常结合 `select` 使用：平台代码 `select HAVE_I2C`，驱动代码 `depends on HAVE_I2C`。
