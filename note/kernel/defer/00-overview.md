---
title: 内核延迟与异步执行机制全景
tags: [defer, softirq, tasklet, workqueue, threaded-irq, wait_queue, completion, kthread, timer]
desc: 内核中所有“推迟执行 / 异步等待 / 线程化执行”机制的统一全景与选型决策树
update: 2026-04-07

---


# 内核延迟与异步执行机制全景

> [!note]
> **Ref:** [`include/linux/interrupt.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/interrupt.h), [`kernel/softirq.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/softirq.c), [`kernel/workqueue.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/workqueue.c), [`include/linux/wait.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/wait.h), [`include/linux/completion.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/completion.h), [`include/linux/kthread.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/kthread.h)

## 1. 目录定位

`note/kernel/defer/` 收录所有与“**不立即做、换个上下文做、等条件满足再做**”相关的机制。这些机制彼此配合，构成了驱动开发中最常用的异步骨架：

- **Botton Half**：softirq / tasklet / workqueue / threaded-irq —— 把 ISR 的重活推迟到更宽松的上下文
- **同步等待族**：wait_queue / completion —— 让进程睡眠直到条件满足或事件发生
- **线程化执行族**：kthread —— 独立内核线程承载长期后台任务
- **定时延迟族**：timer_list（低精度）/ hrtimer（高精度）—— 见 [`../time/02-soft-timer.md`](../time/02-soft-timer.md)、[`../time/03-hrtimer.md`](../time/03-hrtimer.md)

所有机制本质上都是在回答：**“这段代码应该在什么上下文、什么时机被执行？”**

---

## 2. 选型决策树（核心）

```mermaid
flowchart TD
    START(["需要延后/异步执行一段代码"]) --> Q1{"会不会睡眠？\n(mutex/GFP_KERNEL/I2C/SPI)"}

    Q1 -- "否（纯计算/寄存器）" --> Q2{"是否由\n硬中断触发？"}
    Q1 -- "是" --> Q3{"是否由\n硬中断触发？"}

    Q2 -- "是" --> Q2a{"同类处理需要\n多CPU并发？"}
    Q2a -- "需要（网络/块设备）" --> SOFTIRQ["softirq\n子系统级预分配向量"]
    Q2a -- "串行即可" --> TASKLET["tasklet\n驱动下半部经典选择"]
    Q2 -- "否（周期性/一次性延时）" --> Q2b{"精度要求？"}
    Q2b -- "ms 级 tick 精度" --> TIMER["timer_list\n见 time/02"]
    Q2b -- "us/ns 级" --> HRTIMER["hrtimer\n见 time/03"]

    Q3 -- "是" --> Q3a{"需要实时优先级\n/专属线程？"}
    Q3a -- "是" --> TIRQ["threaded IRQ\nirq/N 线程, SCHED_FIFO"]
    Q3a -- "否" --> WQ["workqueue\nkworker 共享线程池"]
    Q3 -- "否（来自进程/其它线程）" --> Q3b{"是等待\n某个条件？"}

    Q3b -- "等条件为真\n(缓冲区非空)" --> WAITQ["wait_queue\nwait_event + wake_up"]
    Q3b -- "等一次性事件\n(DMA完成/探测结束)" --> COMPL["completion\nwait_for_completion"]
    Q3b -- "长期后台循环\n(轮询/看门狗)" --> KTH["kthread\nkthread_run + 主循环"]

    classDef atom fill:#ffe4b5,stroke:#333,color:#000
    classDef sleep fill:#d4f1d4,stroke:#333,color:#000
    classDef timer fill:#e0e0ff,stroke:#333,color:#000
    class SOFTIRQ,TASKLET atom
    class WQ,TIRQ,WAITQ,COMPL,KTH sleep
    class TIMER,HRTIMER timer
```

> **读图提示**：橙色 = 原子上下文机制（不可睡眠）；绿色 = 进程上下文机制（可睡眠）；蓝色 = 时间触发机制。

---

## 3. 机制全家福

```mermaid
graph LR
    subgraph HARD["硬中断上下文 (Top Half)"]
        ISR["ISR handler\n关中断 · 原子"]
    end

    subgraph SOFT["软中断上下文 (BH)"]
        SIRQ["softirq"]
        TL["tasklet\n(TASKLET_SOFTIRQ)"]
        TIMERBH["timer softirq\n(TIMER_SOFTIRQ)"]
    end

    subgraph PROC["进程上下文 (可睡眠)"]
        KWORKER["kworker 线程\n(workqueue)"]
        IRQTHR["irq/N 线程\n(threaded IRQ)"]
        KTHR["自定义 kthread"]
        WAITER["睡眠在 wait_queue\n/ completion 的进程"]
    end

    ISR -- "raise_softirq" --> SIRQ
    ISR -- "tasklet_schedule" --> TL
    ISR -- "queue_work" --> KWORKER
    ISR -- "IRQ_WAKE_THREAD" --> IRQTHR
    ISR -- "wake_up / complete" --> WAITER
    SIRQ -. "承载" .-> TL
    SIRQ -. "承载" .-> TIMERBH
    TIMERBH -- "到期回调" --> KWORKER
```

---

## 4. 横向对比速查

| 机制 | 上下文 | 可睡眠 | 触发者 | 调度策略 | 典型用途 |
|------|:------:|:------:|--------|----------|----------|
| **softirq** | 软中断 BH | ✗ | 子系统 | — | 网络 NET_RX、块设备、RCU |
| **tasklet** | 软中断 BH | ✗ | ISR | — | DMA 完成、驱动下半部 |
| **workqueue** | 进程 (kworker) | ✓ | 任何 | SCHED_NORMAL | I2C/SPI 读取、复杂处理 |
| **threaded IRQ** | 进程 (irq/N) | ✓ | 硬中断返回 | SCHED_FIFO=50 | 传感器、触摸屏 |
| **wait_queue** | 进程 | ✓ | `wake_up()` | — | read/write 阻塞、条件等待 |
| **completion** | 进程 | ✓ | `complete()` | — | 一次性事件：DMA 完成、探测同步 |
| **kthread** | 进程 | ✓ | 显式 `kthread_run` | 可设 | 看门狗、后台轮询、USB khubd |
| **timer_list** | 软中断 BH | ✗ | tick | — | ms 级延时、超时 |
| **hrtimer** | 软中断/硬中断 | ✗ | clockevent | — | us/ns 级精确延时 |

---

## 5. preempt_count 上下文检测

```c
/* include/linux/preempt.h */
in_irq()              /* 硬中断 handler 中 */
in_softirq()          /* softirq 中 或 local_bh_disable 区域 */
in_serving_softirq()  /* 精确：正在跑 softirq handler */
in_interrupt()        /* 硬 or 软中断（任何中断上下文） */
in_atomic()           /* 持锁/中断/preempt_disable —— 禁止睡眠 */
```

详见 [`../context/00-overview.md`](../context/00-overview.md)。

---

## 6. 笔记导航

| 文件 | 内容 |
|------|------|
| [`01-softirq.md`](./01-softirq.md) | softirq 向量、执行点、ksoftirqd |
| [`02-tasklet.md`](./02-tasklet.md) | tasklet 串行语义、上/下半部拆分模板 |
| [`03-workqueue.md`](./03-workqueue.md) | CMWQ、delayed_work、自定义 WQ |
| [`04-threaded-irq.md`](./04-threaded-irq.md) | `request_threaded_irq`、irq_thread 主循环、IRQF_ONESHOT |
| [`05-wait-queue.md`](./05-wait-queue.md) | `wait_event` / `wake_up` 实现与条件等待模板 |
| [`06-completion.md`](./06-completion.md) | `completion` 的 one-shot 同步语义 |
| [`07-kthread-pattern.md`](./07-kthread-pattern.md) | kthread 启动/退出模式、停止协议 |
| 交叉引用 | [`../time/02-soft-timer.md`](../time/02-soft-timer.md)、[`../time/03-hrtimer.md`](../time/03-hrtimer.md) |
