---
title: D310T9362V1 3.1 寸 MIPI 屏移植 - 背景与方案选型
tags: [bsp, mipi-dsi, panel, touch, cst128a, tspi-rk3566]
desc: 立创泰山派 3.1 寸 MIPI 扩展板硬件背景、厂家方案与社区参考方案对比、移植路径决策

update: 2026-05-13
---


# D310T9362V1 3.1 寸 MIPI 屏移植 - 背景与方案选型

> [!note]
> **Ref:**
> - Ref-repo: [TSPI-D310T9362V1/Ref-repo](../Ref-repo/README.md) (Gitee `fengmoxi/tspi-d310t9362v1`)
> - 硬件开源：https://oshwhub.com/fengmoxi/tai-shan-pai-3-1-cun-ping-mu-kuo-zhan-ban
> - SDK 树：`sdk/tspi-rk3566-sdk/kernel-6.1` (devp 分支)


## 1. 硬件背景

立创**泰山派 3.1 寸 MIPI 扩展板**，型号 `D310T9362V1`，**通过 pogopin 接泰山派底部触点**实现 PD 供电 / 串口 (CH343) / RTC / 喇叭 / mic 功能。

| 项 | 参数 |
|---|---|
| 屏分辨率 | 480 × 800（**竖屏**） |
| MIPI 通道 | 2-lane D-PHY |
| Lane Rate | 480 Mbps |
| 像素时钟 | 30.816 MHz（按 `(480+50+60+10) × (800+20+34+2) × 60` 算） |
| 像素格式 | RGB888 |
| 背光 | PWM5（25 kHz 周期） |
| 触摸 IC | **CST128-A**（杭州海凌科 / Hynitron） |
| 触摸地址 | 0x38（FT5x16 兼容固件） / 0x48（CST 原版固件） |
| 触摸协议 | I²C @ 400 kHz on i2c1 |
| 接口排线 | 6-pin 0.5 mm + 31-pin 0.3 mm 同向 FFC/FPC |

## 2. 触摸 IC 地址决策

`Ref-repo/README.md` 关键说明：

> 淘宝厂家发货的触摸芯片是 **CST128-A**，资料是 FT5316……两种固件，一种 I²C 地址 0x48（CST 原版固件），另一种 **0x38**（FT5316 兼容寄存器协议）。

在板上探测：

```sh
root@taishanpi:~# i2cdetect -y 1
30: -- -- -- -- -- -- -- -- UU -- -- -- -- -- -- --   # 0x38 已被驱动占用
50: -- -- -- -- -- -- -- -- 58 -- -- -- -- -- -- --   # 0x58 未知设备 (待查)

root@taishanpi:~# cat /sys/bus/i2c/devices/1-0038/name
touch                                                  # 厂家 my,touch 的 device-name
```

→ **走 0x38 路径**，对接 Ref-repo `0x38/` 子目录的整套适配方案。

`0x58` 这颗 I²C IC 未在 DTS 声明，无驱动绑定，疑似板载 PD controller 或 audio amp（板载功能描述里有 PD 供电与喇叭）。对当前屏移植不阻塞，**待后查原理图**。

## 3. 起点状态（厂家 vs 参考方案对比）

`kernel-6.1/` 是**独立 git 仓库**（`devp` 分支），HEAD 已有"适配泰山派 1M"系列 commit，包含**厂家自己一套触摸方案**。与社区参考方案的差异：

| 维度 | 厂家 HEAD 方案 | Ref-repo `0x38/` 方案 |
|---|---|---|
| 触摸 compatible | `my,touch` | `hyn,cst128a` |
| 触摸驱动文件 | `drivers/input/touchscreen/my_touch/my_touch.c` | `cst128a_cust/cst128a_ts.c` |
| 协议层 | FT5x16 兼容寄存器（CST128-A 在 0x38 固件下与 FT5316 同接口） | hyn 原厂协议（含 IC reset 时序、ESD 监测等） |
| DSI lane-rate | 500 Mbps | 480 Mbps |
| Display route | `vp0_out_dsi1` | `vp1_out_dsi1` |
| DSI flags | 含 `MIPI_DSI_MODE_NO_EOT_PACKET`（**不发** EOT） | 含 `MIPI_DSI_MODE_EOT_PACKET`（**发** EOT，6.0+ 新极性宏） |
| Backlight levels | 0..255 线性 | 0..51 分段 + 256 末值 |
| Default brightness | 255 | 192 |
| 触摸 pinctrl | 缺 `touch_gpio` | 含 `touch_gpio` (gpio1 PA0, pull-up) |

### 原厂镜像为何"屏亮 + 触摸响应"但 DE 分辨率错？

- 触摸：HEAD 的 `my_touch.c` 走 FT5x16 兼容协议路径，**0x38 固件下能直驱**，所以触摸响应 OK。
- 显示：HEAD 已有 3.1 寸 panel 块（与社区方案大同小异），但**部分时序/路由参数与社区参考有差**，且 lane-rate 偏高 (500 vs 480 Mbps)，导致 panel 可亮但 DE 拿到的 mode 不稳定，渲染分辨率"对不上"。

## 4. 路径决策：为何选社区参考方案

| 维度 | 选社区 `cst128a_cust` | 留厂家 `my_touch` |
|---|---|---|
| 协议完整度 | hyn 原厂协议 (支持 IC reset 时序、固件 ID、扩展寄存器) | 仅 FT5x16 子集 |
| 上游兼容 | DT compatible `hyn,cst128a` 是 vendor 命名规范 | `my,touch` 无意义前缀 |
| 维护性 | 独立目录 + Kconfig 门控 | obj-y 强编、无 Kconfig |
| 显示时序 | 已和触摸方案打包验证（社区流片） | 与厂家配置耦合 |

**决策：完整切换到社区参考方案**，相关移植步骤见 [[01-integration]]。

## 5. 与 SDK 的对接

| 路径 | 角色 |
|---|---|
| `sdk/tspi-rk3566-sdk/kernel-6.1/` | Kernel 工作树，devp 分支 |
| `Ref-repo/0x38/cst128a_cust/` | 触摸驱动源（拷入 kernel `drivers/input/touchscreen/`） |
| `Ref-repo/0x38/tspi-rk3566-dsi-v10.dtsi` | 板级 dtsi 参考（用于校对，不能整体替换 — HEAD 已有大量厂家适配） |
| `Ref-repo/0x38/linux-kernel-D310T9362V1-0x38.patch` | 整套 patch（路径基于旧 SDK，不直接套用） |

注意 Ref-repo 的 dtsi 与 kernel 的 dtsi **不能直接替换**：HEAD 已有 10 寸 panel 块、厂家自写的 `my,touch` 节点、cms3568 系列其他扩展节点等。**正确做法是按字段精细对齐**，详见 [[01-integration]]。
