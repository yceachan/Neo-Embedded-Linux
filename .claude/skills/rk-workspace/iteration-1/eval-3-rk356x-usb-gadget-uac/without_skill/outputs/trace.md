---
title: Trace - RK3568 UAC2 调研过程
tags: [trace]
desc: 本任务实际查阅的文件与关键发现
update: 2026-05-27
---


# Trace

## 调研路径

1. **定位 SDK** — `ls /home/pi/imx/sdk/tspi-rk3566-sdk/` 确认是 Rockchip 官方目录树(app/common/device/docs/external/hal/kernel-6.1/u-boot/buildroot/yocto)。
2. **PDF 检索** — `find docs -iname "*.pdf"` + `grep -iE "usb|audio|uac|gadget"` 一把锁定 3 个主文档 + 若干辅助文档。
3. **逐份读 PDF**（用 Read 工具的 pages 参数,每次最多 20 页）：
   - `Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf` p.1-15：拿到目录、Kernel CONFIG、补丁清单、configfs 脚本范式、参数节点、uevent 协议、PPM。
   - `Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf` p.1-15：拿到 S50usbdevice / `/tmp/.usb_config` 一键开关流程；2.2.2 节 UAC 双向回环验证步骤。
   - `Rockchip_RK356x_Developer_Guide_USB_CN.pdf` p.1-15 + p.29-33：拿到 RK3566 vs RK3568 控制器差异(RK3566 OTG 仅 USB2.0)、VBUS/DTS 配置、Type-C 三种电路、OTG mode 软切换命令、Linux-4.19/5.10 节点路径差异。

## 关键发现速记

- RK3566 OTG 只能 USB2.0；RK3568 OTG 是 USB3.0；这点直接影响 UAC2 带宽上限。
- SDK 默认就把 UAC1/UAC2 通过 configfs + S50usbdevice 跑通了；用户层只要 `echo usb_uac2_en > /tmp/.usb_config && restart`。
- Linux-5.10 的脚本入口是 `/oem/usr/bin/usb_config.sh`（含 `uac1_device_config / uac2_device_config` 函数）；4.4/4.19 是 `/etc/init.d/S50usbdevice` + `.usb_config`。
- UAC2 在 Windows 10 1703 之前不被原生支持；Feature Unit 音量在 Win 下 UAC2 有兼容性问题，建议 UAC1 开/UAC2 关或用 HID。
- 实际产品 Codec 时钟与 USB Host 时钟不同源，会 underrun/overrun，需 PPM compensation（当前 SDK 只有 RV1126-4.19 上游化了，RK356x SDK 暂时没有，但 Codec 直接用 USB SOF 同步可规避）。
- OTG mode 软切换在 Linux-5.10 走 `/sys/kernel/debug/usb/<addr>.usb/mode`；4.19 走 `/sys/devices/platform/<addr>.usb2-phy/otg_mode`。

## 用到的工具

- Bash（ls / find / grep） - 定位 SDK、列出/过滤 PDF
- Read（PDF 模式, pages 参数） - 逐页提取 3 份关键文档内容

## 未深入但已记录

- `Rockchip_Developer_Guide_Audio_CN.pdf`（codec 侧；本题问的是 UAC,可选阅读）
- `Documentation/ABI/testing/configfs-usb-gadget-uac1/uac2`（内核侧 ABI，PDF 文档里已经引用）
- buildroot 包路径 (`BR2_PACKAGE_*`)：实际打包 UAC 用户态应用时需要,本次未展开
