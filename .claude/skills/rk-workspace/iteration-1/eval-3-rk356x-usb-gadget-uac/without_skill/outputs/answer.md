---
title: RK3568 USB Gadget UAC2 麦克风+扬声器方案
tags: [RK3568, USB-Gadget, UAC2, OTG, configfs]
desc: SDK 现成方案、流程要点、相关 PDF 索引
update: 2026-05-27
---


# RK3568 当成 UAC2 设备（麦克风+扬声器）方案速查

> [!note]
> **Ref（本仓库实测路径）：**
> - `sdk/tspi-rk3566-sdk/docs/cn/Common/USB/Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf` (V1.3.1, 43 页)
> - `sdk/tspi-rk3566-sdk/docs/cn/Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf` (V1.0.0)
> - `sdk/tspi-rk3566-sdk/docs/cn/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf` (V1.4.0)

## 一、有没有现成方案：**有，开箱即用**

Rockchip Linux SDK 对 UAC1/UAC2 **同时支持录音 + 放音**（双向），不需要实际声卡；BSP 内核已经把 `f_uac1 / f_uac2` 通过 configfs 跑通，并在 buildroot rootfs 里集成了 `S50usbdevice + .usb_config` 一键脚本。

| 能力 | UAC1 | UAC2 | 备注 |
|---|---|---|---|
| 录音(c_*) | YES | YES | 对应 PC 端"麦克风" |
| 放音(p_*) | YES | YES | 对应 PC 端"扬声器" |
| 多采样率 | YES | YES | 8/16/44.1/48 kHz... |
| 多声道 | YES | YES | chmask 配置 |
| 音量/静音 (Feature Unit) | YES | YES | UAC2 在 Windows 不兼容,建议 UAC1 开/UAC2 关 |
| Windows 原生驱动 | 全部支持 | Win10 1703 之后才原生支持 |
| 时钟同步 (PPM Compensation) | 仅 RV1126-4.19 | — | RK356x 不依赖,Codec 用 USB 时钟即可 |

> **重要：** UAC1 走 USB Full Speed 受 1023 byte/ms 限制(理论上限 24bit/96kHz 2ch)；要更高规格走 UAC2 (High Speed)。Rockchip UAC1 默认 `bInterval=4` 跑 HighSpeed，兼容性比 UAC2 更好。

## 二、大致流程（从硬件到出声 6 步）

```mermaid
flowchart LR
    A["1.硬件 OTG 口确认"] --> B["2.DTS:vbus/dr_mode/PHY"]
    B --> C["3.内核 CONFIG_USB_CONFIGFS_F_UAC2=y"]
    C --> D["4./etc/init.d/.usb_config 写 usb_uac2_en"]
    D --> E["5.S50usbdevice restart 拉起 gadget"]
    E --> F["6.PC 端 选 Source/Sink 双向走 ALSA"]
```

### 1. 确认 OTG 硬件（**最先要明确**）

RK3568 上有 **两路** DWC3 控制器，OTG 是 `fcc00000.usb`(USB3.0 OTG)；RK3566 上 OTG 只有 USB 2.0。TSPI-RK3566 板的 OTG 是 USB2.0 Type-C，硬件电路类型决定 DTS 写法：

- **Micro-B / Type-A**：DTS 用 `dr_mode = "otg"` + `extcon` 节点，由 ID 引脚或软件强切；
- **Type-C No PD**：CC1/CC2 直连芯片 + 上拉电阻方案，靠 CC 电平 + ID 中断切 Host/Device；
- **Type-C + PD**：需要外置 FUSB302/HUSB311 等 PD 芯片，Linux-5.10 用 `dr_mode="otg"` + `usb-role-switch`。

### 2. DTS（关键三处）

```dts
&u2phy0_otg {
    vbus-supply = <&vcc5v0_otg>;   // OTG 口 VBUS 受软件控制
    rockchip,vbus-always-on;        // Device 模式常用,防止 PHY autosuspend
    status = "okay";
};

&usbdrd_dwc3 {
    dr_mode = "otg";                // 或 "peripheral" 强制 device
    // Type-C+PD 方案还要加 usb-role-switch
};
```

VBUS 控制方案三选一：GPIO + LDO（最常用）、PMIC 输出（RK809/RK817）、硬件常供电。

### 3. 内核 defconfig

```
CONFIG_USB_CONFIGFS=y
CONFIG_USB_CONFIGFS_F_UAC2=y     # 想兼容老 Win7 就同时开 F_UAC1
```

Linux-4.4/4.19 还要确认内核已合入文档第 1.1 节列出的一长串补丁（uevent for ppm、volume control、多采样率、`fix dev_dbg` 等），否则会有 underrun/overrun、音量上报错位、PC 不识别等问题。Linux-5.10 已上游化，但仍需 `usb: gadget: u_audio: add uevent for ppm compensation` 这一条。

### 4. 应用层配置（两种入口）

**Kernel-5.10 SDK 已集成**到 `/oem/usr/bin/usb_config.sh` 的 `uac2_device_config()`，关键参数（写 configfs 节点）：

```sh
mkdir -p /sys/kernel/config/usb_gadget/rockchip/functions/uac2.gs0
UAC_GS0=/sys/kernel/config/usb_gadget/rockchip/functions/uac2.gs0
echo 3                     > $UAC_GS0/p_chmask   # 放音 2ch (bit0|bit1)
echo 2                     > $UAC_GS0/p_ssize    # 16 bit
echo 8000,16000,44100,48000 > $UAC_GS0/p_srate
echo 1                     > $UAC_GS0/p_volume_present
echo 1                     > $UAC_GS0/p_mute_present
echo 3                     > $UAC_GS0/c_chmask   # 录音 2ch
echo 2                     > $UAC_GS0/c_ssize
echo 8000,16000,44100,48000 > $UAC_GS0/c_srate
echo 4                     > $UAC_GS0/req_number # MAC OS 兼容,加大缓冲
```

**Kernel-4.4/4.19** 用 `S50usbdevice` 脚本 + `/etc/init.d/.usb_config` 开关：

```sh
echo usb_uac2_en > /tmp/.usb_config   # 也可写 usb_uac1_en
/etc/init.d/S50usbdevice restart      # 写 UDC 完成枚举
```

### 5. 复合设备（与 ADB / RNDIS 共存）

直接往 `/tmp/.usb_config` 多行 `echo` 追加 `usb_adb_en / usb_rndis_en / usb_uac2_en`，再 `restart`。注意 idProduct 不要冲突、端点带宽要够。

### 6. PC 端验证

- **Windows 10 1703+**：插上自动出"扬声器 (Source/Sink)" + "麦克风 (Source/Sink)"，右键 Sound Settings 即可；UAC2 在 Win7 需要装第三方驱动。
- **Linux 主机**：`aplay -l` 看到 `USB Audio`，跑 `aplay -D plughw:2,0 -f S16_LE -r 48000 -c 2 test.wav` 把声音"放给板子"，板上用 `arecord -D hw:1,0 -f S16_LE -r 48000 -c 2 -t raw | aplay -D hw:0,0 ...` 做 capture→codec 回环。

应用层还需要监听 `uevent` 来响应 PC 的开/关流、采样率切换、音量调节：

```
g_audio_work: sent uac uevent USB_STATE=SET_INTERFACE STREAM_DIRECTION=OUT STREAM_STATE=ON
g_audio_work: sent uac uevent USB_STATE=SET_SAMPLE_RATE STREAM_DIRECTION=OUT SAMPLE_RATE=44100
g_audio_work: sent uac uevent USB_STATE=SET_VOLUME STREAM_DIRECTION=IN VOLUME=-1154
```

SDK 提供的 `uac_app` 就是参考用户态实现，把这些 uevent 映射到 codec 的 mixer 控制。

## 三、相关 PDF 清单（按优先级）

| 优先级 | PDF (路径相对 `sdk/tspi-rk3566-sdk/docs/cn/`) | 用途 |
|---|---|---|
| ★★★ | `Common/USB/Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf` | UAC1/UAC2 主参考(43 页),内含完整 configfs 脚本、参数节点、uevent 协议、PPM 补偿、UAC1-Legacy/Audio Source 复合 |
| ★★★ | `Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf` | Gadget 框架入门 + `S50usbdevice` 一键开关流程,2.2.2 节专讲 UAC 上手 |
| ★★★ | `Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf` | RK356x 控制器/PHY 框架、OTG 硬件电路 (Micro-B/Type-C/Type-C+PD)、VBUS 配置、DTS 属性、**OTG mode 软切换命令** |
| ★★ | `Common/USB/Rockchip_Developer_Guide_USB_CN.pdf` | Rockchip 通用 USB 开发指南(跨平台基础) |
| ★★ | `Common/AUDIO/Rockchip_Developer_Guide_Audio_CN.pdf` | ASoC/codec 侧,真要接物理 codec 时配套看 |
| ★ | `Common/USB/Rockchip_Developer_Guide_Linux_USB_Initialization_Log_Analysis_CN.pdf` | 枚举失败时看 dmesg 用 |
| ★ | `Common/USB/Rockchip_Developer_Guide_Linux_USB_Performance_Analysis_CN.pdf` | underrun/overrun 调优 |

另外内核 `Documentation` 里这几个文件是 configfs 节点 ABI 的 source-of-truth：

- `Documentation/usb/gadget_configfs.txt`
- `Documentation/usb/gadget-testing.txt`（HID 加按键、音量控制方案）
- `Documentation/ABI/testing/configfs-usb-gadget-uac1`
- `Documentation/ABI/testing/configfs-usb-gadget-uac2`

## 四、踩坑提醒

1. **TSPI-RK3566 OTG 只有 USB 2.0**：UAC2 最高也只能跑 HS(480 Mbps)，做 32bit/384kHz 大流量场景要换 RK3568 的 USB3.0 OTG。
2. **Windows 会缓存设备驱动**：改了 idProduct/idVendor 或配置后，Windows 端最好卸载设备让它重枚举；脚本里通常 `echo none > UDC; sleep 1; echo $UDC > UDC` 来强制重连。
3. **UAC2 + Feature Unit + Windows**：Win 下 UAC2 的 Feature Unit 音量协议有坑，官方建议 UAC1 开 Feature Unit、UAC2 关掉，或者用 HID 上报音量按键。
4. **MAC OS 兼容性**：MAC 不接受 "NO DATA" 同步包，必须把 `req_number` 加大（如 4），否则会丢包。
5. **OTG 强切**：硬件电路不带 ID/CC 自动切换时（例如把 Type-A 当 device 用），需手动：
   ```
   # Linux-5.10
   echo device > /sys/kernel/debug/usb/fcc00000.usb/mode
   # Linux-4.19
   echo peripheral > /sys/devices/platform/fe8a0000.usb2-phy/otg_mode
   ```
