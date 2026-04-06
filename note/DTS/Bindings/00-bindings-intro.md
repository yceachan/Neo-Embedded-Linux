---
title: Bindings 阅读指南 (Bindings Intro)
tags: [DTS, Bindings, Datasheet]
desc: 介绍设备树绑定 (Device Tree Binding) 的基本概念、文档位置以及如何阅读和使用它。
update: 2026-02-07

---


# Bindings 阅读指南 (Bindings Intro)

设备树绑定 (Device Tree Binding) 是驱动程序与设备树之间的“法律合同”。它规定了某个特定的硬件节点 **必须** 提供哪些属性，以及这些属性的数据格式。

## 1. 为什么要读 Bindings?

当你在编写 DTS 时，你可能会疑惑：
- `compatible` 到底该写什么？
- `reg` 需要几个地址？
- 某个属性的值（如 `0x1`）代表什么含义？

答案都在 Binding 文档中。驱动代码通常只负责实现逻辑，而具体的配置格式由 Binding 文档定义。

## 2. 文档位置

在 Linux 内核中（如 4.9 版本），所有文档通常位于：
`Documentation/devicetree/bindings/`

按子系统分类：
- `gpio/`: GPIO 控制器
- `i2c/`: I2C 总线及设备
- `input/`: 输入设备（如触摸屏、按键）
- `display/`: 显示相关

## 3. 实战剖析：IMX GPIO Binding

我们以 `Documentation/devicetree/bindings/gpio/fsl-imx-gpio.txt` 为例，教你如何“阅读理解”。

### 3.1 必须属性 (Required Properties)

文档中明确列出了 **Required properties**（必须包含的属性）：

```text
Required properties:
- compatible : Should be "fsl,<soc>-gpio"
- reg : Address and length of the register set
- gpio-controller : Marks the device node as a gpio controller.
- #gpio-cells : Should be two.
```

**解读**：
- 缺少上述任何一个属性，驱动都可能会导致 Probe（探测）失败。
- `#gpio-cells` 的值为 `2`，这意味着在其他节点引用此 GPIO 时，需要提供 2 个参数（通常是**引脚号**和**极性**）。

### 3.2 值的含义

文档还会解释各个配置参数（魔法数字）的具体含义：

```text
The first cell is the pin number and the second cell is used to specify the gpio polarity:
      0 = active high
      1 = active low
```

**应用实例**：
当我们看到其他节点引用该 GPIO 控制器时：
`reset-gpios = <&gpio1 5 1>;`
- `&gpio1`: 引用 GPIO 控制器节点（Phandle）。
- `5`: 第 1 个 cell，代表引脚号（如 GPIO1_IO05）。
- `1`: 第 2 个 cell，代表 Active Low (低电平有效)。

### 3.3 中断控制器特性

该 Binding 还指出 GPIO 控制器本身也可以作为一个中断控制器：

```text
- interrupt-controller: Marks the device node as an interrupt controller.
- #interrupt-cells : Should be 2.
```

这意味着其他设备可以使用 GPIO 引脚作为触发中断的源头：
`interrupt-parent = <&gpio1>;`
`interrupts = <28 2>;` （引脚 28，触发类型 2 代表高电平下降沿）

## 4. 寻找合适的 Binding

如果你有一个新硬件（例如 AP3216C 光传感器），如何找到它的 Binding？

1.  **全局搜索**：
    ```bash
    grep -r "ap3216c" Documentation/devicetree/bindings/
    ```
2.  **猜测文件名**：
    通常可能会在 `bindings/iio/light/` 或 `bindings/input/misc/` 目录下。
3.  **参考现有 DTS**：
    搜索 `arch/arm/boot/dts/` 目录下曾经使用过该芯片的其他开发板的设备树配置。

## 5. 通用属性 (Standard Properties)

有些属性是由内核核心层 (Core) 统一规范和定义的，在具体设备的 Binding 文档中可能不会重复提及：

- **status**: `"okay"` (启用设备) 或 `"disabled"` (禁用设备)。
- **pinctrl-0**, **pinctrl-names**: 引脚复用配置（隶属于 Pinctrl 子系统）。
- **clocks**: 设备所需的时钟源。
- **reg**: 设备的寄存器物理基地址和空间长度。

## 6. 总结

- **写 DTS 前必查 Binding**：确保配置属性与驱动要求严格一致。
- **Required 属性一个都不能少**：遗漏会导致驱动加载失败。
- **Cell 的含义看文档解释**：切忌死记硬背，一切以 Binding 说明为准。
