---
title: Device Tree (DTS) 知识库治理计划
tags: [DTS, Plan, Kernel]
update: 2026-02-07
---

# Device Tree (DTS) 知识库治理计划

本计划旨在构建一套适用于嵌入式 Linux (IMX6ULL/Linux 4.9) 的设备树知识体系，覆盖语法基础、内核解析机制、驱动绑定（Bindings）及动态叠加（Overlay）。

## 1. 资源映射 (Resource Map)

| 主题 | 本地文档 (Linux 4.9) | 在线 Wiki (Kernel.org) | 说明 |
| :--- | :--- | :--- | :--- |
| **Usage Model** | `usage-model.txt` | [Devicetree Usage](https://docs.kernel.org/devicetree/usage-model.html) | 核心概念、生命周期 |
| **Bindings** | `bindings/` | [Devicetree Bindings](https://docs.kernel.org/devicetree/bindings/index.html) | 硬件描述协议 |
| **Overlay** | `overlay-notes.txt` | [Devicetree Overlays](https://docs.kernel.org/devicetree/overlay-notes.html) | 运行时修改 DT |
| **Booting** | `booting-without-of.txt` | N/A | 启动传参协议 |

## 2. 知识库结构 (Structure)

本目录 `note/DTS/` 将包含以下核心文档：

- **00-governance-plan.md**: 本索引文件。
- **01-usage-model.md**: **[基础]** 什么是设备树，它在内核启动中的作用（Platform Identification, Runtime Config）。
- **02-syntax-basics.md**: **[语法]** 节点（Node）、属性（Property）、标签（Label）、引用（Phandle）与头文件包含。
- **03-bindings-intro.md**: **[协议]** 如何阅读和编写 Binding 文档（`compatible`, `reg`, `interrupts` 等标准属性）。
- **04-addressing.md**: **[进阶]** 地址转换机制 (`#address-cells`, `#size-cells`, `ranges`)。
- **05-overlays.md**: **[高阶]** 运行时动态加载设备树插件（.dtbo）。
- **100-dts-to-driver.md**: **[实战]** (已存在) 从 DTS 节点到 Driver Probe 的完整代码链路。

## 3. 执行策略

1.  **Extract**: 从 `usage-model.txt` 提取核心逻辑。
2.  **Synthesis**: 结合 IMX6ULL 实际 DTS (`arch/arm/boot/dts/imx6ull.dtsi`) 进行案例分析。
3.  **Visual**: 使用 Mermaid 绘制解析流程。

## 4. 进度追踪

- [x] 00-governance-plan.md
- [ ] 01-usage-model.md
- [ ] 02-syntax-basics.md
- [ ] 03-bindings-intro.md
- [ ] 05-overlays.md
