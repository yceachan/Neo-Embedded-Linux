---
title: Distilled 文件写作模板
tags: [rk, skill, reference, workflow]
desc: distilled/<topic>.md 的统一模板与节选示例,保证未来蒸馏一致、可被快速复用
update: 2026-05-27
---


# Distilled 文件写作模板

> 这份模板服务的是 [[SKILL]] Step 5 的产物。每次 cache miss 写一份新的 distilled,务必按这个骨架走——结构稳定,以后 grep/查表才省事。


## 一、命名

文件名:`<模块|主题>-<芯片|generic>.md`,小写,kebab-case。

正例:

- `pinctrl-generic.md`(框架通用)
- `gmac-rk356x.md`(特定芯片细节)
- `rknn-quickstart.md`(workflow 类)
- `mipi-dsi-panel-adapt.md`(具体场景)

反例:

- `pinctrl.md`(没说哪个芯片)
- `RK3566_GMAC_RGMII_delayline_设置.md`(过长,不是 kebab)


## 二、YAML frontmatter

第一行直接是 `---`,不要留空行/BOM/空格(全局约定)。

```yaml
---
title: <一句话主题,名词不放论点>
tags: [rk, distilled, <模块>, <芯片若适用>]
desc: <一句话价值描述,15-30 字>
source: <PDF 相对路径,多个用列表>
source_pages: <用到的页码区间,如 "8-12,20">
update: YYYY-MM-DD
---
```


## 三、正文骨架

```markdown
# <H1 与 title 一致>

> [!note]
> **Ref:**
> - PDF: `<sdk/tspi-rk3566-sdk/docs/cn/...>` p.X-Y
> - (可选)源码: `kernel-6.1/drivers/...`

## 背景

1-3 段。回答"这个东西为什么存在 / 解决什么问题"。不要复述 datasheet,要写一个读者从未听说过这个模块时需要的最低限度上下文。

## 关键配置 / API

按场景列出精确名字。DTS 节点、sysfs 路径、kbuild 配置、用户态命令都用 code block 给原样片段。

```dts
&gmac1 {
    phy-mode = "rgmii";
    tx_delay = <0x44>;
    rx_delay = <0x4f>;
    status = "okay";
};
```

## 易踩坑

列 3-5 条。每条一句话,后面括号注 PDF 页码或 commit。

- IO 电源域不配,GPIO 默认 1.8V,接 3.3V 外设会烧片(p.6)。
- pinctrl-0 写错引用顺序会让模块默默走 default,日志没错(p.10)。
- ...

## 验证方法

如果适用,给出"怎么验证我配对了"的 1-2 个命令或日志关键词。

\`\`\`bash
cat /sys/kernel/debug/pinctrl/pinctrl-rockchip-pinctrl/pinconf-pins | grep gpio4
\`\`\`

## 相关

- [[<其他 distilled 的 slug>]]
- [[chip-matrix]]
```


## 四、节选示例(精简版)

```markdown
---
title: RK3566 GMAC RGMII delayline 调试
tags: [rk, distilled, gmac, rk3566]
desc: RK3566/3568 GMAC RGMII 时序漂移的 tx-delay/rx-delay 取值与示波器验证流程
source: cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf
source_pages: 1-18
update: 2026-05-27
---

# RK3566 GMAC RGMII delayline 调试

> [!note]
> **Ref:** PDF `cn/Common/GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf` p.1-18

## 背景

RGMII 在千兆模式下 TX/RX 时钟与数据要求 ~2ns 的窗口对齐。PCB 走线长度不一会引入时序偏差,Rockchip 在 GMAC 控制器侧提供可调的 `tx_delay`/`rx_delay` 寄存器(7-bit,每 step ~150ps),让软件不改板也能补偿。

## 关键配置

DTS 在 `&gmac1` 节点配置:

\`\`\`dts
&gmac1 {
    phy-mode = "rgmii";
    tx_delay = <0x44>;  /* 默认起点,需扫描 */
    rx_delay = <0x4f>;
    snps,reset-gpio = <&gpio4 RK_PB6 GPIO_ACTIVE_LOW>;
    status = "okay";
};
\`\`\`

## 易踩坑

- ...
```


## 五、何时**不**写蒸馏

- 单数值/单条事实(如 "RK3588 H265 最大 7680x4320")——直接查 [[chip-matrix]] 答完就行。
- 用户问的问题在源 PDF 里答案只有一段,且本身只用一次——蒸馏开销大于节省。
- 高度项目特定的现象(如"我板子今天点不亮 MIPI 屏"——是诊断不是知识)。

判据:**知识能被未来 ≥3 次复用,且需多步推导**,才值得蒸馏。
