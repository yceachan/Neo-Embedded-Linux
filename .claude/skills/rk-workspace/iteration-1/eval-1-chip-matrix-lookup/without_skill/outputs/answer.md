---
title: RK3566 H.265 编解码能力上限及与 RK3568 对比
tags: [RK3566, RK3568, H.265, HEVC, VPU, codec]
desc: TSPI 板（RK3566）H.265 编/解码能力及与 RK3568 的差异
update: 2026-05-27
---


# RK3566 H.265 编解码能力上限及与 RK3568 对比

> [!note]
> **Ref:**
> - `sdk/tspi-rk3566-sdk/docs/cn/RK3566_RK3568/Datasheet/Rockchip_RK3566_Datasheet_V1.4-20240621.pdf` §1.2.7 Video CODEC
> - `sdk/tspi-rk3566-sdk/docs/cn/RK3566_RK3568/Datasheet/Rockchip_RK3568_Datasheet_V2.1-20240621.pdf` §1.2.7 Video CODEC

## 直接结论

**RK3566 和 RK3568 的视频编解码能力完全一致**——两颗芯片共用同一套 VPU/VDPU 硬件 IP，datasheet 第 1.2.7 节的描述逐字相同。它们的差异在 CPU 频率、PCIe/SATA/USB3 通道数、千兆以太网数量、显示输出能力等外围方面，不在 codec 上。

## H.265 编码（Encoder, VEPU）

| 项目 | 上限 |
|------|------|
| Profile / Level | **H.265 Main Profile @ Level 4.1** |
| 常规最大分辨率/帧率 | **1920×1080 @ 60fps**（1080p60） |
| TILE 模式最大分辨率 | **4096×4096 @ 10fps**（仅 10fps，相当于静态/慢速场景） |
| 输入格式 | YUV / RGB，支持旋转、镜像 |

要点：
- 实时编码场景，**官方承诺到 1080p@60fps 为止**，不要在工程评估时按 4K60 来规划码流和码控；
- 4K（4096×4096）那一档是 TILE 拼帧路径，**只能跑到 10fps**，适合做截图/录屏/低帧率监控，不适合直播；
- 同芯片的 H.264 编码也是 1080p@60fps（BP/MP/HP @ L4.2）。

## H.265 解码（Decoder, VDPU）

| 项目 | 上限 |
|------|------|
| Profile | **H.265/HEVC Main 10 Profile**（支持 10-bit） |
| 色度采样 | yuv420 |
| Level | **L5.1** |
| 最大分辨率/帧率 | **4096×2304 @ 60fps**（4K60，超过常规 3840×2160） |

对照其它解码：
- H.264 AVC/MVC Main10：4096×2304@60fps（与 H.265 同档）
- VP9 Profile 0/2：4096×2304@60fps
- VP8：1920×1088@60fps

## 编/解码不对称小结

| 维度 | 解码（看片） | 编码（推流/录制） |
|------|------|------|
| H.265 | 4K60 + 10bit | 1080p60 (Main, 8bit) |
| 4K 用途 | 可以播 4K HDR 视频 | 只能 10fps 截图级 |

工程上的含义：**RK3566/RK3568 是"重解码、轻编码"型 SoC**，做盒子、电子标牌、4K 信息播放没问题；做 4K 编码推流要换 RK3588（4K60 编码 + 8K30 解码）。

## 与 RK3568 的差别（codec 之外）

虽然 codec 一样，但选型时仍要分清：

- RK3568 比 RK3566 多 1 路 PCIe2.1、多 1 路 SATA3.0、多 1 路千兆以太网，CPU 主频也更高一档（典型 2.0GHz vs 1.8GHz）；
- 显示侧 RK3566 限制更紧（HDMI 最高 1080p@60Hz / 4K@30Hz；RK3568 支持 4K@60Hz）；
- 你手上的 TSPI 板是 RK3566，做 4K 视频解码 + HDMI 4K 输出的组合时，注意 HDMI 那一端可能只能跑 4K30 或 1080p60，并不是 VPU 本身的限制。
