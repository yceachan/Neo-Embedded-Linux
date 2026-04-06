---
title: DTS 编码风格指南 (Coding Style)
tags: [DTS, Coding Style, Linux Kernel]
desc: 总结 Linux Kernel 官方文档中关于 Device Tree 源码编写的规范和最佳实践。
update: 2026-04-07

---


# DTS 编码风格指南 (Coding Style)

> [!note]
> **Ref:** 
> - [Linux Kernel: Devicetree Sources (DTS) Coding Style](https://docs.kernel.org/devicetree/bindings/dts-coding-style.html)

为了保证设备树文件（DTS/DTSI）的可读性和在整个社区生态中的一致性，Linux 内核制定了严格的编码风格指南。以下是核心规则的梳理：

## 1. 命名规范 (Naming)

*   **节点与属性名**：只能使用小写字母 `[a-z]`、数字 `[0-9]` 和短划线 `-`。**绝对不要使用下划线 `_`**。
*   **标签名 (Labels)**：只能使用小写字母 `[a-z]`、数字 `[0-9]` 和下划线 `_`。**不要使用短划线 `-`**。
    *   *示例*：`pinctrl_i2c1: i2c1grp { ... };` (标签用 `_`，节点名称及属性名用小写或 `-`)
*   **单元地址 (Unit Addresses) 与十六进制**：必须使用小写十六进制，且**不要包含前导零**（Leading Zeros），除非是在特定的属性值中为了强制对齐需要填充。
    *   *正确示例*：`i2c@21a0000` 
    *   *错误示例*：`i2c@021a0000` (虽然在旧代码和旧版本 SoC 配置中常见，但新的编写规范已不推荐)

## 2. 节点与属性的顺序 (Ordering)

节点内的属性必须按照特定的业务逻辑顺序排列，以便其他开发者快速抓取重点信息：

1.  `compatible` (永远放在第一位，表明设备身份)
2.  `reg` (基础的寄存器资源配置)
3.  `ranges`
4.  标准的或通用的属性 (如 `interrupts`, `clocks`, `#address-cells`, `#size-cells`)
5.  厂商自定义属性 (如 `fsl,...`, `goodix,...`)
6.  *空行* (推荐在 status 前留出空行进行区隔)
7.  `status` (通常放在属性列表的最后，紧跟在后续子节点之前)

**同级节点的排列顺序**：
*   **带有单元地址的节点**：按照地址从小到大（升序）排列。
*   **不带单元地址的节点**：按照字母数字顺序排列。

*标准示例结构*：
```dts
i2c@21a0000 {
    compatible = "fsl,imx6ul-i2c";
    reg = <0x21a0000 0x4000>;
    interrupts = <GIC_SPI 36 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&clks IMX6UL_CLK_I2C1>;
    #address-cells = <1>;
    #size-cells = <0>;

    status = "okay";

    /* 内部子节点 */
    codec@1a {
        ...
    };
};
```

## 3. 格式化与缩进 (Formatting)

*   **缩进**：遵循 Linux Kernel 标准的缩进风格（通常为单个 Tab，视项目整体的代码风格配置而定）。
*   **数组换行**：当属性的值是一个包含多个元素的数组时，在逻辑边界处应换行。并且，多 Cell 的条目应该使用一对独立的尖括号 `< >` 包裹，以提高复杂数据的结构化表现力。
    *   *示例*：
        ```dts
        clocks = <&clks IMX6UL_CLK_I2C1>,
                 <&clks IMX6UL_CLK_I2C2>;
        ```

## 4. 文件结构与组织架构 (Structure)

DTS 文件应按模块化和可重用的思路进行分层组织：

*   **SoC 级定义 (`.dtsi`)**：存放 CPU/SoC 内部硬件控制器和核心架构的定义（如 `imx6ull.dtsi`）。
*   **核心板级/SoM 级定义 (`.dtsi`)**：如果实际硬件是由核心板（System-on-Module）+ 底板组成的形态，核心板的定义应被独立出来。
*   **具体开发板定义 (`.dts`)**：板级特有硬件（外设、LED、按键、具体的引脚复用分配）必须放在最终的 `.dts` 文件中。**核心原则：避免将具体的板级配置或状态修改“污染”到通用的 SoC `.dtsi` 文件中**。
