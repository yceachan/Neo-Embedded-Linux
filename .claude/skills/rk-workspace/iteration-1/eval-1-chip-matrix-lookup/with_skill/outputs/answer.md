---
title: TSPI(RK3566) H.265 编解码能力速查
tags: [rk, rk3566, rk3568, codec, h265]
desc: TSPI 板 RK3566 的 H.265 编解码极限规格与 RK3568 的差异
update: 2026-05-27
---


# TSPI(RK3566) H.265 编解码能力速查

> [!note]
> **Ref:**
> - `sdk/tspi-rk3566-sdk/docs/readme_cn.md`(第 746-776 行,解码 / 编码能力规格表)
> - skill 内 [[chip-matrix]] 速查表

## 一、结论先行

- **H.265 编码**:RK3566 最高 **1920x1088 @ 60 fps**(即 1080p60),**与 RK3568 完全相同**。
- **H.265 解码**:RK3566 最高 **4096x2304 @ 60 fps**(即 4K60),**与 RK3568 完全相同**。
- RK3566 与 RK3568 是同 die / 管脚兼容,**VPU(编解码 IP)规格一致**,在视频编解码这个维度上**没有差别**。差异在 CPU 主频、Ethernet 路数、PCIe、SATA、衍生型号(工业级、宽温)这些外围,不影响 codec。

## 二、关键细节

### 编码能力(RK3566 / RK3568 同列)

| 编码格式 | 最大规格 |
|------|------|
| H.264 | 1920x1088 @ 60 fps |
| **H.265** | **1920x1088 @ 60 fps** |
| VP8 | N/A(不支持) |

### 解码能力(RK3566 / RK3568 同列)

| 解码格式 | 最大规格 |
|------|------|
| H.264 | 4096x2304 @ 60 fps |
| **H.265** | **4096x2304 @ 60 fps** |
| VP9 | 4096x2304 @ 60 fps |
| JPEG | 1920x1080 @ 60 fps |

### 与 RK3568 的差异(非 codec 维度,顺带说明)

| 维度 | RK3566 | RK3568 |
|------|--------|--------|
| 定位 | 消费类 / 商显 / 教育平板 | 工业 / 工控 / NVR / 商显 |
| CPU | 4×A55 @ 1.8 GHz | 4×A55 @ 2.0 GHz |
| Ethernet | 1×GMAC | 2×GMAC(双千兆) |
| PCIe | PCIe 2.1 ×1 | PCIe 3.0 ×1 + PCIe 2.1 ×1 |
| SATA | 无 | 3×SATA 3.0 |
| 衍生型号 | — | RK3568B2、RK3568J(工业级 / 宽温) |
| **VPU / NPU** | **相同(0.8 TOPS、同款编解码 IP)** | **相同** |

## 三、补充说明与注意点

1. **官方表标注的是"标定规格上限"**,实际跑 1080p60 H.265 编码时,码率、参考帧数、B 帧设置、内存带宽都会影响是否能跑满。Rockchip 在 readme_cn.md 的说明里也强调"实际搭配不同系统可能支持格式和性能会有所不同"。
2. **走 MPP 验证最快**:RK 平台编解码统一走 `external/mpp`,可以直接用 `mpi_enc_test` / `mpi_dec_test` 实测,详细 API 参见 `Rockchip_Developer_Guide_MPP_CN.pdf`。GStreamer / Rockit 上层用 `mpp` 作为底层。
3. **H.265 编码不支持 4K**:如果应用要 4K H.265 编码,RK3566/RK3568 无法满足,需上 RK3576(4K60)或 RK3588(8K30)。
4. **AV1 仅解码,不编码**:整代 RK 芯片(含 3566/3568)都只有 AV1 解码 IP,无编码。

## 四、引用

- 源文档:`/home/pi/imx/sdk/tspi-rk3566-sdk/docs/readme_cn.md` 第 746-776 行(解码 / 编码能力规格表)
- skill 速查:`/home/pi/imx/.claude/skills/rk/references/chip-matrix.md` 第 17-46 行(视频解码 / 编码表),第 86-101 行(RK3566 vs RK3568)
- 上层文档(本次未展开,需要时再读):`docs/cn/Common/MPP/Rockchip_Developer_Guide_MPP_CN.pdf`
- 本次未新建 distilled 文件:依 SKILL.md 第五节示例 2 的约定,"单数值不值得缓存",直接由 chip-matrix 命中作答。
