# RK3566 GMAC1 RGMII 千兆大包丢包 —— tx_delay / rx_delay 怎么调

## 结论先行

**不要拍脑袋写 `tx_delay` / `rx_delay`，要用 Rockchip 给的 `dwmac-rk-tool` 在线扫描出当前硬件的 pass 窗口中心，再回写 DTS。** 范围 `0x00~0x7f`，步进约 150 ps；RTL8211F 这类 PHY 自带 RX delay 时必须把 SoC 端切成 `phy-mode = "rgmii-rxid"` 并注释掉 `rx_delay`。验证分两层：先软件层 `ping -M do -s 1472` + `iperf3` 双向 + `ethtool -S` 看错误计数；窗口仍很小或软件层不过再上示波器，看 PHY 端 RGMII 时钟占空比 45%~55%、TXC 与 TXD 相位差 1.5~2 ns。

---

## 一、调参流程（按这个顺序，别跳步）

### 1. 确认内核里已编入 dwmac-rk-tool

代码在 `kernel-6.1/drivers/net/ethernet/stmicro/stmmac/dwmac-rk-tool.c`。Kernel ≥ 4.19 默认带；老内核要找 Rockchip 要 `Rockchip_RGMII_Delayline_Kernel*.tar.gz` 补丁。

编入后会在 GMAC1 的 platform device 目录下生成 sysfs 节点（RK3566 的 GMAC 寄存器基址在 `0xfe2a0000` 一带，按 `dmesg | grep gmac` 实际看）：

```
/sys/devices/platform/fe2a0000.ethernet/
├── phy_lb            # 单点 loopback 测试
├── phy_lb_scan       # 扫描整个 delayline 矩阵
└── rgmii_delayline   # 写入 (tx rx) 在线生效
```

没看到这几个节点说明补丁/Kconfig 没打进去。

### 2. 千兆扫描 pass 窗口

> RTL8211E 测试时必须**拔网线**，其他 PHY 一般不用拔。

```bash
# 千兆扫描（一定要 1000，不要用 100；百兆窗口几乎全 pass，参考价值低）
echo 1000 > /sys/devices/platform/fe2a0000.ethernet/phy_lb_scan
```

输出是一张矩阵：横轴 `TX delay 0x00~0x7f`，纵轴 `RX delay 0x00~0x7f`，`O` = pass，空白 = fail。命令末尾会自动打印窗口中心坐标，例如 `center tx=0x2e rx=0x0f`。窗口越大说明硬件冗余度越好；窗口小（< 5×5 格）多半是硬件信号有问题，调参救不回来。

### 3. 在线写入中心值，做一次 loopback 验证

```bash
echo 0x2e 0x0f > /sys/devices/platform/fe2a0000.ethernet/rgmii_delayline
cat /sys/devices/platform/fe2a0000.ethernet/rgmii_delayline
echo 1000 > /sys/devices/platform/fe2a0000.ethernet/phy_lb   # 必须 pass
```

`phy_lb` pass 才能进入下一步；不 pass 说明这个坐标其实不行，回 Step 2 再选。

### 4. 写回 DTS，重烧固件复测

把扫出来的中心值填到 GMAC1 节点的 `tx_delay` / `rx_delay`（语法见下一节），编译烧机后跑 `ping -M do -s 1472` 与 `iperf3` 复测。

### 5.（可选）打开自动扫描

当一批板子硬件离散较大、希望开机自动扫一次：

```
Device Drivers →
  Network device support →
    Ethernet driver support →
      [*] Auto search rgmii delayline   (CONFIG_DWMAC_RK_AUTO_DELAYLINE)
```

首次开机扫一次，结果存到 vendor storage，之后开机直接覆盖 DTS。**vendor storage 不擦不会重扫。** 文档明确说窗口本身就小的板子，开这个宏也救不了，要先解决硬件。

---

## 二、DTS 配置示例（RK3566/RK3568 GMAC1 RGMII）

RK3566 只有一路 GMAC（在 DTS 中命名为 `&gmac1`，引脚组通常是 `gmac1m0` / `gmac1m1`，按硬件选）。下面以 **PHY 不带内部 RX delay** 的常规 RGMII，clock 由 SoC 输出（`clock_in_out = "output"`，最常见）为例：

```dts
&gmac1 {
    phy-mode = "rgmii";
    clock_in_out = "output";

    snps,reset-gpio = <&gpio2 RK_PD1 GPIO_ACTIVE_LOW>;
    snps,reset-active-low;
    snps,reset-delays-us = <0 20000 100000>;   /* RTL8211F 复位需 100 ms */

    assigned-clocks      = <&cru SCLK_GMAC1_RX_TX>, <&cru SCLK_GMAC1>, <&cru CLK_MAC1_OUT>;
    assigned-clock-parents = <&cru SCLK_GMAC1_RGMII_SPEED>;
    assigned-clock-rates = <0>, <125000000>, <25000000>;

    pinctrl-names = "default";
    pinctrl-0 = <&gmac1m1_miim
                 &gmac1m1_tx_bus2
                 &gmac1m1_rx_bus2
                 &gmac1m1_rgmii_clk
                 &gmac1m1_rgmii_bus
                 &eth1m1_pins>;

    /* === 关键：填你扫出来的窗口中心 === */
    tx_delay = <0x4f>;   /* RK3568 EVB 参考值，必须按你板子重扫 */
    rx_delay = <0x26>;

    phy-handle = <&rgmii_phy1>;
    status = "okay";
};

&mdio1 {
    rgmii_phy1: phy@0 {
        compatible = "ethernet-phy-ieee802.3-c22";
        reg = <0x0>;
        clocks = <&cru CLK_MAC1_OUT>;
    };
};
```

### 如果 PHY 是 RTL8211F（自带 RX delay，约 2 ns）

```dts
&gmac1 {
    phy-mode = "rgmii-rxid";   /* 关 SoC 内部 RX delay，避免双重补偿 */
    /* ... 其它字段同上 ... */
    tx_delay = <0x43>;         /* 仍需扫描得到 */
    /* rx_delay = <0x26>; */   /* 一定要注释/删除 */
};
```

文档中给的 RK3568 EVB 参考起点是 `tx_delay = <0x4f>; rx_delay = <0x26>;`，仅作为你扫描脚本的"种子"，不要直接抄上线。

---

## 三、怎么验证调对了

### 3.1 软件层（必做，先做）

```bash
# 1) 链路确认
ethtool eth0 | grep -E "Speed|Duplex"
#   期望: Speed: 1000Mb/s  Duplex: Full

# 2) 大包不分片 ping —— 复现"大包丢包"现象的核心命令
ping -M do -s 1472 -c 500 <peer>
#   期望: 0% packet loss

# 3) 双向吞吐
iperf3 -c <peer> -t 30
iperf3 -c <peer> -t 30 -R       # 反向也要跑
#   期望: 两个方向都 >= 940 Mbps（TCP）

# 4) MAC 误码统计 —— 最敏感的窗口偏移指示器
ethtool -S eth0 | grep -Ei "error|crc|drop|fifo|underrun"
#   rx_crc_errors / rx_fifo_errors 持续涨 -> rx_delay 不在窗口
#   tx_underrun / tx_carrier_errors 涨   -> tx_delay 不对或 PHY 端时序问题
```

判定标准：上述四项都过 = 调对了，可以收工。

### 3.2 示波器层（窗口小、软件层不过、或要量产复核时再做）

文档原文要求（PDF p.8-9）：

- **示波器带宽**：≥ 125 MHz × 5 = **≥ 625 MHz**。125 MHz 是 RGMII 千兆时钟，5 倍是为了捕到方波边沿的谐波。
- **探头**：差分探头最佳；用单端探头时地线尽量短（< 5 mm），否则量出来全是地回路噪声。
- **测点**：靠近 **PHY 端**（接收端）的 `MAC_CLK`、`TX_CLK`、`RX_CLK`。**不要在 SoC 发送端测**——反射严重，量不出真实信号质量。
- **判据 1（基础信号质量）**：波形应为方波而非正弦波；**占空比 45%~55%**。如果是正弦波且占空比 50%，基本是带宽不够或探头方法不对。
- **判据 2（千兆时序）**：测 TXC 与任一根 TXD 的相位差，**目标 1.5~2 ns**（RGMII v2.0 规范是 1~2.6 ns，留余量）。相位不对就用 `echo 0xXX 0xYY > rgmii_delayline` 试，**每次 step 5 由 0x10 向上扫，找到 iperf 能跑过 900 M 的区段后，再以 step 2 细调**，取这个区段的中位写回 DTS（这是 PDF p.9 在 PHY 无 loopback 时的兜底流程）。

### 3.3 信号质量不够时的硬件级补救（文档原话）

- `TX_CLK` 边沿过缓：先看 IO 驱动强度有没有拉到最大；不行就发送端串高频电感整形，或加大原本的 22 Ω 串阻。
- 占空比超出 45%~55%：在 `MAC_CLK` 路径做分压（串 100 Ω 配下地电阻），下地电阻从 100 Ω 起向上调，盯示波器调到合规。
- 接收端 `RX_CLK` 信号差：一般 PHY 端没寄存器可调，只能上面那套电感/分压硬件手段。
- 整体最优解：**如果 IO 电源域能切，3.3 V 改 1.8 V**——1.8 V IO 信号完整性显著优于 3.3 V，Rockchip 文档明确推荐 1.8 V。

---

## 引用

源 PDF（中文版，相对 `sdk/tspi-rk3566-sdk/docs/`）：

- `cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf` p.4-9（扫描流程、`rgmii-rxid`、示波器判据、FAQ）
- `cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_Mode_Configuration_CN.pdf` p.38-45（RK3568 GMAC1 RGMII 各种时钟拓扑下的完整 DTS 模板，RK3566 共用）

源码：`kernel-6.1/drivers/net/ethernet/stmicro/stmmac/dwmac-rk-tool.c`

蒸馏文件（本次新建）：`.claude/skills/rk/references/distilled/gmac-rgmii-delayline.md`
