---
title: RK356x GMAC RGMII delayline 调试
tags: [rk, distilled, gmac, rgmii, rk3566, rk3568]
desc: RK3566/RK3568 GMAC RGMII tx_delay/rx_delay 取值流程、DTS 片段与示波器/iperf 验证方法
source:
  - cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf
  - cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_Mode_Configuration_CN.pdf
source_pages: "delayline:1-10; mode-config:38-45"
update: 2026-05-27
---


# RK356x GMAC RGMII delayline 调试

> [!note]
> **Ref:**
> - PDF: `sdk/tspi-rk3566-sdk/docs/cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf` p.1-10
> - PDF: `sdk/tspi-rk3566-sdk/docs/cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_Mode_Configuration_CN.pdf` p.38-45 (RK3568 RGMII，RK3566 共用)
> - 源码: `kernel-6.1/drivers/net/ethernet/stmicro/stmmac/dwmac-rk-tool.c`

## 背景

RGMII 千兆模式下 TX/RX 数据相对时钟需要 ~2 ns 的建立/保持窗口（规范 1~2.6 ns）。PCB 走线长度、阻抗、PHY 选型差异都会让窗口偏移，造成千兆下大包丢、ping -s 1472 不通、iperf 跑不满 900 M。Rockchip 在 GMAC 控制器侧提供 7-bit 可调的 `tx_delay` / `rx_delay`（取值范围 `0x00~0x7f`），软件不改板就能补偿；步进约 ~150 ps。调一次只对当前硬件有效，**换板/换 PHY 必须重扫**。

PHY 自带 RX delay（如 RTL8211F）时，应当走 `phy-mode = "rgmii-rxid"`，SoC 端只扫 TX、关闭 RX delay。

## 关键配置 / API

### sysfs 调试节点（依赖 `dwmac-rk-tool`）

Kernel-4.19+ 默认带；4.4 / 3.10 需要打 Rockchip 内部补丁 `Rockchip_RGMII_Delayline_Kernel*.tar.gz`。生成节点示例（RK3399；RK356x 对应 `/sys/devices/platform/fe2a0000.ethernet/` 一类，按板看）：

```bash
# 1. 扫描出 pass 窗口（横轴 TX，纵轴 RX，每个 'O' 表示该坐标 pass）
echo 1000 > /sys/devices/platform/fe2a0000.ethernet/phy_lb_scan
# 命令末尾会自动打印窗口中心坐标，比如 "center tx=0x2e rx=0x0f"

# 2. 把中心值写回去测一次 loopback
echo 0x2e 0x0f > /sys/devices/platform/fe2a0000.ethernet/rgmii_delayline
echo 1000 > /sys/devices/platform/fe2a0000.ethernet/phy_lb     # 必须先 pass

# 3. pass 后写进 DTS，烧机后跑 ping -s 1472/iperf 复测
```

### DTS（RK3568/RK3566 GMAC1 RGMII，clock_in_out=output；GMAC1 仅在 RK3568 存在，RK3566 只有 GMAC1，结构相同）

下面取自 mode-config PDF p.38-39 中 `gmac1m1` 引脚组（RK3566/68 板常用 M1）：

```dts
&gmac1 {
    phy-mode = "rgmii";                /* PHY 自带 RX delay 则用 "rgmii-rxid" */
    clock_in_out = "output";

    snps,reset-gpio = <&gpio2 RK_PD1 GPIO_ACTIVE_LOW>;
    snps,reset-active-low;
    snps,reset-delays-us = <0 20000 100000>;  /* RTL8211F 复位需 100 ms */

    assigned-clocks = <&cru SCLK_GMAC1_RX_TX>, <&cru SCLK_GMAC1>, <&cru CLK_MAC1_OUT>;
    assigned-clock-parents = <&cru SCLK_GMAC1_RGMII_SPEED>;
    assigned-clock-rates = <0>, <125000000>, <25000000>;

    pinctrl-names = "default";
    pinctrl-0 = <&gmac1m1_miim
                 &gmac1m1_tx_bus2
                 &gmac1m1_rx_bus2
                 &gmac1m1_rgmii_clk
                 &gmac1m1_rgmii_bus
                 &eth1m1_pins>;

    /* 实测扫出窗口中心后填这两个值；下面是 RK3568 EVB 参考值 */
    tx_delay = <0x4f>;
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

PHY 是 RTL8211F（启用了内部 RX delay）时改两处：

```dts
&gmac1 {
    phy-mode = "rgmii-rxid";   /* 关 SoC 内部 RX delay */
    tx_delay = <0x43>;         /* 仍需扫描，PHY 固定 RX≈2 ns */
    /* rx_delay = <0x26>; */   /* 注释掉 */
};
```

### Kconfig 自动扫描（窗口很小时再开）

```
Device Drivers →
  Network device support →
    Ethernet driver support →
      [*] Auto search rgmii delayline   (CONFIG_DWMAC_RK_AUTO_DELAYLINE)
```

首次开机扫一次，结果存 vendor storage，后续覆盖 DTS。**vendor storage 不擦不会重扫**。

## 易踩坑

- **RTL8211E PHY 实测时必须拔网线**，否则 loopback 不准；Rockchip 直接建议改用 RTL8211F 或其他 PHY（delayline PDF p.4/9）。
- DTS 跟 sysfs 数值要一致；改 DTS 后忘了烧固件，下次复测还是旧窗口。
- PHY 端启用了内部 delay（RTL8211F 默认），SoC 端仍用 `phy-mode = "rgmii"` 会双重补偿——必须 `rgmii-rxid`，且注释 `rx_delay`（delayline PDF p.7）。
- 百兆扫窗口几乎全 pass，**必须用 `echo 1000` 千兆扫**；百兆通不代表千兆通。
- 窗口扫出来很小（<5 格），多半是硬件信号不行，开 `CONFIG_DWMAC_RK_AUTO_DELAYLINE` 也救不了，要回去查 RGMII_CLK 占空比/幅度（delayline PDF p.8-9）。
- IO 电源域：3.3 V IO 的信号完整性弱于 1.8 V，PHY 与 RK 都支持 1.8 V 时优先用 1.8 V（PDF p.8）。

## 验证方法

### 软件层（先做）

```bash
# 1) 链路确认
ethtool eth0 | grep -E "Speed|Duplex"      # 应 Speed: 1000Mb/s, Duplex: Full

# 2) 大包丢包（不分片）
ping -M do -s 1472 -c 200 <peer>           # 0% loss 才算窗口稳

# 3) 吞吐
iperf3 -c <peer> -t 30                     # TCP 通常 ≥ 940 Mbps
iperf3 -c <peer> -t 30 -R                  # 反向也要跑，TX/RX 两个方向各看一边

# 4) MAC 误码统计
ethtool -S eth0 | grep -Ei "error|crc|drop|fifo"
# rx_crc_errors / rx_fifo_errors 持续涨 ⇒ 当前 rx_delay 不在窗口
# tx_underrun / tx_carrier_errors 涨   ⇒ tx_delay 不对或 PHY 端时序问题
```

判定标准：千兆下 `ping -s 1472` 0% 丢包，iperf3 双向 ≥ 900 Mbps，`ethtool -S` 错误计数静止——窗口算调对了。

### 示波器层（窗口小/软件层不过时再做）

> delayline PDF p.8-9 原文要求。

- 探头/示波器带宽 ≥ 125 MHz × 5 = **≥ 625 MHz**；优先差分探头；单端探头地线务必短。
- 测点：靠近 **PHY 端**（接收端）的 `MAC_CLK`、`TX_CLK`、`RX_CLK`，**不要在发送端测**（反射严重）。
- 判据：波形是方波而非正弦；**占空比 45%~55%**。
- 千兆下额外测 TXC 与 TXD（任一根）的相位差，目标 **1.5 ns ~ 2 ns**（规范 1~2.6 ns 留余量）。相位差不对就用 `echo` 改 `rgmii_delayline` 试，每次 step 5 由小到大扫，找到 iperf 能跑过 900 M 的区段后再以 step 2 细调，取中位写回 DTS（PDF p.9）。
- 信号边沿过缓：调 IO 驱动强度到最大；仍不行可在发送端串 22~100 Ω / 高频电感整形，或把 IO 电源从 3.3 V 改 1.8 V。

## 相关

- [[chip-matrix]] —— RK3566 / RK3568 GMAC 路数与 RGMII 引脚组 M0/M1
- 源 PDF 的 `dwmac-rk-tool.c` 实现细节（如要写自定义扫描脚本时再展开）
