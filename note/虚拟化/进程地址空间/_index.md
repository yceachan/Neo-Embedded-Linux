---
title: 进程地址空间 — 章节索引
tags: [VirtualMemory, MMU, Index]
desc: 虚拟地址空间原理线：布局全景 → vDSO → 页表/MMU → 透明性与软件成本。
update: 2026-04-08
---


# 进程地址空间

| # | 文件 | 主题 |
|---|------|------|
| 00 | [地址空间全景](00-进程地址空间.md) | demo 实测：分类 VMA + 反查种子地址 + W^X + ASLR |
| 01 | [vDSO 与 vvar](01-vDSO与vvar.md) | 零切换系统调用机制 |
| 02 | [页表与 MMU](02-进程地址空间-页表与MMU.md) | VA→PA 翻译、ARMv7/v8 层级 |
| 03 | [虚实之间](03-虚实之间：MMU的无感与负担.md) | MMU 透明度与隐性成本 |

## demo

[`demo/`](./demo/README.md) — 自种子靶子 + 分类型分析器，量化 vDSO 加速比 16×。

## 已迁出（工具链类）

原 04–07 篇 GDB / readelf / 反汇编 / 观测工具已归并入 [`../../devp/debug/`](../../devp/debug/_index.md)。

## 邻接主题

- 进程结构与生命周期 → [`../程序和进程/`](../程序和进程/_index.md)
- 系统调用与 MMU 协作 → [`../../SysCall/`](../../SysCall/)
