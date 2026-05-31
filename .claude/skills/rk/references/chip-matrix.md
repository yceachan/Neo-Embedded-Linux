---
title: Rockchip 芯片能力速查矩阵
tags: [rk, skill, reference, chip-matrix]
desc: 各 RK 芯片在视频编解码、ISP、休眠等维度的能力差异速查表,源自 SDK readme_cn.md
update: 2026-05-27
---


# Rockchip 芯片能力速查矩阵

> [!note]
> **Ref:** [`sdk/tspi-rk3566-sdk/docs/cn/readme_cn.md`](../../../../sdk/tspi-rk3566-sdk/docs/readme_cn.md) 的多媒体能力章节、各模块文档清单。
>
> 用途:用户问"RK3566 支持 H265 多大?"或"RK3588 ISP 是哪个版本?"时,先查这里,命中即答,不必开 PDF。表格之外的细节再去 [[resources-map]] 找 PDF。


## 一、视频解码能力

| 芯片 | H.264 | H.265 | VP9 | JPEG | 备注 |
|------|-------|-------|-----|------|------|
| RV1126B | 3840x2160@30 | 3840x2160@30 | N/A | 3840x2160@30 | RV 系列,低功耗 |
| RK3588 | 7680x4320@30 | 7680x4320@60 | 7680x4320@60 | 1920x1088@200 | 旗舰 8K |
| RK3576 | 3840x2160@60 | 7680x4320@30 | 7680x4320@30 | 1920x1088@200 | 8K 解码,4K 编码 |
| RK3566 / RK3568 | 4096x2304@60 | 4096x2304@60 | 4096x2304@60 | 1920x1080@60 | 同 die,管脚兼容 |
| RK3562 | 1920x1088@60 | 2304x1440@30 | 4096x2304@30 | 1920x1080@120 | 中端 |
| RK3399 | 4096x2304@30 | 4096x2304@60 | 4096x2304@60 | 1920x1088@30 | 老旗舰 |
| RK3328 | 4096x2304@30 | 4096x2304@60 | 4096x2304@60 | 1920x1088@30 | OTT 盒子 |
| RK3288 | 3840x2160@30 | 4096x2304@60 | N/A | 1920x1080@30 | 上一代 |
| RK3326 / PX30 | 1920x1088@60 | 1920x1088@60 | N/A | 1920x1080@30 | 入门 |
| RK312X | 1920x1088@30 | 1920x1088@60 | N/A | 1920x1080@30 | 入门 |


## 二、视频编码能力

| 芯片 | H.264 | H.265 | VP8 |
|------|-------|-------|-----|
| RV1126B | 3840x2160@30 | 3840x2160@30 | N/A |
| RK3588 | 7680x4320@30 | 7680x4320@30 | 1920x1088@30 |
| RK3576 | 4096x2304@60 | 4096x2304@60 | N/A |
| RK3566 / RK3568 | 1920x1088@60 | 1920x1088@60 | N/A |
| RK3562 | 1920x1088@60 | N/A | N/A |
| RK3399 | 1920x1088@30 | N/A | 1920x1088@30 |
| RK3328 | 1920x1088@30 | 1920x1088@30 | 1920x1088@30 |
| RK3288 | 1920x1088@30 | N/A | 1920x1088@30 |
| RK3326 / PX30 / RK312X | 1920x1088@30 | N/A | 1920x1088@30 |


## 三、ISP 版本与芯片对应

| ISP 版本 | 适用芯片 |
|----------|----------|
| ISP1.X | RK3399 / RK3288 / PX30 / RK3326 / RK1808 |
| ISP21 | RK3566 / RK3568 |
| ISP30 | RK3588 |
| ISP32-lite | RK3562 |
| ISP35 | RV1126B |
| ISP39 | RK3576 |


## 四、休眠唤醒文档覆盖

`cn/Common/TRUST/` 下提供平台专属 System Suspend 指南:

| 芯片 | PDF |
|------|-----|
| RK3308 | `Rockchip_RK3308_Developer_Guide_System_Suspend_CN.pdf` |
| RK3399 | `Rockchip_RK3399_Developer_Guide_System_Suspend_CN.pdf` |
| RK3506 | `Rockchip_RK3506_Developer_Guide_System_Suspend_CN.pdf` |
| RK3566 / RK3568 | `Rockchip_RK356X_Developer_Guide_System_Suspend_CN.pdf` |
| RK3576 | `Rockchip_RK3576_Developer_Guide_System_Suspend_CN.pdf` |
| RK3588 | `Rockchip_RK3588_Developer_Guide_System_Suspend_CN.pdf` |


## 五、USB 平台专属文档

`cn/Common/USB/` 下除通用文档外,还有平台专属:

| 芯片 | PDF |
|------|-----|
| RK3399 | `Rockchip_RK3399_Developer_Guide_USB_CN.pdf` |
| RK3566 / RK3568 | `Rockchip_RK356x_Developer_Guide_USB_CN.pdf` |
| RK3576 | `Rockchip_RK3576_Developer_Guide_USB_CN.pdf` |
| RK3588 | `Rockchip_RK3588_Developer_Guide_USB_CN.pdf` |


## 六、RK3566 vs RK3568 区别速记

两颗芯片同 die、管脚兼容,差异点:

| 维度 | RK3566 | RK3568 |
|------|--------|--------|
| 定位 | 消费类 / 商显 / 教育平板 | 工业 / 工控 / NVR / 商显 |
| CPU | 4×A55 @ 1.8GHz | 4×A55 @ 2.0GHz(更高频可选) |
| Ethernet | 1×GMAC | 2×GMAC(双千兆) |
| PCIe | PCIe 2.1 ×1 | PCIe 3.0 ×1 + PCIe 2.1 ×1 |
| SATA | N/A | 3×SATA 3.0 |
| USB3 | 1×USB3.0 OTG | 1×USB3.0 OTG |
| 衍生型号 | — | RK3568B2(B0 stepping)、RK3568J(工业级,宽温 -40~85°C) |
| NPU | 0.8 TOPS(共用) | 0.8 TOPS(共用) |

> 实际差异要查 `Datasheet/Rockchip_RK3566_Datasheet_V1.4-*.pdf` 与 `Rockchip_RK3568_Datasheet_V2.1-*.pdf`。


## 七、PMIC 配对建议

Rockchip SoC 与自家 PMIC 的典型搭配:

| SoC 系列 | 推荐 PMIC |
|----------|----------|
| RK3566 / RK3568 | RK809(主流) / RK817 |
| RK3588 / RK3588S | RK806 |
| RK3576 | RK806 |
| RK3399 | RK808 |
| RK3328 / RK3288 | RK808 / RK818 |
| 低端 RK312X / PX30 | RK817 / RK805 |

PMIC 详细规格见 [[resources-map#PMIC]] 列出的对应 PDF。
