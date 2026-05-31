---
title: Eval-2 trace（without_skill 分支）
tags: [eval-trace, rk3566, gmac]
desc: 回答 RK3566 GMAC1 RGMII delayline 问题时的探源轨迹
update: 2026-05-27
---


# Trace —— eval-2-gmac-rgmii-delayline / without_skill

## 工作前提
- 本分支显式 **不**调用 `rk` skill（branch 名 `without_skill`）。
- 仅靠 SDK 内 source-of-truth + 公开 RGMII / dwmac 知识作答。
- 也未走 WebFetch / WebSearch（脚手架虽提供，但本地源码足够）。

## 探源清单（按时间顺序）

1. `ls /home/pi/imx/sdk/tspi-rk3566-sdk/` —— 确认存在 `kernel-6.1/` 内核源码树。
2. `find .../Documentation/devicetree/bindings/net -name 'rockchip*' -o -name 'stmmac*'` —— 定位绑定文档。
3. **Read** `/home/pi/imx/sdk/tspi-rk3566-sdk/kernel-6.1/Documentation/devicetree/bindings/net/rockchip-dwmac.yaml`
   - 确认 `tx_delay` 默认 `0x30`，`rx_delay` 默认 `0x10`，范围 `0~0x7F`。
   - 确认 `clock_in_out` 含义（input=PHY 给时钟 / output=SoC 给时钟）。
4. `grep "&gmac1" rk3568-evb1-ddr4-v10.dtsi` —— 拿到官方 RK3568 EVB1 GMAC1 完整范例（`tx=0x4f, rx=0x26`，pinctrl 用 `gmac1m1_*` 组）。
5. `grep "tx_delay" rk3568*.dts*` / `grep "rx_delay" rk356*.dts*` —— 横向收集 9 块官方板子的实测 delay 值，做"起步值挑选表"。
6. **Read** `/home/pi/imx/sdk/tspi-rk3566-sdk/kernel-6.1/drivers/net/ethernet/stmicro/stmmac/dwmac-rk.c` 关键区段
   - 1842~1862 行：`RK3568_GRF_GMAC[01]_CON0/CON1` 寄存器布局；
   - `RK3568_GMAC_CLK_RX_DL_CFG = HIWORD_UPDATE(val, 0x7F, 8)` 与 `..._TX_DL_CFG = HIWORD_UPDATE(val, 0x7F, 0)` —— 证实 7 bit 字段；
   - `RK3568_GMAC_RXCLK_DLY_ENABLE / TXCLK_DLY_ENABLE` —— 证实"写了 delay 才会自动 enable"。
7. **Read** `/home/pi/imx/sdk/tspi-rk3566-sdk/kernel-6.1/drivers/net/ethernet/stmicro/stmmac/dwmac-rk-tool.c`
   - 行 103 `MAX_DELAYLINE 0x7f`、行 1408 `rgmii_delayline_show`、行 1422 `rgmii_delayline_store`、行 1459 `DEVICE_ATTR_RW(rgmii_delayline)`、行 1534 `phy_lb_scan_store` —— 确认 sysfs 节点 `rgmii_delayline / phy_lb_scan / mac_lb` 的存在与用法。
   - 行 903 `dwmac_rk_delayline_scan` —— 确认扫描算法是"先 tx 取中点，再 rx 取中点"。
8. `grep "gmac1m[01]_(miim|rgmii)" rk3568-pinctrl.dtsi` —— 确认 pinctrl 组命名（m0/m1 两组复用）。

## 没有用到（但脚手架可用）
- `WebFetch` / `WebSearch`：本地源码已经够，避免引入未经验证的外部说法。
- `rk` skill：分支约束。
- 任何示波器实测：仅 SoC 文档级表述，未在板上实测；已在 answer 中标注"经验值"避免误导。

## 写作要点
- DTS 范本不是凭空写的，直接引自 `rk3568-evb1-ddr4-v10.dtsi` 第 231~256 行并改裁。
- delay 步进 `~50 ps/step` 在 SDK 文档里未硬写，标注为"工程经验值"。
- 强调"PHY 内部 delay + MAC 内部 delay 冲突"是第一名踩坑（用户问题里 ping 大包丢，phy-mode/PHY delay mismatch 的概率最高）。
- 验证流程 = sysfs `phy_lb_scan` → iperf3/ethtool → 示波器；按"成本 / 命中率"递增排序。
