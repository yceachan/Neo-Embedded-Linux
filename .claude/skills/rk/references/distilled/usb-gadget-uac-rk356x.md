---
title: RK356x USB Gadget UAC2 (麦克风+扬声器) 快速上手
tags: [rk, distilled, usb, gadget, uac, rk356x, rk3566, rk3568]
desc: RK356x OTG 口当作 USB Audio Class 2.0 双向声卡的 SDK 现成方案、configfs 节点与切换命令
source:
  - cn/Common/USB/Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf
  - cn/Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf
  - cn/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf
source_pages: "UAC:1-18; QuickStart:4-11; RK356x USB:31-32"
update: 2026-05-27
---


# RK356x USB Gadget UAC2 快速上手

> [!note]
> **Ref:**
> - PDF: `cn/Common/USB/Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf` p.1-18 (功能说明、configfs 脚本、parameters、Windows/Linux 兼容性)
> - PDF: `cn/Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf` p.4-11 (`/etc/init.d/S50usbdevice` + `.usb_config` 一键方案)
> - PDF: `cn/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf` p.31-32 (RK356x OTG mode 切换命令)

## 背景

USB Audio Class (UAC) 是 USB 标准音频协议族。Rockchip 平台用内核自带的 `g_audio` / configfs UAC function (`CONFIG_USB_CONFIGFS_F_UAC1`、`CONFIG_USB_CONFIGFS_F_UAC2`) 把 OTG 控制器封装成 PC 可直接识别的标准声卡——**Device 端无须实际 ACodec 也能跑通**,因为 UAC1/UAC2 driver 在内核里构造的是"虚拟 PCM 设备",PC 看到一个 USB 声卡,Device 看到一对 `/dev/snd/pcmC*D0p|c` 节点 (p.3、p.8)。

UAC2 与 UAC1 的关键差异:
- UAC2 使用 USB High Speed,带宽充裕,可达 32 bit + 384 kHz;UAC1 限 USB Full Speed(理论 24 bit/96 kHz)。
- Windows 10 1703 之前不原生支持 UAC2;UAC1 全平台 plug & play。
- UAC2 的 Feature Unit 音量控制在 Windows 上不兼容,**Windows 场景必须关闭** `c_feature_unit / p_feature_unit` (p.14、p.16 表 2-1)。

`c_*` 是 capture (PC 视角的录音 = Device 把声音发给 PC 当麦克风);`p_*` 是 playback (PC 视角的放音 = Device 接收 PC 的扬声器流) (p.13)。

## SDK 现成方案

Rockchip Linux SDK 的 `/etc/init.d/S50usbdevice` + `/etc/init.d/.usb_config` + `/usr/bin/usbdevice` + `/lib/udev/rules.d/61-usbdevice.rules` 一套脚本已经支持 UAC、ADB、RNDIS、MTP、UMS、ACM、UVC 等 function 一键开关 (QuickStart p.6)。

**一键开 UAC2**:

```sh
echo usb_uac2_en > /tmp/.usb_config
/etc/init.d/S50usbdevice restart
```

(`usb_uac1_en` 同理。QuickStart p.8)

启动成功 dmesg 关键字:

```
android_work: sent uevent USB_STATE=CONNECTED
configfs-gadget gadget: high-speed config #1: b
android_work: sent uevent USB_STATE=CONFIGURED
```

Device 端 `aplay -l` 会看到新增声卡 `card N: rk3xxx [rk3xxx], device 0: USB Audio`。PC 端枚举出 USB Audio 输入/输出设备 (QuickStart p.8)。

## 关键 kbuild

```
CONFIG_USB_CONFIGFS_F_UAC1=y   # UAC1
CONFIG_USB_CONFIGFS_F_UAC2=y   # UAC2,推荐
# 可选
CONFIG_USB_CONFIGFS_F_UAC1_LEGACY=y  # 需要绑实际声卡的 UAC1 legacy
CONFIG_USB_CONFIGFS_F_ACC + CONFIG_USB_CONFIGFS_F_AUDIO_SRC=y  # Audio Source(多采样率录音)
```

(UAC PDF p.6 §1.2)

## configfs 参数(`/sys/kernel/config/usb_gadget/<g>/functions/uac2.gs0/`)

| 节点 | 含义 | 典型值 |
|------|------|--------|
| `p_chmask` / `c_chmask` | 播放/录音通道位图 | `3` (双声道) `0x3F` (5.1) |
| `p_ssize` / `c_ssize` | 位深 (字节) | `2` (16-bit) `3` (24-bit) `4` (32-bit) |
| `p_srate` / `c_srate` | 采样率(可多值, 逗号隔开) | `8000,16000,44100,48000` |
| `p_feature_unit` / `c_feature_unit` (4.4/4.19) | UAC1: 音量控制开关 | UAC1=1, UAC2=0 (Windows 兼容) |
| `p_volume_present` / `p_mute_present` (5.10+) | UAC2 音量/静音使能 | 5.10 内核默认已使能 |
| `p_volume_min/max/res` | 音量范围(单位 1/256 dB) | 默认 -100dB/0dB/1dB |
| `req_number` | URB buffer 数 | 默认 2,Mac OS 兼容性差时调大 |

(UAC PDF p.13-14)

## RK356x OTG mode 强制切换

如果硬件不靠 Type-C CC 或 OTG ID 自动协商,可以软件强制把 OTG 口切到 Peripheral (Device) 模式 (RK356x USB PDF p.31):

```sh
# Linux-5.10 新接口(推荐)
echo device > /sys/kernel/debug/usb/fcc00000.usb/mode    # 切 device
echo host   > /sys/kernel/debug/usb/fcc00000.usb/mode    # 切 host

# Legacy 接口(依赖 DTS 的 extcon 属性)
echo peripheral > /sys/devices/platform/fe8a0000.usb2-phy/otg_mode
echo host       > /sys/devices/platform/fe8a0000.usb2-phy/otg_mode
echo otg        > /sys/devices/platform/fe8a0000.usb2-phy/otg_mode
```

RK3566 的 USB 控制器节点名一般是 `fcc00000.usb` (USB 2.0/3.0 OTG)。地址实测以 `ls /sys/class/udc/` 为准。

## 测试方法 (PC 端 ↔ Device 端)

Device 端把 PC 的 playback 流回环到 capture,验证双向 (QuickStart p.9):

```sh
# Device shell: 把 hw:N,0 上的录音(来自 PC) 重定向回 playback(送给 PC)
arecord -D "hw:3,0" -f S16_LE -r 48000 -c 2 -t raw -N |
  aplay -D "hw:3,0" -f S16_LE -r 48000 -c 2 -t raw -N
```

PC 端在 Windows 声音设置里把 USB 音频设备选为"输入设备"和"输出设备";Linux 用 `pavucontrol`。

## 易踩坑

- **UAC2 + Windows 必须关 Feature Unit**,即 `echo 0 > c_feature_unit; echo 0 > p_feature_unit`,否则驱动加载异常 (p.14、p.16)。
- Windows 会**缓存 USB 设备驱动**,修改 idProduct/function 后建议在设备管理器里卸载该设备再重新插拔,否则旧描述符被沿用 (p.9、p.12)。
- `c` / `p` 方向**容易反**:`c_*` = Host 录音 = Device 上传;`p_*` = Host 放音 = Device 接收 (p.13)。
- 异源时钟 (USB host clock vs codec clock) 导致 `alsa underrun/overrun`,RV1126-Linux-4.19 提供 PPM 补偿 uevent,RK356x 暂未在 SDK 提供 (p.15 §2.3.3)。
- 想加物理按键调音量/静音需要叠 HID function,**当前 SDK 不带**,要参考 `Documentation/usb/gadget-testing.txt` 自己加 (p.3)。
- idProduct 自定义时,**不能与已启用的其他 function 冲突**,例如同时叠 ADB 时要换号 (p.12)。
- OTG 口若硬件无 Type-C CC 检测,需要在 DTS 加 `dr_mode = "peripheral"` 或用 `echo device` 强切;否则上电默认进 host 看不到 Device 枚举 (RK356x USB p.31)。

## 进阶

- **Composite (复合)** 设备:`.usb_config` 里同时 `echo usb_uac2_en + usb_adb_en + ...` 可拼出 UAC + ADB + RNDIS 复合枚举 (QuickStart p.14 §2.3)。
- **多采样率切换通知**:内核通过 uevent `USB_STATE=SET_SAMPLE_RATE STREAM_DIRECTION=... SAMPLE_RATE=...` 上报应用层 `uac_app`,应用层可据此切 ALSA 配置 (UAC p.14-15)。
- **UAC1 Legacy / Audio Source**:UAC1 Legacy 需要真实声卡只能放音;Audio Source 只能录音但支持 15 种采样率。普通"麦+喇叭"场景**直接选 UAC2**(或 UAC1)即可 (UAC PDF §3-5)。

## 相关

- [[chip-matrix]] (RK356x USB 控制器拓扑)
