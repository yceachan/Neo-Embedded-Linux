---
title: timer_list：内核软定时器
tags: [kernel, timer_list, jiffies, HZ]
desc: 内核定时器timer_list的数据结构、jiffies时间换算、API与GPIO按键防抖模板
update: 2026-04-01

---


# timer_list：内核软定时器

> [!note]
> **Ref:** [`sdk/Linux-4.9.88/include/linux/timer.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/timer.h), [`sdk/Linux-4.9.88/kernel/time/timer.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/time/timer.c)
> **底半部机制（softirq / tasklet / workqueue / threaded-irq）已移至** → [`../BottomHalf/`](../BottomHalf/00-overview.md)

## 1. 数据结构

```c
/* include/linux/timer.h */
struct timer_list {
    struct hlist_node  entry;        /* 挂入时间轮的链表节点 */
    unsigned long      expires;      /* 触发时刻（单位：jiffies）*/
    void             (*function)(unsigned long);  /* 回调函数 */
    unsigned long      data;         /* 传给 function 的参数 */
    u32                flags;
};
```

> **timer 运行在软中断上下文（`TIMER_SOFTIRQ`）**，回调函数不可睡眠。

---

## 2. 时间单位换算

```c
HZ                          /* 每秒 jiffies 数（CONFIG_HZ，IMX6ULL 通常 100）*/
jiffies                     /* 当前时刻（单调递增，溢出安全）*/

/* 常用换算宏 */
msecs_to_jiffies(ms)        /* 毫秒 → jiffies */
usecs_to_jiffies(us)        /* 微秒 → jiffies */
jiffies_to_msecs(j)         /* jiffies → 毫秒 */

/* 时间比较（处理 jiffies 溢出）*/
time_before(a, b)           /* a < b */
time_after(a, b)            /* a > b */
time_after_eq(a, b)         /* a >= b */
```

---

## 3. API

```c
struct timer_list my_timer;

/* 初始化（Linux 4.x）*/
setup_timer(&my_timer, my_callback, (unsigned long)dev);
/* 或等价写法 */
init_timer(&my_timer);
my_timer.function = my_callback;
my_timer.data     = (unsigned long)dev;

/* 调度：N 毫秒后触发 */
my_timer.expires = jiffies + msecs_to_jiffies(200);
add_timer(&my_timer);

/* 修改到期时间（无论是否已调度）*/
mod_timer(&my_timer, jiffies + msecs_to_jiffies(200));

/* 检查是否已调度 */
if (timer_pending(&my_timer)) { ... }

/* 取消（有竞争风险：回调可能正在其他CPU运行）*/
del_timer(&my_timer);

/* 取消并等待回调执行完毕（安全取消，不能在中断上下文调用）*/
del_timer_sync(&my_timer);
```

---

## 4. 驱动典型：GPIO 按键防抖

```c
/* 中断上半部：不直接处理，只重置定时器 */
static irqreturn_t key_isr(int irq, void *dev_id)
{
    struct gpio_desc *g = dev_id;
    /* mod_timer 在定时器已调度时直接修改到期时间
     * 连续抖动时反复刷新，只有最后一次会真正触发 */
    mod_timer(&g->key_timer, jiffies + msecs_to_jiffies(20));
    return IRQ_HANDLED;
}

/* 定时器回调（软中断上下文，不可睡眠）*/
static void key_timer_cb(unsigned long data)
{
    struct gpio_desc *g = (struct gpio_desc *)data;
    int val = gpio_get_value(g->gpio);
    printk("key %d stable: %d\n", g->gpio, val);
}

/* probe 中初始化 */
setup_timer(&g->key_timer, key_timer_cb, (unsigned long)g);
request_irq(g->irq, key_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
            "key", g);
```

---

## 5. delayed_work 对比

```c
/* timer_list 防抖：回调在软中断上下文，不可睡眠 */
mod_timer(&dev->timer, jiffies + msecs_to_jiffies(20));

/* delayed_work 防抖：回调在进程上下文，可以 I2C 读取 */
mod_delayed_work(system_wq, &dev->debounce_work, msecs_to_jiffies(20));
```

需要睡眠操作时使用 `delayed_work`，详见 → [`../BottomHalf/03-workqueue.md`](../BottomHalf/03-workqueue.md)
