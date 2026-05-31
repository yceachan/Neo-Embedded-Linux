---
title: RK3566 M.2 B-Key 5G CPE 模组接入(USB3 / PCIe)与 CPE 路由方案
tags: [rk, distilled, m2, 5g, modem, cpe, usb3, pcie, rk3566]
desc: M.2 B-Key 5G 模组在 RK3566 上的总线选型、DTS 模板、内核 driver 栈、用户态拨号与 GMAC 下行路由
source:
  - cn/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf
  - cn/Common/PCIe/Rockchip_Developer_Guide_PCIe_CN.pdf
  - cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_CN.pdf
source_pages: USB p.4,14-21,31; PCIe p.10,15,24-30
update: 2026-05-27
---


# RK3566 M.2 B-Key 5G CPE 模组接入与 CPE 路由方案

> [!note]
> **Ref:**
> - USB: `cn/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf` p.4(USB 控制器总览)、p.14-21(USB3 OTG/Host 硬件+DTSI)、p.31(VBUS 属性)
> - PCIe: `cn/Common/PCIe/Rockchip_Developer_Guide_PCIe_CN.pdf` p.10(RK3566 资源)、p.15(RK3566 DTS 入口)、p.24-30(comboPHY+Wi-Fi 范例 — 5G 模组照搬)


## 一、背景:M.2 B-Key 上的 5G 模组到底走什么总线

M.2 B-Key 物理槽位同时引出 USB 3.0 + USB 2.0 + PCIe ×1(×2 也允许但模组多数只用 ×1) + SIM + UART + I2C 等。具体走 USB 还是 PCIe **由模组决定**,主机端不能切。常见模组:

| 模组 | 推荐总线 | 备注 |
|------|----------|------|
| Quectel RM500Q / RM502Q | USB3 默认,PCIe 可选 | USB3 下行 ~1.5-2Gbps 实测 |
| Quectel RM520N-GL | **PCIe(MHI)首选** | USB3 下行不超过 2.5Gbps;PCIe 可到 5Gbps |
| Fibocom FM150-AE | USB3 | 走 NCM/MBIM |
| 高通 SDX55/SDX65 系列模组(PCIe 模式) | **PCIe MHI** | 走 `mhi-pci-generic` + `mhi_net` |

**用户问的"2.5G/千兆"**:5G NR Sub-6GHz 下行 DL CAT-19 理论 ~2.5 Gbps、典型 1-1.5 Gbps。这是 **WAN 上行**的速率,**不是 LAN 侧**。RK3566 自身只有 **1×千兆 GMAC**,下行 LAN 最多千兆。若一定要 2.5G LAN,需再接 RTL8125 走 PCIe,但 RK3566 PCIe 已被模组占满 — **资源冲突,RK3566 上做不到 2.5G LAN**。


## 二、RK3566 总线资源约束(规划必看)

USB 侧(参考 USB PDF p.4):

| 控制器 | 速度 | PHY | 复用对象 |
|--------|------|-----|----------|
| `usbdrd30` OTG | **仅 USB2 480Mbps**(RK3566 阉割了 OTG3) | usb2phy0 + combphy0_us(理论上,RK3566 不出 USB3 OTG) | — |
| `usbhost30` USB3 Host | **USB3.0 5Gbps** | usb2phy0(host port) + combphy1_usq | combphy1 ↔ SATA1/QSGMII 互斥 |
| `usb_host0_ehci/ohci` | USB2 | usb2phy1 port0 | — |
| `usb_host1_ehci/ohci` | USB2 | usb2phy1 port1 | — |

PCIe 侧(参考 PCIe PDF p.10):

| 控制器 | 速度 | PHY | 复用对象 |
|--------|------|-----|----------|
| `pcie2x1@fe260000` | Gen2 ×1 = 5GT/s | `combphy2_psq` | combphy2 ↔ SATA0/RGMII1(GMAC1 RGMII) 互斥 |

**结论:**

- 走 **USB3**:占 `usbhost30` + `combphy1_usq`。必须 disable `&sata1` 与 QSGMII。GMAC1 不受影响。
- 走 **PCIe**:占 `pcie2x1` + `combphy2_psq`。必须 disable `&sata0` 与 GMAC1 的 RGMII 模式(只能改用 RGMII1 之外的);若要 GMAC1 + PCIe 同时使能,GMAC1 必须切到 RMII 或换用 GMAC0(RK3566 仅 1 个 GMAC,所以这里需仔细查板级原理图,有的板子是把 RGMII1 复用让出 combphy2)。


## 三、内核驱动栈

按"模组实际接什么总线"分两条线。

### 3.1 走 USB3(模组多数默认)

```
xhci_hcd  (USB3 主控)
  └─ usb-net 协议层
       ├─ option / cdc_acm     (/dev/ttyUSB* /dev/ttyACM*  →  AT/QMI 控制端口)
       ├─ qmi_wwan             (Quectel QMI 数据通道 wwan0)
       ├─ cdc_mbim             (MBIM 数据通道 wwan0)
       ├─ cdc_ncm              (NCM 数据通道 usb0)
       └─ cdc_ether / rndis    (legacy fallback)
```

**Kconfig 必开**(基于 kernel-6.1):

```
CONFIG_USB_XHCI_HCD=y          USB3 host
CONFIG_USB_DWC3=y              RK 用的 dwc3 控制器
CONFIG_USB_SERIAL_OPTION=m     模组 AT 串口
CONFIG_USB_NET_QMI_WWAN=m      QMI 数据
CONFIG_USB_NET_CDC_MBIM=m      MBIM 数据
CONFIG_USB_NET_CDC_NCM=m       NCM 数据
CONFIG_USB_USBNET=m            cdc 通用框架
```

### 3.2 走 PCIe(MHI,2.5G+ 必备)

```
rockchip-pcie  (RC 驱动)
  └─ mhi-pci-generic  (高通 Modem Host Interface 通用驱动)
       └─ mhi_net    (网络接口 mhi_hwip0)
       └─ mhi_uci    (用户态控制通道 /dev/mhi_DIAG /dev/mhi_QMI ...)
```

**Kconfig 必开:**

```
CONFIG_PCIE_ROCKCHIP_HOST=y    RK PCIe RC
CONFIG_PCIE_DW=y               DesignWare PCIe(RK3566 用 DW core)
CONFIG_MHI_BUS=m               MHI 总线核心
CONFIG_MHI_BUS_PCI_GENERIC=m   通用 MHI PCI 驱动(覆盖 RM520N-GL / SDX55/65)
CONFIG_MHI_NET=m               MHI 网卡
```

`mhi-pci-generic` 自带 SDX55/SDX65 的 PID/VID 表,Quectel RM520N-GL 在 mainline 已收。老内核需 cherry-pick 设备表 patch。


## 四、DTS 配置模板

### 4.1 USB3 路径(Wi-Fi 范例换皮)

5G 模组在 M.2 上没有 PERST,但常常有 PWR_EN(`W_DISABLE` 信号上电序)和 RESET。需要在 DTS 里建一个 regulator 控 VBUS 上电,并把模组 reset/disable 引脚作为 GPIO 控制。

```dts
/ {
    vcc3v3_m2_5g: vcc3v3-m2-5g {
        compatible = "regulator-fixed";
        regulator-name = "vcc3v3_m2_5g";
        regulator-min-microvolt = <3300000>;
        regulator-max-microvolt = <3300000>;
        enable-active-high;
        gpios = <&gpio0 RK_PCx GPIO_ACTIVE_HIGH>;   /* PWR_EN_3V3 */
        startup-delay-us = <50000>;                  /* 模组上电至少 50ms */
        vin-supply = <&vcc5v0_sys>;
    };

    /* W_DISABLE1#(无线禁用)默认拉高让模组工作 */
    m2_w_disable: m2-w-disable {
        compatible = "regulator-fixed";
        regulator-name = "m2_w_disable";
        enable-active-high;
        regulator-always-on;
        gpios = <&gpio0 RK_PDy GPIO_ACTIVE_HIGH>;
    };
};

&combphy1_usq {
    status = "okay";   /* USB3 Host 用 */
};

&sata1 { status = "disabled"; };   /* 让出 combphy1 */

&usb2phy0 {
    status = "okay";
};
&u2phy0_host {
    phy-supply = <&vcc3v3_m2_5g>;
    status = "okay";
};
&usbhost30 { status = "okay"; };
&usbhost_dwc3 {
    dr_mode = "host";
    status = "okay";
};
```

### 4.2 PCIe 路径(MHI)

```dts
/ {
    vcc3v3_pcie_m2: vcc3v3-pcie-m2 {
        compatible = "regulator-fixed";
        regulator-name = "vcc3v3_pcie_m2";
        regulator-min-microvolt = <3300000>;
        regulator-max-microvolt = <3300000>;
        enable-active-high;
        gpios = <&gpio0 RK_PCx GPIO_ACTIVE_HIGH>;
        startup-delay-us = <50000>;
        vin-supply = <&vcc5v0_sys>;
    };
};

&combphy2_psq {
    status = "okay";
};

&sata0 { status = "disabled"; };           /* 让出 combphy2 */
/* GMAC1 若使用 RGMII1,需评估是否冲突;RK3566 通常 GMAC1 用 RMII,不冲突  */

&pcie2x1 {
    reset-gpios = <&gpio3 RK_PCx GPIO_ACTIVE_HIGH>;    /* M.2 PERST# */
    vpcie3v3-supply = <&vcc3v3_pcie_m2>;
    rockchip,perst-inactive-ms = <200>;     /* 5G 模组 PERST 复位至少 200ms,具体看模组手册  */
    status = "okay";
};
```

参考 PCIe PDF p.28-30 的 Wi-Fi 模块范例,5G 模组 DTS 几乎一致,只是把 `wireless_wlan` 节点换成模组的 reset/W_DISABLE GPIO 控制。


## 五、用户态拨号流程

无论 USB 还是 PCIe,只要 net 接口起来(`ip link` 可见 `wwan0` 或 `mhi_hwip0`),用户态步骤通用。

### 5.1 推荐方案:ModemManager + NetworkManager

```bash
# 1. 启动 ModemManager
systemctl start ModemManager

# 2. 看模组识别情况
mmcli -L                                    # 列出所有 modem,得到 /Modem/0
mmcli -m 0                                  # 看 modem 状态

# 3. SIM PIN(如有)
mmcli -m 0 --enable
mmcli -i 0 --pin=1234

# 4. 创建并启用 PDN
mmcli -m 0 --simple-connect="apn=cmnet,ip-type=ipv4v6"

# 5. 让 NetworkManager 接管
nmcli c                                     # 应看到一条 GSM/MBIM 连接
nmcli c up <connection-name>
```

### 5.2 Quectel 厂商工具(QMI)

模组若用 Quectel_QMI,可用厂商提供的 `quectel-CM`:

```bash
quectel-CM -s cmnet -4                      # IPv4 拨号,自动起 wwan0
# 自动 udhcpc 拿 IP,默认走 raw_ip 模式
```

### 5.3 MHI(PCIe)拨号

`mhi_net` 暴露的接口名是 `mhi_hwip0`(无线)/`mhi_swip0`(软件)。用 `libmbim-utils`:

```bash
mbimcli -d /dev/mhi_MBIM --connect="apn=cmnet"
ip link set mhi_hwip0 up
udhcpc -i mhi_hwip0
```


## 六、CPE 下行路由(把 5G 网络喂给 LAN)

RK3566 GMAC1(千兆)作为下行 LAN 口。

```bash
# 1. 启用 IPv4 转发
echo 1 > /proc/sys/net/ipv4/ip_forward

# 2. NAT(WAN = wwan0 or mhi_hwip0,LAN = eth0)
iptables -t nat -A POSTROUTING -o wwan0 -j MASQUERADE
iptables -A FORWARD -i eth0 -o wwan0 -j ACCEPT
iptables -A FORWARD -i wwan0 -o eth0 -m state --state RELATED,ESTABLISHED -j ACCEPT

# 3. 给 eth0 配静态 IP + dnsmasq DHCP
ip addr add 192.168.50.1/24 dev eth0
ip link set eth0 up
# /etc/dnsmasq.conf:
#   interface=eth0
#   dhcp-range=192.168.50.100,192.168.50.200,12h
systemctl start dnsmasq
```

完整 CPE 软件栈也可以直接上 **OpenWrt**(Rockchip 已合 RK3568 BSP),内置 LuCI 配置 5G 模组、防火墙、QoS,省去手写 iptables。


## 七、踩坑清单

- **RK3566 OTG 只是 USB2,别拿 OTG 口接 5G 模组指望 USB3 速率**——必须走 `usbhost30`(USB PDF p.4 Note 4)。
- **combphy 互斥要在 DTS 里显式 disable 冲突控制器**,不 disable 会出现 PHY 抢占,模组随机不识别。SATA1 默认开,USB3 路径必须 `&sata1 { status = "disabled"; };`(PCIe PDF p.29 Wi-Fi 范例同款套路)。
- **VBUS / 模组 3.3V 上电至少留 50ms `startup-delay-us`**,否则模组 SOC 没启动完 USB 枚举就过来,认不出来。
- **W_DISABLE1#/W_DISABLE2# 必须默认拉高**——M.2 规范这两个低有效信号是"禁用射频",GPIO 浮空会被外部下拉默认禁用。用 `regulator-always-on` 的 fixed regulator 拉一个 GPIO 高即可。
- **PERST# 复位时间 5G 模组要 200ms+**(对比 Wi-Fi 模块的 500ms,见 PCIe PDF p.29),太短模组复位不完整链路 train 不起来。模组手册的 `T_PERST` 必须照搬到 `rockchip,perst-inactive-ms`。
- **PCIe + MHI 模式下,模组通常需要 `mhi_pm_resume` 显式唤醒**,不然休眠后接口黑掉。`mhi-pci-generic` 已有处理,但 mainline kernel ≥ 5.13 才齐全;RK kernel-6.1 OK,kernel-4.19 不要走 MHI。
- **2.5G LAN 不要寄希望 RK3566 自身 GMAC**。RK3566 GMAC 是 RGMII 千兆;想 2.5G 必须外加 RTL8125 走 PCIe — 但 PCIe 已经给了模组。**这是硬件层的死结**,只能换 RK3568(双 GMAC + 双 PCIe + USB3 OTG3) 或 RK3576/RK3588。


## 八、验证步骤(最小自检)

```bash
# USB3 路径
dmesg | grep -E "xhci|usb 2-|wwan"          # 看 USB3 enumerate 和 wwan 注册
lsusb | grep -i "Quectel\|Fibocom\|Sierra"
ls /dev/ttyUSB*                              # AT 串口出现 = 模组 PD 框架就绪
ip link | grep -E "wwan|usb"                 # 看到 wwan0 / usb0

# PCIe + MHI 路径
dmesg | grep -E "pcie|mhi"                   # MHI 注册 + BHIe DownLoad 成功
lspci -nn | grep -i qualcomm                 # 看到 modem 设备
ls /dev/mhi_*                                # 控制通道节点

# 拨号后
ip addr show wwan0     # 或 mhi_hwip0
ping -I wwan0 8.8.8.8
iperf3 -c <server>     # 测下行速率
```


## 九、模组型号 cheatsheet:Fibocom FM650(SDX62 平台)

| 维度 | FM650 |
|------|-------|
| 平台 | Qualcomm SDX62(X62 modem) |
| 封装 | M.2 3042 或 3052 B-Key |
| 主接口 | **PCIe Gen3 ×2(MHI)** — 接 RK3566 时会 downtrain 到 Gen2 ×1 |
| 次接口 | USB 3.2 Gen 1(USB3) 作为 fallback |
| 5G 速率(理论) | DL Cat-19 ~3.5 Gbps,UL Cat-18 ~900 Mbps |
| RK3566 实测瓶颈 | PCIe Gen2 ×1 ≈ 4 Gbps 物理带宽,够吃满 FM650 5G 实际速率(1.5~3 Gbps) |

**关键决策:RK3566 上 FM650 优先走 PCIe(MHI),不要走 USB3**。FM650 USB3 走 RNDIS/NCM,实际下行很难超过 2 Gbps,而 PCIe MHI 路径才能跑到 3 Gbps+。

### 驱动配套

- 内核:`MHI_BUS_PCI_GENERIC`(kernel ≥ 5.14 自带 SDX62 PID `0x17cb:0x0306` 的 table)。RK kernel-6.1 已就绪。若用 RK kernel-4.19/5.10,需 cherry-pick mainline 的 SDX62 PID table 补丁,或用 Fibocom 提供的 `pcie_mhi` out-of-tree 驱动。
- 用户态:Fibocom 提供 `fibocom-mbim`/`fibocom-CM` 工具集,或直接用 mainline `mbimcli`/`mmcli`(ModemManager ≥ 1.20 支持 SDX62)。

### FM650 控制/数据端口拓扑(PCIe MHI 模式)

```
/dev/wwan0at0    AT 命令(等价老接口的 /dev/ttyUSB2)
/dev/wwan0mbim0  MBIM 控制 + 数据(主用)
/dev/wwan0qmi0   QMI 控制(若启用)
mhi_hwip0        数据 netdev(MBIM/QMI 出来后内核挂的网卡)
```

### FM650 上电时序(M.2 PDF + Fibocom HW Guide 综合)

```
1. M.2 槽 3V3 + 3V3_AUX 上电  →  等 5ms
2. RESET# 释放(拉高)            →  等 100ms
3. PERST# 释放(拉高)            →  等 200ms 后 PCIe link up
4. W_DISABLE1# 必须保持高(默认拉高)否则射频关闭
5. PCIe 枚举完成,MHI 设备出现   →  ~3-8 秒
```

DTS 里要给 `&pcie2x1` 配:
```dts
reset-gpios = <&gpio3 RK_PCx GPIO_ACTIVE_HIGH>;   /* PERST# */
rockchip,perst-inactive-ms = <200>;                /* FM650 datasheet 要求 */
vpcie3v3-supply = <&vcc3v3_pcie_m2>;
```

并单独控制 `RESET#` 和 `W_DISABLE1#`:
```dts
fm650_pwr: fm650-pwr {
    compatible = "regulator-fixed";
    regulator-name = "fm650_pwr";
    enable-active-high;
    regulator-always-on;
    regulator-boot-on;
    gpios = <&gpio0 RK_PDx GPIO_ACTIVE_HIGH>;     /* W_DISABLE1# 拉高 */
};
```

### FM650 拨号最小例(MBIM via ModemManager)

```bash
# 模组识别(应看到 Fibocom FM650 / Qualcomm SDX62)
mmcli -L
mmcli -m 0

# 拨号(IPv4v6 双栈)
mmcli -m 0 --enable
mmcli -m 0 --simple-connect="apn=cmnet,ip-type=ipv4v6"

# 看分配到的 IP
mmcli -m 0 --bearer=0
ip addr show mhi_hwip0
```

### FM650 特有踩坑

- **必须确认你拿的是哪个 SKU**:FM650-CN(中国)、FM650-NA(北美)、FM650-EAU(欧亚)频段不同;不对 SKU 在国内插中国移动/电信卡可能搜不到网。
- **PCIe Gen3 ×2 → RK3566 Gen2 ×1 会自动降速,正常**——`lspci -vv` 看 `LnkSta: Speed 5GT/s, Width x1`,不是故障。
- **MHI 设备枚举慢(3-8s)**——上电后 `lspci` 立刻看可能没有,等 10s 再看。
- **`W_DISABLE` 信号容易被忽略**:M.2 槽位的 `W_DISABLE1#` (B-Key pin 26)和 `W_DISABLE2#` (pin 28)若任一被外部下拉,射频不出。原理图务必拉到 GPIO 软控或直接上拉。
- **休眠唤醒**:FM650 + RK3566 PCIe 二级休眠下 modem 链路可能掉,kernel ≥ 6.1 的 mhi 驱动配 `mhi_pm_suspend/resume` 才稳;参考 PCIe PDF 第 57 节"如何使得 PCIe 在二级休眠过程中保持工作状态"。


## 十、相关

- [[gmac-rgmii-delayline]] — 若 LAN 侧 GMAC 用 RGMII PHY,可能踩到 RGMII delay 坑
- [[chip-matrix#二、视频解码能力]] — 用于评估 RK3566 是否还要兼顾视频解码(VPU 不占 PCIe/USB,可同时使用)
- [[resources-map#二、Common 模块文档]] PCIe / USB / GMAC 章节
