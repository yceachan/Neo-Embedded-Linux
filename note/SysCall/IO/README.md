---
title: SysCall/IO 笔记库索引
tags: [index, syscall, io, README]
desc: SysCall/IO 子目录的学习路径、主题反查与 demo 对应
update: 2026-04-09
---


# SysCall/IO — 索引

> [!note]
> **配套 demo:** [`prj/03-Advanced-IO`](../../../prj/03-Advanced-IO/)(本目录的 `Demo-Advanced-IO/` 是它的 symlink)
> **重构记录:** [`plan/refact-20260409.md`](./plan/refact-20260409.md)

## 学习路径(顺序读)

| # | 文件 | 主题 | 阶段 |
|---|------|------|------|
| 01 | [`01-read-write.md`](./01-read-write.md) | `read/write` 系统调用全景:user → glibc → SVC → VFS → fops | 入门 |
| 02 | [`02-ioctl.md`](./02-ioctl.md) | `ioctl` 控制命令编码 + 驱动 switch 范式 | 入门 |
| 03 | [`03-blocking-semantics.md`](./03-blocking-semantics.md) | 阻塞 / 非阻塞 IO 的双侧实现细节(`wait_event` / `O_NONBLOCK` / `EAGAIN`) | 中级 |
| 04 | [`04-io-models-overview.md`](./04-io-models-overview.md) | **五种 IO 范式地图** — 对照表 + 决策树 + 索引 | 中级 |
| 05 | [`05-poll-kernel.md`](./05-poll-kernel.md) | `poll` 内核机制单一可信源:`do_sys_poll` → `__pollwait` → `wait_queue` | 进阶 |
| 06 | [`06-multiplex-compare.md`](./06-multiplex-compare.md) | `select` / `poll` / `epoll` 横向对比 — 时序、机制、复杂度 | 进阶 |
| 07 | [`07-fasync-sigio.md`](./07-fasync-sigio.md) | 信号驱动 IO:`fasync` + `SIGIO` 三阶段时序 | 进阶 |
| 08 | [`08-drv-fops-recipes.md`](./08-drv-fops-recipes.md) | 字符驱动 fops 五件套落地模板(read/write/poll/fasync) | 实战 |
| 08-1 | [`08-1-app-IO-recipes.md`](./08-1-app-IO-recipes.md) | 用户态五种 IO 范式编程模板 + 各自时序图 | 实战 |
| 09 | [`09-epoll-et-lt-deep-dive.md`](./09-epoll-et-lt-deep-dive.md) | `epoll` ET/LT 模式深度解析、铁律与最佳实践 | 实战 |

**实录与观察类(不参与序号):**

- [`trail-strace.md`](./trail-strace.md) — 五种 IO 范式在 IMX6ULL EVB 上的 strace 实证

## 主题反查

| 想学什么 | 看哪篇 |
|---------|--------|
| `read()` 调用从用户态到驱动怎么走 | 01 |
| 怎么在驱动里实现一个控制命令 | 02 |
| 为什么 `O_NONBLOCK` 单独用没意义 | 03 §2 |
| 五种 IO 范式怎么选 | 04 §3 决策树 |
| `poll_wait` 真的会睡眠吗 | 05 §3.2 |
| 为什么 poll 是天然 LT | 05 §6 |
| select/poll/epoll 在内核里到底差在哪 | 06 §3 |
| `epoll` ET 为什么必须配 `O_NONBLOCK` | 06 §5.3, 09 §3.1 |
| `epoll` ET 模式下读不干净会怎样 | 09 §2 |
| `kill_fasync` 能在中断里调吗 | 07 §4 |
| `.release` 不清 fasync 会怎样 | 07 §7 |
| 想抄一个完整的字符驱动 | 08 §5 |
| 想看 SIGIO / AIO 在 strace 里长什么样 | trail-strace ④⑤ |

## 与 prj/03-Advanced-IO demo 的对应

| Demo 元素 | 对应笔记 |
|-----------|---------|
| `src/adv_io_fops.c` 整体结构 | 08 §5 |
| `adv_io_do_read` / `adv_io_do_write` 阻塞分支 | 03 §1 |
| `adv_poll` 实现 | 05 §5、08 §2 |
| `adv_io_fasync` + `kill_fasync` | 07 §3.2、§4 |
| `test/test_block.c` 等 5 个测试 | trail-strace ①–⑤ |
