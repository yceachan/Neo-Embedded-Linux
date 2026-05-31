# Trace — eval-2 GMAC RGMII delayline (with_skill)

## 工作流执行

按 `SKILL.md` 七步走，全程未跳步。

### Step 1: Triage

- SoC: **RK3566**（用户明确）
- 模块: **GMAC / RGMII / delayline**
- 意图: **DTS 配置 + 示波器/软件验证**

### Step 2: 查 distilled 缓存

- 读 `/home/pi/imx/.claude/skills/rk/references/distilled/INDEX.md`
- 索引中只有 `usb-gadget-uac-rk356x.md`，与 GMAC 无关 → **cache miss**

### Step 3: 定位源 PDF（resources-map）

`grep` 命中 3 行（resources-map.md L155-159）：
- `GMAC/Rockchip_Developer_Guide_Linux_GMAC_CN.pdf` —— GMAC 总览（本题用不到）
- `GMAC/Rockchip_Developer_Guide_Linux_GMAC_Mode_Configuration_CN.pdf` —— 模式配置（要用，拿 DTS 模板）
- `GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf` —— 主目标

完整路径前缀 `sdk/tspi-rk3566-sdk/docs/cn/Common/GMAC/`（见 resources-map L48）。

### Step 4: 用 pdf_text.py 抽片段

| 命令 | 用途 | 结果 |
|------|------|------|
| `pdf_text.py .../GMAC_RGMII_Delayline_CN.pdf --outline` | 看 PDF 大纲 | 9 节，全文仅 9 页 |
| `pdf_text.py .../GMAC_RGMII_Delayline_CN.pdf --pages 1-10` | 全文（小文档可一次性抽） | 拿到 sysfs 节点、扫描命令、`rgmii-rxid` 模式、示波器判据、FAQ |
| `pdf_text.py .../GMAC_Mode_Configuration_CN.pdf --outline` | 找 RK356x 章节 | RK3568 在 p.34-49 |
| `pdf_text.py .../GMAC_Mode_Configuration_CN.pdf --pages 38-48` | 取 RK3568 RGMII DTS 模板 | 拿到 gmac1m0/gmac1m1 完整 DTS（RK3566 共用） |

未使用 `--grep`（大纲清晰，按章节直取页码即可）。

### Step 5: 写蒸馏文件

新建 `/home/pi/imx/.claude/skills/rk/references/distilled/gmac-rgmii-delayline.md`

按 workflow-distill 模板：YAML frontmatter、Ref note、背景、关键配置（sysfs + DTS + Kconfig）、易踩坑 6 条、验证方法（软件层 + 示波器层）、相关。

source_pages 标注：`delayline:1-10; mode-config:38-45`。

### Step 6: 登记到 INDEX

往 `/home/pi/imx/.claude/skills/rk/references/distilled/INDEX.md` 追加一行：

```
- [RK356x GMAC RGMII delayline 调试](gmac-rgmii-delayline.md) — GMAC/RK3566+RK3568 — RGMII tx_delay/rx_delay 扫描流程、DTS 片段、ping/iperf 与示波器验证
```

### Step 7: 中文回答

写到 `outputs/answer.md`：结论先行 + 调参 5 步流程 + DTS 完整示例（含 RTL8211F `rgmii-rxid` 变体）+ 软件层验证（ping/iperf/ethtool -S）+ 示波器层验证（带宽、测点、占空比、相位差）+ 硬件级补救。引用列了两份 PDF 的具体页码区间 + 源码路径 + 新建 distilled 文件名。

## 关键发现 / 决策

- delayline 调试 PDF 很短（9 页），直接全文抽更高效；如果是 100+ 页的 PDF 才需要逐节抽。
- RK3566 / RK3568 / RK356x 在 GMAC 部分文档共用一份 mode-config（PDF 内部章节是 "RK3568"，但适用 RK3566）。蒸馏文件标签里同时写 `rk3566` 和 `rk3568`，方便未来 grep。
- 关键易踩坑：PHY 带内部 RX delay（RTL8211F 默认行为）时 SoC 必须用 `rgmii-rxid` + 注释 `rx_delay`，否则双重补偿；PDF p.7 明确写了。
- 用户问的"大包丢包"对应的判定命令是 `ping -M do -s 1472`（1500 MTU 减 28 字节 IP/ICMP header，不分片），这是 RGMII 时序窗口偏移最典型的现象，在回答里强调了。

## 读过的文件

- `/home/pi/imx/.claude/skills/rk/SKILL.md`
- `/home/pi/imx/.claude/skills/rk/references/resources-map.md`（grep）
- `/home/pi/imx/.claude/skills/rk/references/workflow-distill.md`
- `/home/pi/imx/.claude/skills/rk/references/distilled/INDEX.md`
- `/home/pi/imx/sdk/tspi-rk3566-sdk/docs/cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf` (outline + pages 1-10)
- `/home/pi/imx/sdk/tspi-rk3566-sdk/docs/cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_Mode_Configuration_CN.pdf` (outline + pages 34-48)

## 写过的文件

- `/home/pi/imx/.claude/skills/rk/references/distilled/gmac-rgmii-delayline.md`（新建蒸馏）
- `/home/pi/imx/.claude/skills/rk/references/distilled/INDEX.md`（追加一行）
- `/home/pi/imx/.claude/skills/rk-workspace/iteration-1/eval-2-gmac-rgmii-delayline/with_skill/outputs/answer.md`
- `/home/pi/imx/.claude/skills/rk-workspace/iteration-1/eval-2-gmac-rgmii-delayline/with_skill/outputs/trace.md`（本文件）
