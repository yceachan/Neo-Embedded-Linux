---
title: 编写 Binding 的 DOs and DON'Ts
tags: [DTS, Binding, Linux Kernel, Best Practices]
desc: 总结 Linux Kernel 官方关于编写 Device Tree Bindings 的核心准则和避坑指南。
update: 2026-04-07

---


# 编写 Binding 的 DOs and DON'Ts

> [!note]
> **Ref:** 
> - [Linux Kernel: DOs and DON'Ts for designing and writing Devicetree bindings](https://docs.kernel.org/devicetree/bindings/writing-bindings.html)

设备树（Device Tree）的核心哲学是**描述硬件本身**，而不是描述软件层面的驱动应当如何工作。编写 Binding 时，必须严格遵循向后兼容（ABI 稳定性）和硬件无关性两大原则。以下是官方推荐的 "DOs and DON'Ts"：

## 核心原则：描述硬件，而非软件

*   **硬件视角**：Binding 必须纯粹基于硬件规格书（Datasheet / Reference Manual）来客观描述设备的物理组成。
*   **完整性**：即使某些硬件特性当前的操作系统或驱动暂未支持开发，也应当在 Binding 的设计阶段进行全面且完整的描述。

## ✅ DOs (必须做)

1.  **具体化 `compatible` 字符串**：
    *   必须使用特定的厂商前缀（Vendor Prefix，如 `fsl,`）。
    *   尽可能具体到具体的芯片型号（例如使用 `fsl,imx6ull-i2c` 而不仅仅是粗略的 `fsl,imx-i2c`），因为硬件小版本之间的细微差异常常在开发后期才会完全暴露出来。
2.  **明确 Phandle 的顺序与定义**：
    *   对于包含多个条目的属性（如 `clocks`, `interrupts`, `resets`），必须在 Binding 文档中明确规定它们的排列和对应顺序。
    *   配合使用 `-names` 后缀属性（如 `clock-names`, `interrupt-names`）来消除在引用时的歧义。
3.  **严格的属性约束**：
    *   在文档中明确定义各个属性的数据类型、有效取值范围或可供选择的枚举值。
4.  **维持 ABI 稳定性**：
    *   Device Tree 是一份与内核代码库解耦的独立硬件描述，**Binding 本质上是一种 ABI（应用程序二进制接口）**。旧的设备树（通常烧录在固件或 ROM 中）必须能够和新的内核版本一起正常工作，因此绝对不能随意破坏或变更已有的 Binding 结构和属性含义。
5.  **使用标准单位后缀**：
    *   在描述物理量（如时间、电压、电流等）时，相关的属性命名应当带有标准的单位后缀（例如 `-microvolt`, `-ms`, `-hz` 等），以防出现理解偏差。

## ❌ DON'Ts (绝对禁止)

1.  **禁止提及 Linux 驱动或 OS 概念**：
    *   设备树跨越操作系统的边界（可用在 U-Boot, Linux, FreeBSD, Zephyr 等不同环境）。在 Binding 文档中绝不能出现类似于“用于触发 xxx 驱动模块”、“配置 Linux 子系统”等特定 OS 下的实现字眼。
2.  **禁止在 `compatible` 中使用通配符**：
    *   **不要**使用像 `fsl,imx6*-i2c` 或 `fsl,imx<soc>-i2c` 这样的通配符表达。必须明确穷举支持的型号，或者使用已知最老且兼容的型号作为 Fallback 机制的备选。
3.  **禁止为了“实例化驱动”而凭空捏造节点**：
    *   设备树中的每一个节点都必须直接对应真实的物理硬件实体、总线或内部逻辑 IP 核。绝不能仅仅为了让 Linux 内核顺利加载某个虚拟设备驱动（比如纯软件形式的轮询任务或抽象层）而在 DTS 中凭空捏造出一个不存在的硬件节点（此类情况应由内核内部的纯软件逻辑或模块处理）。
