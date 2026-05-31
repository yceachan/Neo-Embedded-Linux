# Trace: eval-1 chip matrix lookup (without_skill)

## Goal
Answer user question about RK3566 H.265 codec capability, compare with RK3568, and add decoder upper limit.

## Tools / sources used
- `ls` on `sdk/tspi-rk3566-sdk/docs/` to discover available SDK docs.
- `find` for `*mpp*` / `*codec*` / `*datasheet*` to locate authoritative spec PDFs.
- **Primary source (RK3566)**: `sdk/tspi-rk3566-sdk/docs/cn/RK3566_RK3568/Datasheet/Rockchip_RK3566_Datasheet_V1.4-20240621.pdf`, read pages 1-5 (TOC) then 6-17 (Chapter 1 Features, including §1.2.7 Video CODEC).
- **Comparison source (RK3568)**: `sdk/tspi-rk3566-sdk/docs/cn/RK3566_RK3568/Datasheet/Rockchip_RK3568_Datasheet_V2.1-20240621.pdf`, pages 9-11 (§1.2.7 Video CODEC).
- Did NOT invoke the `rk` skill; relied only on raw datasheet PDFs already in repo and prior knowledge of RK356x family for context (e.g. RK3568 has extra PCIe/SATA/GMAC, HDMI 4K60 vs RK3566 4K30).

## Key extracted facts
- RK3566 §1.2.7 Video Encoder: `H.265/HEVC MP@level4.1, up to 1920x1080@60fps (4096x4096@10fps with TILE)`.
- RK3566 §1.2.7 Video Decoder: `H.265 HEVC/MVC Main10 Profile yuv420@L5.1 up to 4096x2304@60fps`.
- RK3568 §1.2.7 Video Encoder / Decoder: **逐字相同** with RK3566 — confirms codec parity.
- Overview line in RK3566 datasheet p.6: "H.265 decoder by 4K@60fps... H.264/H.265 encoder by 1080p@60fps".

## Cross-check
- Block diagram on RK3566 datasheet p.17 labels the multi-media block as "4K Video Decoder" + "1080p Video Encoder", matching the §1.2.7 numbers.
- No discrepancy found between RK3566 and RK3568 datasheet codec sections after side-by-side compare.

## Output written
- `outputs/answer.md` — final Chinese answer for the user.
- `outputs/trace.md` — this file.
