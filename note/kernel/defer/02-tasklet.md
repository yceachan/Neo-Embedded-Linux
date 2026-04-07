---
title: tasklet：驱动下半部串行执行
tags: [tasklet, BottomHalf, softirq, driver, kernel]
desc: tasklet数据结构、串行保证机制、API与ISR上下半部拆分驱动模板
update: 2026-04-01

---


# tasklet：驱动下半部串行执行

> [!note]
> **Ref:** [`sdk/Linux-4.9.88/include/linux/interrupt.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/interrupt.h), [`sdk/Linux-4.9.88/kernel/softirq.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/softirq.c)

## 1. 定位

tasklet 是**基于 softirq 的轻量封装**，运行在 `TASKLET_SOFTIRQ`（普通）或 `HI_SOFTIRQ`（高优先级）向量上。相比裸 softirq：

| | softirq | tasklet |
|--|---------|---------|
| 同类多CPU并发 | ✓ | ✗（同一实例串行）|
| 驱动直接使用 | 极少 | ✓（首选）|
| 并发安全 | handler 需自行加锁 | 内核保证串行，简化驱动 |

---

## 2. 数据结构

```c
/* include/linux/interrupt.h */
struct tasklet_struct {
    struct tasklet_struct *next;   /* 挂入 per-CPU 调度链表 */
    unsigned long          state;  /* TASKLET_STATE_SCHED | TASKLET_STATE_RUN */
    atomic_t               count;  /* 禁用计数：0=使能，>0=禁用 */
    void                 (*func)(unsigned long);
    unsigned long          data;   /* 传给 func 的参数 */
};
```

### 串行保证机制

```c
/* kernel/softirq.c: tasklet_action() 简化 */
static void tasklet_action(struct softirq_action *a)
{
    struct tasklet_struct *t = __this_cpu_read(tasklet_vec.head);

    while (t) {
        /* 尝试设置 TASKLET_STATE_RUN 标志（原子 test_and_set）*/
        if (tasklet_trylock(t)) {
            /* 成功：当前CPU独占执行此 tasklet */
            if (atomic_read(&t->count) == 0) {
                t->func(t->data);   /* 执行回调 */
            }
            tasklet_unlock(t);      /* 清除 RUN 标志 */
        } else {
            /* 失败：另一个CPU正在执行此 tasklet，跳过，下轮再试 */
        }
        t = t->next;
    }
}
```

> `TASKLET_STATE_RUN` 是 `test_and_set_bit` 原子操作，保证同一 `tasklet_struct` 实例在任意时刻只在一个 CPU 上运行。不同 tasklet 实例可以并发。

---

## 3. API

```c
/* 静态声明 */
DECLARE_TASKLET(name, func, data);            /* 使能状态 */
DECLARE_TASKLET_DISABLED(name, func, data);   /* 禁用状态 */

/* 动态初始化 */
tasklet_init(&t, func, data);

/* 调度 */
tasklet_schedule(&t);       /* 普通优先级（TASKLET_SOFTIRQ）*/
tasklet_hi_schedule(&t);    /* 高优先级（HI_SOFTIRQ）*/

/* 禁用/使能 */
tasklet_disable(&t);        /* count++，等待正在执行的完成后返回 */
tasklet_disable_nosync(&t); /* count++，不等待 */
tasklet_enable(&t);         /* count-- */

/* 销毁（driver remove 时必须调用）*/
tasklet_kill(&t);           /* 等待执行完毕 + 从调度链表移除 */
                            /* ⚠ 不能在中断上下文调用 */
```

---

## 4. 驱动模板：ISR 上/下半部拆分

```c
struct my_dev {
    void __iomem          *base;
    struct tasklet_struct  dma_tasklet;
    u32                    dma_status;
    wait_queue_head_t      read_wq;
};

/* ---- 下半部：tasklet 回调（软中断上下文，不可睡眠）---- */
static void dma_complete_bh(unsigned long data)
{
    struct my_dev *dev = (struct my_dev *)data;
    process_dma_result(dev);
    wake_up_interruptible(&dev->read_wq);
}

/* ---- 上半部：ISR（关中断，极短）---- */
static irqreturn_t dma_isr(int irq, void *dev_id)
{
    struct my_dev *dev = dev_id;
    dev->dma_status = readl(dev->base + DMA_STATUS);
    writel(DMA_IRQ_CLEAR, dev->base + DMA_STATUS);
    tasklet_schedule(&dev->dma_tasklet);
    return IRQ_HANDLED;
}

/* ---- probe ---- */
tasklet_init(&dev->dma_tasklet, dma_complete_bh, (unsigned long)dev);
request_irq(dev->irq, dma_isr, 0, "my-dma", dev);

/* ---- remove ---- */
free_irq(dev->irq, dev);
tasklet_kill(&dev->dma_tasklet);   /* 必须在 free_irq 之后 */
```
