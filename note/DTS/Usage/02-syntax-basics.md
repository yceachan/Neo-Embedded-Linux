---
title: DTS 语法基础 (Syntax Basics)
tags: [DTS, Syntax, Reference]
desc: 介绍设备树的语法基础，包括节点、属性的结构，引用与覆盖，以及头文件包含机制。
update: 2026-02-07

---


# DTS 语法基础 (Syntax Basics)

DTS (Device Tree Source) 是一种 ASCII 文本格式，语法结构类似于 C 语言，但主要描述数据而非逻辑。

## 1. 基本结构 (Structure)

DTS 是一个树状结构，由 **节点 (Node)** 和 **属性 (Property)** 组成。

### 1.1 节点 (Nodes)
节点格式为 `[label:] node-name[@unit-address] { ... };`。

```dts
/ {  /* 根节点 */
    cpus {
        cpu0: cpu@0 {  /* label: name@address */
            device_type = "cpu";
        };
    };
};
```
- **node-name**: 节点名称，通常是设备类型的通用名称（如 `cpu`, `ethernet`, `i2c`），而不是具体型号。
- **unit-address**: 单元地址，通常对应设备的寄存器基地址（如 `@0`）。必须与 `reg` 属性的第一个地址匹配。
- **label**: 标签（如 `cpu0`），用于在其他地方快速引用该节点（例如 `&cpu0`）。

### 1.2 属性 (Properties)
属性是键值对，描述节点的具体信息。

| 类型 | 示例 | 说明 |
| :--- | :--- | :--- |
| **Empty** | `interrupt-controller;` | 空属性，仅表示标志位（True/False） |
| **U32** | `clock-latency = <61036>;` | 32位整数，使用尖括号 `< >` |
| **String** | `status = "okay";` | 字符串，使用双引号 `" "` |
| **String List** | `compatible = "fsl,imx6ull", "fsl,imx6ul";` | 字符串列表，逗号分隔 |
| **Byte Data** | `local-mac-address = [00 00 00 00 00 00];` | 二进制数据，使用方括号 `[ ]` |
| **Phandle** | `clocks = <&clks IMX6UL_CLK_ARM>;` | 引用其他节点句柄（phandle） |

## 2. 引用与覆盖 (References & Overlays)

这是 DTS 最强大的特性之一，允许在不修改原始节点结构的情况下修改其属性。

### 2.1 引用 (Reference)
使用 `&label` 引用一个节点。
```dts
/* 在 imx6ull.dtsi 中定义 */
i2c1: i2c@21a0000 { ... };

/* 在板级 dts (100ask_imx6ull.dts) 中引用并开启 */
&i2c1 {
    status = "okay";  /* 覆盖默认的 "disabled" */
    clock-frequency = <100000>;
};
```

### 2.2 别名 (Aliases)
`/aliases` 节点将长路径映射为短 ID，常用于驱动中通过 ID 获取设备（如 `i2c0`, `serial1`）。
```dts
aliases {
    serial0 = &uart1;
    gpio0 = &gpio1;
};
```

## 3. 头文件包含 (Includes)

DTS 支持 C 语言风格的预处理。

- **`#include <...>`**: 包含 C 头文件（定义宏）。
    - 这里的宏用于将魔术数字转换为可读名称，例如 `IMX6UL_CLK_ARM` 或 `GPIO_ACTIVE_LOW`。
    - 路径通常位于 `include/dt-bindings/`。
- **`#include "..."`**: 包含其他 `.dtsi` (Include) 文件。
    - `.dtsi` 文件包含 SoC 级别的通用定义（如 CPU, 中断控制器, 内部外设）。
    - `.dts` 文件包含板级特定的定义（如 LED, 按键, 外接传感器）。

## 4. 常见问题 (FAQs)

### Q: `< >` 和 `[ ]` 可以混用吗？
A: 不建议。`< >` 是 32-bit cell 数组，`[ ]` 是 byte 数组。虽然有些编译器支持混合解析，但语义不同。

### Q: `compatible` 属性的作用？
A: 用于驱动匹配。格式通常为 `"厂商,芯片型号"`。内核会将 DTS 中的 compatible 字符串与驱动源码中的 `of_match_table` 进行比对。

### Q: `#address-cells` 和 `#size-cells` 是什么？
A: 它们决定了子节点 `reg` 属性的格式。
- `#address-cells = <1>`: 地址占 1 个 cell (32-bit)。
- `#size-cells = <1>`: 长度占 1 个 cell。
- `reg = <addr length>`: 于是 `reg` 就解析为 `address` 和 `length` 两个数。