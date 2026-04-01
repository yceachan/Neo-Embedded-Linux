---
title: GICv2硬件深度：寄存器与裸机初始化
tags: [GIC, GICv2, IMX6ULL, 寄存器, 裸机]
desc: GICv2 Distributor/CPU Interface寄存器操作与IMX6ULL裸机初始化详解
update: 2026-04-01

---


# GICv2硬件深度：寄存器与裸机初始化

> [!note]
> **Ref:** [`docs/ARM® Generic Interrupt Controller(ARM GIC控制器).pdf`](../../../docs/ARM®%20Generic%20Interrupt%20Controller(ARM%20GIC控制器).pdf), [`sdk/.../001_exception/gic.c`](../../../sdk/100ask_imx6ull-sdk/NoosProgramProject/11_GPIO中断/001_exception/gic.c)

## 1. GICv2 内部模块结构

```mermaid
graph TD
    subgraph "GIC 内部"
        subgraph "Distributor (GICD)"
            D_CTRL["GICD_CTLR\n全局使能"]
            D_TYPER["GICD_TYPER\n中断数查询"]
            D_IGROUP["GICD_IGROUPRn\nGroup0/1分配"]
            D_ISENABLE["GICD_ISENABLERn\n使能位"]
            D_ICENABLE["GICD_ICENABLERn\n禁用位"]
            D_IPRI["GICD_IPRIORITYRn\n优先级"]
            D_ITARGET["GICD_ITARGETSRn\n目标CPU"]
            D_ICFG["GICD_ICFGRn\n触发类型"]
        end
        subgraph "CPU Interface (GICC)"
            C_CTRL["GICC_CTLR\nCPU接口使能"]
            C_PMR["GICC_PMR\n优先级掩码"]
            C_BPR["GICC_BPR\n抢占阈值"]
            C_IAR["GICC_IAR\n中断应答(读)"]
            C_EOIR["GICC_EOIR\n中断完成(写)"]
            C_RPR["GICC_RPR\n运行优先级"]
        end
    end

    SPI["SPI 外设"] --> D_CTRL
    D_IPRI --> C_PMR
    C_IAR --> C_EOIR
```

---

## 2. 中断触发类型配置

`GICD_ICFGRn` 每个中断占 2 bit，高位决定触发类型：

| `[1:0]` 值 | 触发类型 | 适用中断 |
|------------|----------|----------|
| `0b00` | **电平敏感** (Level-sensitive) | 大多数 SPI 外设中断 |
| `0b10` | **边沿触发** (Edge-triggered) | GPIO 按键、SGI、LPI |

> SGI 永远是边沿触发，软件不可修改。

---

## 3. 优先级机制

GICv2 使用 **8-bit 优先级**，值越小优先级越高（0x00 最高）。

```
优先级值:  0x00  ...  0xFF
              ↑              ↑
            最高            最低
```

**`GICC_PMR`（优先级掩码）：** 只有优先级值 **低于** PMR 的中断（即数值小于PMR）才会被发送给 CPU。

```c
/* IMX6ULL 裸机：使能所有优先级（设置为最低优先级阈值）
 * 5位实现 → 有效位在高位，需左移 (8-5)=3 位
 */
gic->C_PMR = (0xFF << (8 - 5)) & 0xFF;  // = 0xF8

/* 二进制分组：全部位用于优先级，不分子优先级 */
gic->C_BPR = 7 - 5;  // = 2
```

---

## 4. 裸机 GIC 初始化流程（参考 SDK gic.c）

```mermaid
sequenceDiagram
    autonumber
    participant SW as "裸机代码"
    participant GICD as "Distributor"
    participant GICC as "CPU Interface"

    SW->>SW: get_gic_base() 读CP15获取基地址
    SW->>GICD: 读GICD_TYPER，获取最大中断数
    SW->>GICD: 写GICD_ICENABLERn 禁用所有中断
    SW->>GICC: 写GICC_PMR = 0xF8 (开放所有优先级)
    SW->>GICC: 写GICC_BPR = 2 (无子优先级分组)
    SW->>GICD: 写GICD_CTLR = 1 (使能Distributor)
    SW->>GICC: 写GICC_CTLR = 1 (使能CPU Interface)
    Note over SW: 初始化完成，可开始注册中断
```

**读取 GIC 基地址（CP15 协处理器）：**

```c
GIC_Type *get_gic_base(void)
{
    GIC_Type *dst;
    /* CBAR: Configuration Base Address Register (CP15, c15, c0, 0)
     * 返回 GIC 物理基地址（= Distributor 基地址）
     */
    __asm volatile ("mrc p15, 4, %0, c15, c0, 0" : "=r" (dst));
    return dst;
}
```

---

## 5. 使能/禁用单个中断（位操作）

GICv2 使用**位图**管理每个中断的使能状态，每32个中断共用一个32-bit寄存器。

```c
/* 使能中断 nr */
void gic_enable_irq(IRQn_Type nr)
{
    GIC_Type *gic = get_gic_base();
    /* nr >> 5  : 确定是第几个 ISENABLER 寄存器（哪个 32-bit word）
     * nr & 0x1F: 确定寄存器内的位偏移（0-31）
     * 写 1 置位 = 使能；写 0 无效（Set-enable 语义）
     */
    gic->D_ISENABLER[nr >> 5] = (1UL << (nr & 0x1FUL));
}

/* 禁用中断 nr */
void gic_disable_irq(IRQn_Type nr)
{
    GIC_Type *gic = get_gic_base();
    /* ICENABLER：写 1 清除使能位（Clear-enable 语义）*/
    gic->D_ICENABLER[nr >> 5] = (1UL << (nr & 0x1FUL));
}
```

> **Set/Clear 语义**：GIC 使用两套寄存器分别执行使能和禁用，避免读-改-写竞争。这是 ARM 外设寄存器的通用设计模式。

---

## 6. 裸机中断处理 dispatch

```c
void handle_irq_c(void)
{
    GIC_Type *gic = get_gic_base();

    /* Step 1: 读 GICC_IAR → 认领中断，获取 INTID
     * 此读操作同时将中断状态从 Pending → Active
     */
    int nr = gic->C_IAR;

    /* Step 2: 通过 irq_table 查找并调用注册的处理函数 */
    irq_table[nr].irq_handler(nr, irq_table[nr].param);

    /* Step 3: 写 GICC_EOIR → 通知 GIC 处理完成
     * 将中断状态从 Active → Inactive，允许下次触发
     */
    gic->C_EOIR = nr;
}
```

---

## 7. IMX6ULL GIC 内存映射地址

根据 IMX6ULL 参考手册（Chapter 3: Memory Map）：

| 模块 | 基地址 | 大小 |
|------|--------|------|
| GIC Distributor (GICD) | `0x00A01000` | 4KB |
| GIC CPU Interface (GICC) | `0x00A02000` | 8KB |

> 在 Linux 内核中，这些地址通过 DTS 描述并由 `irq-gic.c` 驱动的 `of_iomap()` 映射到虚拟地址空间。

```dts
/* arch/arm/boot/dts/imx6ull.dtsi */
intc: interrupt-controller@00a01000 {
    compatible = "arm,cortex-a7-gic";
    #interrupt-cells = <3>;
    interrupt-controller;
    reg = <0x00a01000 0x1000>,   /* GICD */
          <0x00a02000 0x2000>;   /* GICC */
};
```

---

## 8. 中断状态机

每个中断在 GIC 内部有 4 种状态：

```mermaid
stateDiagram-v2
    [*] --> Inactive : 复位/EOI写入
    Inactive --> Pending : 外设触发 / 软件写ISPENDR
    Pending --> Active : CPU 读 GICC_IAR (Acknowledge)
    Active --> Inactive : CPU 写 GICC_EOIR (EOI)\n非电平中断
    Pending --> ActiveAndPending : CPU 读 GICC_IAR 时\n电平中断仍有效
    ActiveAndPending --> Pending : CPU 写 GICC_EOIR
    Active --> ActiveAndPending : 电平中断重新 Assert
```

> **电平中断**：只要电平持续，中断会在 EOI 后重新进入 Pending。内核流控层（`handle_level_irq`）会在 ISR 执行前先 **Mask** 该中断，ISR 清除外设标志后再 **Unmask**，防止重入。
