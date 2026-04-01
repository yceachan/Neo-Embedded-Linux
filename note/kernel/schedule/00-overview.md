---
title: 内核延迟机制与同步原语全景
tags: [kernel, wait_queue, workqueue, softirq, tasklet, timer, spinlock, mutex, schedule]
desc: 内核延迟执行机制（timer/softirq/tasklet/workqueue）与同步原语（spinlock/mutex）的体系总览与选型决策
update: 2026-04-01

---


# 内核延迟机制与同步原语全景

> [!note]
> **Ref:** [`note/虚拟化/进程通信IPC/semaphore/04-kernel-sync-primitives.md`](../../虚拟化/进程通信IPC/semaphore/04-kernel-sync-primitives.md), [`note/Subsystem/Interrupt/02-kernel-framework.md`](../../Subsystem/Interrupt/02-kernel-framework.md), `sdk/Linux-4.9.88/include/linux/`

## 1. 执行上下文分类

Linux 内核代码运行在 3 种上下文，每种有不同的能力约束：

```mermaid
graph TD
    subgraph "进程上下文 (Process Context)"
        PC["系统调用 / kthread\n• 可以睡眠\n• 可以调度\n• 可以访问用户空间"]
    end
    subgraph "软中断上下文 (Softirq/BH Context)"
        SC["softirq / tasklet\n• 硬件中断开启\n• 不能睡眠\n• 不能调度"]
    end
    subgraph "硬中断上下文 (Hardirq Context)"
        HC["ISR (request_irq handler)\n• 所有中断关闭（该CPU）\n• 不能睡眠\n• 极短，必须快速返回"]
    end

    HC -- "raise_softirq()" --> SC
    SC -- "schedule_work()" --> PC
```

---

## 2. 延迟执行机制选型

驱动中断处理需要延迟/异步执行时的机制选择：

```mermaid
graph TD
    START["需要延迟执行工作？"]
    Q1{"执行时需要<br>睡眠/阻塞？"}
    Q2{"需要精确<br>定时触发？"}
    Q3{"同一类型<br>需要并发？"}

    WQ["workqueue\n✓ 可睡眠\n✓ I2C/SPI读写\n✓ kmalloc(GFP_KERNEL)"]
    TIMER["timer_list\n✓ 定时回调\n✗ 不可睡眠\n典型：防抖、超时检测"]
    SOFTIRQ["softirq\n✓ 多CPU并发\n✗ 需自定义\n仅网络/块设备等子系统用"]
    TASKLET["tasklet\n✓ 串行执行\n✓ 驱动首选下半部\n✗ 不可睡眠"]

    START --> Q1
    Q1 -- "是" --> WQ
    Q1 -- "否" --> Q2
    Q2 -- "是" --> TIMER
    Q2 -- "否" --> Q3
    Q3 -- "需要多CPU并发" --> SOFTIRQ
    Q3 -- "串行即可" --> TASKLET
```

---

## 3. 同步原语选型

```mermaid
graph TD
    SQ["需要同步/互斥？"]
    Q1{"持锁期间<br>可能睡眠？"}
    Q2{"读多写少？"}
    Q3{"等待一次性<br>事件完成？"}
    Q4{"读多写少？"}

    MUTEX["mutex\n✓ 进程上下文推荐\n✗ 不能用于中断"]
    RWSEM["rw_semaphore\n✓ 大量并发读\n读者可同时持有"]
    COMP["completion\n✓ 等待另一线程完成\n等待初始化、IO完成"]
    SPINLOCK["spinlock_t\n✓ 任意上下文\n✗ 持锁期间不能睡眠"]
    SEQLOCK["seqlock\n✓ 读者无需锁\n✓ 读者无限制并发"]

    SQ --> Q1
    Q1 -- "是" --> Q3
    Q3 -- "是" --> COMP
    Q3 -- "否" --> Q2
    Q2 -- "是" --> RWSEM
    Q2 -- "否" --> MUTEX
    Q1 -- "否（中断/原子上下文）" --> Q4
    Q4 -- "是" --> SEQLOCK
    Q4 -- "否" --> SPINLOCK
```

**上下文与原语约束速查：**

| 原语 | 硬中断 ISR | 软中断/tasklet | 进程上下文 |
|------|:---:|:---:|:---:|
| `spinlock_t` | ✓ (需 `_irq` 变体) | ✓ (需 `_bh` 变体) | ✓ |
| `mutex` | ✗ | ✗ | ✓ |
| `semaphore` | ✗ | ✗ | ✓ |
| `completion` | ✗ (wait侧) | ✗ (wait侧) | ✓ |
| `atomic_t` | ✓ | ✓ | ✓ |

---

## 4. wait_queue — 睡眠/唤醒核心机制

`wait_queue` 是 mutex、semaphore、IO 等待的**底层实现基础**，驱动中也可直接使用。

```c
/* 典型模式：生产者-消费者 */
wait_queue_head_t wq;
int data_ready = 0;

// 消费者（进程上下文）：等待数据
wait_event_interruptible(wq, data_ready != 0);

// 生产者（中断 ISR）：产生数据后唤醒
data_ready = 1;
wake_up_interruptible(&wq);
```

详见 → [`01-wait-queue.md`](./01-wait-queue.md)

---

## 5. 笔记导航

| 文件 | 内容 |
|------|------|
| [`01-wait-queue.md`](./01-wait-queue.md) | wait_queue 睡眠/唤醒机制，condition 语义，独占唤醒 |
| [`02-softtimer.md`](./02-softtimer.md) | timer_list 定时器、softirq、tasklet 三级底半部 |
| [`03-workqueue.md`](./03-workqueue.md) | 工作队列：system WQ、自定义 WQ、delayed work |
| [`../../虚拟化/进程通信IPC/semaphore/04-kernel-sync-primitives.md`](../../虚拟化/进程通信IPC/semaphore/04-kernel-sync-primitives.md) | 同步原语全览：mutex/spinlock/rwsem/completion/seqlock |
