# Trace — eval-3 RK356x USB Gadget UAC

## 1. SKILL 流程执行

- **Step 1 Triage**: SoC=RK3568 (同 SDK 适用 RK3566); 模块=USB-Gadget/UAC; 意图=方案咨询 + 大致流程 + PDF 清单。
- **Step 2 查 distilled cache**: 读 `references/distilled/INDEX.md` → 初始为空 → **miss**。
- **Step 3 查 resources-map**: 在 `references/resources-map.md` §二.USB 命中三条候选:
  - `cn/Common/USB/Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf` (主)
  - `cn/Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf` (一键脚本)
  - `cn/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf` (平台 OTG mode 切换)
- **Step 4 用 pdf_text.py 抽片段**: 见下方"PDF 操作"。
- **Step 5 写 distilled**: 新建 `references/distilled/usb-gadget-uac-rk356x.md`。
- **Step 6 登记 INDEX**: 已追加一行到 `distilled/INDEX.md`。
- **Step 7 回答用户**: `answer.md` 中文,带 PDF 路径与页码。

## 2. 读过的参考文件

- `/home/pi/imx/.claude/skills/rk/SKILL.md` (完整读)
- `/home/pi/imx/.claude/skills/rk/references/distilled/INDEX.md`
- `/home/pi/imx/.claude/skills/rk/references/resources-map.md` (完整读)
- `/home/pi/imx/.claude/skills/rk/references/workflow-distill.md` (模板)

## 3. PDF 操作 (pdf_text.py)

| PDF | 命令 | 用途 |
|-----|------|------|
| `cn/Common/USB/Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf` | `--outline` | 看章节结构 |
| 同上 | `--pages 1-9` | 概述 + UAC1/UAC2 比较 + CONFIG + configfs 脚本头 |
| 同上 | `--pages 9-18` | configfs 完整脚本 + Parameters + Uevent + Compatibility + Test |
| `cn/Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf` | `--outline` | 看章节结构 |
| 同上 | `--pages 4-11` | S50usbdevice 流程 + UAC 一键脚本 + ALSA 回环测试 |
| `cn/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf` | `--outline` | 看章节结构 |
| 同上 | `--pages 31-32` | OTG mode 强制切换命令(Legacy + Linux-5.10 新接口) + Type-C 支持列表 |

未读整本 PDF;每次都用 `--pages` 抽段 + `=== page N ===` 分隔保留页码。

## 4. 产出

- 蒸馏: `/home/pi/imx/.claude/skills/rk/references/distilled/usb-gadget-uac-rk356x.md`
- INDEX 已更新: `/home/pi/imx/.claude/skills/rk/references/distilled/INDEX.md`
- 答案: `/home/pi/imx/.claude/skills/rk-workspace/iteration-1/eval-3-rk356x-usb-gadget-uac/with_skill/outputs/answer.md`
- 本 trace: `/home/pi/imx/.claude/skills/rk-workspace/iteration-1/eval-3-rk356x-usb-gadget-uac/with_skill/outputs/trace.md`

## 5. 备注

- 没用训练知识替代文档;所有 configfs 节点名、CONFIG 名、sysfs 路径、dmesg 关键字都从 PDF 抽出。
- 没动 `note/` 或 `prj/`;只在 skill 私有缓存里建蒸馏。
- INDEX.md 在本次写入后被(看起来是 linter 或并行任务)又追加了一条 `gmac-rgmii-delayline` 条目——保留,未回滚。
