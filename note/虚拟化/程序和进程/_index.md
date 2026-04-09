---
title: 程序和进程 — 章节索引
tags: [Process, Index]
desc: 进程子系统学习脉络：从起源到 task_struct、生命周期、再到组与终端控制。
update: 2026-04-08
---


# 程序和进程

| # | 文件 | 主题 |
|---|------|------|
| 00 | [进程的起源](00-进程的起源-Init与进程树.md) | 0/1 号进程、进程树 |
| 01 | [task_struct 解剖](01-进程控制块-task_struct.md) | PCB 字段全景 |
| 02 | [进程状态管理](02-进程状态管理.md) | TASK_RUNNING/INTERRUPTIBLE/zombie |
| 03 | [fork 与 COW](03-进程创建-fork-COW.md) | 写时拷贝原理（实现见 SysCall/进程API/01） |
| 04 | [fork-exec-wait](04-进程生命周期-fork-exec-wait.md) | 生命周期 API 三件套 |
| 05 | [进程组与会话](05-控制的边界-进程组与会话.md) | PGID/SID、作业控制 |
| 06 | [TTY 与 PTS](06-终端-TTY与PTS.md) | 终端设备模型 |

## 邻接主题

- 地址空间与 MMU → [`../进程地址空间/`](../进程地址空间/_index.md)
- 系统调用接口与 demo → [`../../SysCall/进程API/`](../../SysCall/进程API/)
- 调度器 → [`../../kernel/sched/`](../../kernel/sched/)
