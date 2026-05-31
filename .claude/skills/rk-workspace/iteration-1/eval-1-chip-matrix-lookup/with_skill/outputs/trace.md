# Trace —— eval-1-chip-matrix-lookup (with_skill)

## 用户问题
TSPI(RK3566)H.265 编码能到多少分辨率 / 帧率?跟 RK3568 有差别吗?解码上限多少?

## Step 1 —— Triage(按 SKILL.md 第三节)
- **SoC**: RK3566(用户已指明),对比目标 RK3568
- **模块**: VPU codec / H.265
- **意图**: 能力规格查询(单数值类查询)

## Step 2 —— 查 distilled INDEX
- 读 `/home/pi/imx/.claude/skills/rk/references/distilled/INDEX.md`
- 结果:索引为空 → miss

## Step 3 —— 定位源
- SKILL.md 第三节明文指引:"简单型号能力问题(如 'RK3566 编码最高多少')直接查 [[chip-matrix]],不用开 PDF"
- 读 `/home/pi/imx/.claude/skills/rk/references/chip-matrix.md`
  - 表二"视频编码能力"命中 RK3566/RK3568 H.265 = 1920x1088@60
  - 表一"视频解码能力"命中 RK3566/RK3568 H.265 = 4096x2304@60
  - 第六节"RK3566 vs RK3568 区别速记"命中:同 die、codec IP 一致

## Step 4 —— 回溯源文档校验(确保 chip-matrix 与 readme_cn 一致)
- 用 `grep` 在 `/home/pi/imx/sdk/tspi-rk3566-sdk/docs/readme_cn.md` 定位"编解码"关键词
- 用 `Read` 读 readme_cn 第 735-790 行
- 验证:
  - 解码表(第 748-760 行)RK3566/RK3568 H.265 = 4096x2304@60f ✓
  - 编码表(第 764-776 行)RK3566/RK3568 H.265 = 1920x1088@60f ✓
  - chip-matrix 与 readme_cn 一致,数据可信

## Step 5 —— 蒸馏?
- 不写。按 SKILL.md 第五节示例 2:"单数值不值得缓存"
- INDEX.md 未更新

## Step 6 —— 输出
- 写答案到 `outputs/answer.md`,中文 + 引用 readme_cn 路径与行号、chip-matrix 路径与行号

## 未读 / 未开的资料(避免无用开销)
- 未读任何 PDF(包括 `cn/Common/MPP/...pdf`)——单纯规格上限不需要
- 未读 resources-map.md(chip-matrix 已经命中)
- 未跑 `pdf_text.py`

## 工具调用清单
- Read SKILL.md
- Read chip-matrix.md
- Read distilled/INDEX.md
- Bash find + grep + Read readme_cn.md(校验源)
- Write answer.md
- Write trace.md(本文件)
