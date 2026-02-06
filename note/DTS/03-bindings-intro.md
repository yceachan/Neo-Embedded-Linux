---
title: Bindings 阅读指南 (Bindings Intro)
tags: [DTS, Bindings, Datasheet]
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

在 Linux 4.9 内核中，所有文档位于：
`Documentation/devicetree/bindings/`

按子系统分类：
- `gpio/`: GPIO 控制器
- `i2c/`: I2C 总线及设备
- `input/`: 输入设备（触摸屏、按键）
- `display/`: 显示相关

## 3. 实战剖析：IMX GPIO Binding

我们以 `Documentation/devicetree/bindings/gpio/fsl-imx-gpio.txt` 为例，教你如何“阅读理解”。

### 3.1 必须属性 (Required Properties)

文档中明确列出了 **Required properties**：

```text
Required properties:
- compatible : Should be "fsl,<soc>-gpio"
- reg : Address and length of the register set
- gpio-controller : Marks the device node as a gpio controller.
- #gpio-cells : Should be two.
```

**解读**：
- 缺少任何一个，驱动可能会 Probe 失败。
- `#gpio-cells` 的值为 2，意味着引用它时需要跟 2 个参数（通常是 引脚号 和 极性）。

### 3.2 值的含义

文档解释了魔法数字的含义：

```text
The first cell is the pin number and the second cell is used to specify the gpio polarity:
      0 = active high
      1 = active low
```

**应用**：
当我们看到其他节点引用 GPIO 时：
`reset-gpios = <&gpio1 5 1>;`
- `&gpio1`: 引用 GPIO 控制器节点。
- `5`: 第 1 个 cell，代表引脚号 (GPIO1_IO05)。
- `1`: 第 2 个 cell，代表 Active Low (低电平有效)。

### 3.3 中断控制器特性

该 Binding 还指出 GPIO 控制器本身也是一个中断控制器：

```text
- interrupt-controller: Marks the device node as an interrupt controller.
- #interrupt-cells : Should be 2.
```

这意味着其他设备可以使用 GPIO 引脚作为中断源：
`interrupt-parent = <&gpio1>;`
`interrupts = <28 2>;` (引脚28，触发类型2=高电平下降沿)

## 4. 寻找合适的 Binding

如果你有一个新硬件（例如 AP3216C 光传感器），如何找到它的 Binding？

1.  **全局搜索**：
    ```bash
    grep -r "ap3216c" Documentation/devicetree/bindings/
    ```
2.  **猜测文件名**：
    通常在 `bindings/iio/light/` 或 `bindings/input/misc/` 下。
3.  **参考现有 DTS**：
    搜索 `arch/arm/boot/dts/` 下使用过该芯片的板子。

## 5. 通用属性 (Standard Properties)

有些属性是内核核心层 (Core) 定义的，在具体驱动 Binding 中可能不会重复提及：

- **status**: `"okay"` (启用) 或 `"disabled"` (禁用)。
- **pinctrl-0**, **pinctrl-names**: 引脚复用配置（Pinctrl 子系统）。
- **clocks**: 设备时钟源。
- **reg**: 寄存器物理地址和长度。

## 6. 总结

- **写 DTS 前必查 Binding**。
- **Required 属性一个都不能少**。
- **Cell 的含义看文档解释**。
