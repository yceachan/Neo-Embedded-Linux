---
title: RK3566 GMAC1 RGMII tx_delay / rx_delay 调参实战
tags: [RK3566, RK3568, GMAC, RGMII, dwmac-rk, delayline, ethernet]
desc: RK3566 GMAC1 接千兆 RGMII PHY 丢包问题排查 —— DTS delayline 配置范例与示波器 / 软件双重验收方法
update: 2026-05-27
---


# RK3566 GMAC1 RGMII tx_delay / rx_delay 调参实战

> [!note]
> **Ref**
> - `kernel-6.1/Documentation/devicetree/bindings/net/rockchip-dwmac.yaml`（值域 / 默认值定义）
> - `kernel-6.1/drivers/net/ethernet/stmicro/stmmac/dwmac-rk.c`（`RK3568_GMAC_CLK_RX/TX_DL_CFG`, `DELAY_ENABLE/VALUE` 宏）
> - `kernel-6.1/drivers/net/ethernet/stmicro/stmmac/dwmac-rk-tool.c`（`rgmii_delayline` / `phy_lb_scan` sysfs 扫描工具）
> - `kernel-6.1/arch/arm64/boot/dts/rockchip/rk3568-evb1-ddr4-v10.dtsi`（官方 RK3568 EVB1 GMAC1 + RTL8211F 范本 `tx=0x4f, rx=0x26`）

---

## 1. 先把"丢包根因 = 时序"这件事说清楚

RGMII 是 DDR 接口：125 MHz 的 TXC/RXC 在上下沿都采样 4 bit 数据。
**1000Base-T full duplex 下，数据有效窗口只剩 ~2 ns**。任何"时钟相对数据偏移"的不匹配都会让接收端在数据跳变点上采样 → CRC 错 → 大包先丢、小包侥幸过。典型现象正是：
- `ping -s 64` 全通；
- `ping -s 1472` 或 `iperf` 大量丢包 / 速率上不去；
- `ip -s link show eth0` 里 `RX errors`、`crc` 计数随流量增长。

RGMII 让时钟相对数据延迟 **~2 ns** 的方式有三种：
1. PCB 走线让 TXC 比 TXD 多走 ~25 cm（"long trace"）；
2. PHY 内部开 RGMII-ID（推荐，软件最省心）；
3. **MAC 侧给 TXC / RXC 加 delayline**（RK SoC 内置，靠 `tx_delay` / `rx_delay` 配）。

RK3566/RK3568 走第 3 种，**MAC 侧的 delayline 等效于把时钟相对数据往后挪**。调参的最终目标，是让 PHY 收数据时钟沿落在数据眼图正中。

---

## 2. RK3566 delayline 的硬件本质

直接看驱动 `dwmac-rk.c`（RK3566 共用 rk3568 一套 GRF 寄存器）：

```c
/* RK3568_GRF_GMAC1_CON0 */
#define RK3568_GMAC_CLK_RX_DL_CFG(val)  HIWORD_UPDATE(val, 0x7F, 8)
#define RK3568_GMAC_CLK_TX_DL_CFG(val)  HIWORD_UPDATE(val, 0x7F, 0)

/* RK3568_GRF_GMAC1_CON1 */
#define RK3568_GMAC_RXCLK_DLY_ENABLE    GRF_BIT(1)
#define RK3568_GMAC_TXCLK_DLY_ENABLE    GRF_BIT(0)
```

关键事实：
- **每路 delayline 7 bit，范围 `0x00 ~ 0x7F`（127 档）**。
- 一档步进官方未在 DTS bindings 中写死，工程经验值约 **~50 ps/step**，全量程覆盖 ~6 ns，足以校齐 1000M RGMII 的 2 ns 偏移。
- DTS 里 `tx_delay` / `rx_delay` 一旦写了，驱动会自动 **ENABLE 对应延迟链路**；不写则使用 binding 默认值 `tx=0x30, rx=0x10`（多数板子不可用）。
- 默认值是 RK3288 时代的遗留，**RK3566/RK3568 几乎所有官方板都不用默认值**。

---

## 3. DTS 配置示例（RK3566，GMAC1 + 1G RGMII PHY，RTL8211F 类）

下面这份是按 RK3568-EVB1 范例（实测 tx=0x4f, rx=0x26）裁出来给 RK3566 用，去掉 PCIe 干扰、只留 GMAC1。

```dts
&gmac1 {
    phy-mode    = "rgmii";
    clock_in_out = "output";   /* 125M 由 SoC PLL 产生输出给 PHY */

    /* PHY 复位：低有效；rtl8211f 至少 20 ms low + 100 ms 等待 */
    snps,reset-gpio       = <&gpio2 RK_PD1 GPIO_ACTIVE_LOW>;
    snps,reset-active-low;
    snps,reset-delays-us  = <0 20000 100000>;

    /* RGMII 125 MHz 时钟源切换 */
    assigned-clocks       = <&cru SCLK_GMAC1_RX_TX>, <&cru SCLK_GMAC1>;
    assigned-clock-parents= <&cru SCLK_GMAC1_RGMII_SPEED>, <&cru CLK_MAC1_2TOP>;
    assigned-clock-rates  = <0>, <125000000>;

    pinctrl-names = "default";
    pinctrl-0 = <&gmac1m1_miim          /* 按你板子用 m0 还是 m1 选 */
                 &gmac1m1_tx_bus2
                 &gmac1m1_rx_bus2
                 &gmac1m1_rgmii_clk
                 &gmac1m1_rgmii_bus>;

    /* === 关键：delayline === */
    tx_delay = <0x4f>;   /* 起步建议值，按下面"扫描"再细调 */
    rx_delay = <0x26>;

    phy-handle = <&rgmii_phy1>;
    status     = "okay";
};

&mdio1 {
    rgmii_phy1: phy@0 {
        compatible = "ethernet-phy-ieee802.3-c22";
        reg = <0x0>;
        /* 如 PHY 自身已开 RX/TX delay (RGMII-ID)，MAC 侧 delay 要相应减小 */
    };
};
```

### 起步值挑选表（RK 官方板子的实测分布）

| 板卡 / SoC | tx_delay | rx_delay | 备注 |
|---|---|---|---|
| RK3568-EVB1 (GMAC1, RTL8211F) | `0x4f` | `0x26` | 官方主参考 |
| RK3568-EVB5 / EVB6 | `0x46` | `0x2f` | DDR 类型不同 |
| RK3566-EVB2-LP4x | `0x45` | `0x25` | 来自官方 dtsi |
| RK3566 Box-Demo | `0x42` | `0x2d` | |
| RK3566 Quartz64-B | `0x3c` | `0x24` | 第三方 |

**经验**：先抄一份最像你板子的，然后用下面工具围绕这个点扫一圈。

### 与 PHY 内部 delay 的关系（避免最常见的踩坑）

| PHY 模式 (`phy-mode`) | PHY 内部 TX delay | PHY 内部 RX delay | MAC 侧应配 |
|---|---|---|---|
| `rgmii`     | off | off | **MAC 双侧 delay 都要大**（典型 tx≈0x4f, rx≈0x26） |
| `rgmii-id`  | on  | on  | MAC 侧 delay 应小或 0（重复了就过冲，反而丢包） |
| `rgmii-txid`| on  | off | MAC 侧只配 rx_delay |
| `rgmii-rxid`| off | on  | MAC 侧只配 tx_delay |

> RTL8211F 默认 RX delay = on, TX delay = off。**如果你 DTS 写的是 `phy-mode = "rgmii"`，你实际是在做 rgmii-rxid，再给 MAC 一个大 rx_delay 就过补偿了**。这是丢包 case 第一名。建议要么改成 `rgmii-id` 并关掉 PHY 内部 delay 的相应行为，要么严格按 `rgmii` 模式确保 PHY 两路 delay 都关。

---

## 4. 软件验证 —— 三件套，不用拆板子

### 4.1 dwmac-rk 自带 sysfs 在线扫描（首选）

Kernel `CONFIG_DWMAC_RK_TOOL=y` 后，`/sys/class/net/eth0/device/` 下会有：

```
rgmii_delayline    # cat 读当前值；echo "0x4f 0x26" > 设置
phy_lb_scan        # 触发自动扫描
mac_lb             # MAC 内部环回
```

最关键的两步：

```sh
# 1) 实时改 delay 不用重启不用重编
echo 0x4f 0x26 > /sys/class/net/eth0/device/rgmii_delayline
cat /sys/class/net/eth0/device/rgmii_delayline
# tx delayline: 0x4f, rx delayline: 0x26

# 2) 触发 PHY 环回扫描（驱动在 0x00~0x7F 范围步进打 UDP/TCP 包，
#    统计每个 (tx,rx) 组合的丢包/CRC，找出最大连续 valid 窗口的中点）
echo 1 > /sys/class/net/eth0/device/phy_lb_scan
dmesg | tail -50    # 看输出表格，挑窗口正中那对 (tx, rx) 写回 DTS
```

`dwmac-rk-tool.c` 的扫描逻辑就是：先扫 tx，找到 tx 合法区间取中点 → 再以该 tx 扫 rx，取中点。**最终选的就是数据眼图正中央，是最稳的方式**。

### 4.2 真实流量回归验证

```sh
# A) 大包 ping，1472B（=1500-IP-ICMP），count 1000 容忍 0 丢
ping -s 1472 -c 1000 -i 0.01 <peer>

# B) iperf3 双向打满 940M（千兆理论值）
iperf3 -c <peer> -t 60 -P 4
iperf3 -c <peer> -t 60 -R

# C) 看错误计数是否在涨（这是真·硬指标）
ethtool -S eth0 | grep -E "crc|errors|rx_pkt_n|tx_pkt_n"
ip -s link show eth0
```

判收准则（缺一不可）：
- `iperf3` 双向稳定 ≥ 900 Mbps；
- 60s 期间 `rx_crc_errors` / `rx_length_errors` **零增长**；
- 1000 个 1472B 包 0 丢；
- `ethtool eth0` 显示 `Speed: 1000Mb/s Duplex: Full Link: yes`。

### 4.3 启动期排错的两句话

```sh
ethtool eth0                             # 协商速率确认到千兆
ethtool --register-dump phy eth0 | head  # PHY 状态
dmesg | grep -iE "dwmac|gmac|phy|rgmii"  # 看 link 协商、delay enable 日志
```

---

## 5. 示波器验证 —— 只在最后定型 / 量产挑机时用

软件已经能定位 99% 问题，**示波器主要是给硬件团队做 PCB / 元件挑选回归**。如果手头有 ≥500 MHz 带宽示波器、差分探头或 50Ω 单端探头：

### 5.1 探测点

| 信号 | 探测位置 | 用途 |
|---|---|---|
| **TXC**  | 紧贴 PHY 的 RGMII TXC 球脚（或 0Ω 串阻 PHY 侧） | 参考时钟 |
| **TXD0** | 同上，TXD0/1/2/3 取一根 | 测时钟-数据相位 |
| **RXC**  | 紧贴 SoC 球脚（或 0Ω 串阻 SoC 侧） | 反向同理 |
| **RXD0** | 同上 | |

注意：**地脚一定用弹簧针就近接，不要长鳄鱼夹**，否则 125M DDR 信号根本看不准。

### 5.2 看什么

1. **时钟-数据相位**（最重要）：双通道触发 TXC 上升沿，看 TXD 在 TXC 上升/下降沿前后的 setup / hold。
   - RGMII 规范：setup ≥ 1.0 ns，hold ≥ 1.0 ns（含 ±500 ps 偏移容差）。
   - 调 `tx_delay`：调大 → TXC 相对 TXD 后移 → TXD 在 TXC 边沿前更稳，setup 变大、hold 变小。
   - **目标：setup 和 hold 大致相等（≈1.5 ns 各）**，意味着时钟沿落在数据眼图正中。

2. **眼图**：示波器 persist 模式打开几秒钟，叠加几万次跳变。数据眼"睁开"高度 ≥ 800 mV、宽度 ≥ 2 ns 才算合格。

3. **过冲 / 振铃**：上升沿过冲 > 30% 说明端接不对（PCB 问题），靠 delay 救不了。

### 5.3 调参的物理对照

```
增大 tx_delay  → MAC 侧 TXC 输出再延后  → PHY 看 TXD 的 setup ↑, hold ↓
增大 rx_delay  → MAC 侧把 RXC 进一步延后采样 RXD → 看 RXD 的 setup ↓, hold ↑
```

所以现象判断很直接：
- **TX 方向丢包**（peer 收 CRC 错）：示波器测 PHY 侧 TXD 相对 TXC 的 setup / hold，哪边小就反向调 `tx_delay`。
- **RX 方向丢包**（本机 `rx_crc_errors` 涨）：调 `rx_delay`。

---

## 6. 一句话总结调试流程

1. 抄 RK3568-EVB1 的 `tx=0x4f, rx=0x26` 写进 DTS、确认 `phy-mode` 和 PHY 内部 delay 不打架；
2. 启动后 `echo 1 > phy_lb_scan` 让驱动自动找最佳点，写回 DTS；
3. `iperf3 -P 4 -t 60` + `ethtool -S` 看 60s 零 CRC 才算过；
4. 若仍丢包且 PCB 走线异常，再上示波器看 TXC/TXD 相位 + 眼图定位 PCB / 端接问题。

99% 的 RGMII 大包丢包，止步于第 2 步。
