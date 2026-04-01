---
title: 中断下半部机制全景
tags: [BottomHalf, softirq, tasklet, workqueue, threaded-irq, kernel]
desc: ISR上半部的四种下半部延迟机制（softirq/tasklet/workqueue/threaded-irq）对比与选型
update: 2026-04-01

---


# 中断下半部机制全景

> [!note]
> **Ref:** [`sdk/Linux-4.9.88/include/linux/interrupt.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/interrupt.h), [`sdk/Linux-4.9.88/kernel/softirq.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/softirq.c), [`sdk/Linux-4.9.88/kernel/workqueue.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/workqueue.c)

## 1. 为什么需要下半部

硬中断 ISR（Top Half）运行在**关抢占、关本CPU中断**的上下文，受三个强约束：

1. **不可睡眠**：禁止 `kmalloc(GFP_KERNEL)`、`mutex_lock()`、I2C/SPI 读写
2. **不可调度**：不能主动让出 CPU
3. **必须极短**：占用 CPU 时间过长会导致其他中断延迟、实时性劣化

Bottom Half 模式：ISR 只做最小必要工作（读状态、清中断标志），将耗时处理推迟到更宽松的上下文执行。

---

## 2. 四种机制全景

```mermaid
graph TD
    ISR["硬中断 ISR (Top Half)\n关中断 · 关抢占 · 不可睡眠"]

    subgraph "软中断上下文（BH Context）"
        SOFTIRQ["① softirq\n开硬中断 · 关抢占\n多CPU并发 · 不可睡眠"]
        TASKLET["② tasklet\n运行于 TASKLET_SOFTIRQ\n同实例串行 · 不可睡眠"]
    end

    subgraph "进程上下文（Process Context）"
        WQ["③ workqueue\nkworker 内核线程\n可睡眠 · 可调度"]
        TIRQ["④ threaded IRQ\nirq/N 专属线程\nSCHED_FIFO · 可睡眠"]
    end

    ISR -- "raise_softirq()" --> SOFTIRQ
    ISR -- "tasklet_schedule()" --> TASKLET
    ISR -- "schedule_work()" --> WQ
    ISR -- "return IRQ_WAKE_THREAD" --> TIRQ
    SOFTIRQ -. "TASKLET_SOFTIRQ 向量承载" .-> TASKLET
```

---

## 3. 选型决策

```mermaid
graph TD
    Q1{"下半部需要\n睡眠/阻塞？"}
    Q2{"有实时响应\n要求？"}
    Q3{"同类型需要\n多CPU并发？"}

    TIRQ["threaded IRQ\n✓ SCHED_FIFO 实时优先级\n✓ 专属线程，唤醒最快"]
    WQ["workqueue\n✓ 通用，延迟稍高\n✓ 共享 kworker 线程池"]
    SOFTIRQ["softirq\n⚠ 仅子系统级使用\n网络/块设备/RCU"]
    TASKLET["tasklet\n✓ 驱动下半部首选\n✓ 串行，不需考虑并发"]

    Q1 -- "是" --> Q2
    Q2 -- "是" --> TIRQ
    Q2 -- "否" --> WQ
    Q1 -- "否" --> Q3
    Q3 -- "需要多CPU" --> SOFTIRQ
    Q3 -- "串行即可" --> TASKLET
```

---

## 4. 横向对比

| 特性 | softirq | tasklet | workqueue | threaded IRQ |
|------|:-------:|:-------:|:---------:|:------------:|
| 执行上下文 | 软中断 BH | 软中断 BH | 进程（kworker）| 进程（irq/N）|
| 可以睡眠 | ✗ | ✗ | ✓ | ✓ |
| 同类并发（多CPU）| ✓ | ✗ | ✓ | ✗（专属线程）|
| 调度策略 | — | — | SCHED_NORMAL | SCHED_FIFO prio=50 |
| 驱动直接使用 | 极少 | ✓ | ✓ | ✓（推荐）|
| 触发开销 | 最低 | 低 | 中 | 低（直接唤醒）|
| 典型场景 | 网络/块设备 | DMA完成 | I2C/SPI读取 | 传感器/触摸屏 |

---

## 5. preempt_count 上下文检测

```c
/* include/linux/preempt.h — 四个上下文判断宏 */
in_irq()              /* 正在执行硬中断 handler */
in_softirq()          /* 正在执行 softirq（含 local_bh_disable 区域）*/
in_serving_softirq()  /* 正在执行 softirq handler（精确）*/
in_interrupt()        /* 硬中断 OR 软中断（任何中断上下文）*/
in_atomic()           /* 不可睡眠：持锁/中断/preempt_disable */
```

---

## 6. 笔记导航

| 文件 | 内容 |
|------|------|
| [`01-softirq.md`](./01-softirq.md) | softirq 执行上下文、preempt_count 机制、ksoftirqd |
| [`02-tasklet.md`](./02-tasklet.md) | tasklet 串行保证、API、ISR 上/下半部拆分模板 |
| [`03-workqueue.md`](./03-workqueue.md) | workqueue 体系、CMWQ、自定义WQ、delayed_work |
| [`04-threaded-irq.md`](./04-threaded-irq.md) | 线程化IRQ内核实现、irq_thread主循环、IRQF_ONESHOT原理 |
