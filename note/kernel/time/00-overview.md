---
title: Linux 时间子系统全景
tags: [time, clocksource, clockevents, hrtimer, jiffies, tick, imx6ull]
desc: Linux 4.9.88 时间子系统的分层结构、核心对象依赖关系以及在 IMX6ULL 上的落地
update: 2026-04-07

---


# Linux 时间子系统全景

> [!note]
> **Ref:** [`kernel/time/clocksource.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/time/clocksource.c), [`kernel/time/clockevents.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/time/clockevents.c), [`kernel/time/tick-common.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/time/tick-common.c), [`kernel/time/hrtimer.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/time/hrtimer.c), [`kernel/time/timer.c`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/kernel/time/timer.c), [`include/linux/jiffies.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/jiffies.h)

## 1. 两个基本问题

Linux 时间子系统回答两个正交的问题：

1. **“现在几点？”** —— 需要一个**单调递增、高分辨率、可读**的计数器 → **clocksource**
2. **“到点叫我。”** —— 需要一个**可编程产生中断**的硬件 → **clockevents**

所有上层 API（`jiffies`、`timer_list`、`hrtimer`、`nanosleep`、`gettimeofday` …）都构筑在这两类抽象之上。

---

## 2. 核心依赖图（中心图）

```mermaid
graph TD
    subgraph HW["硬件层 (IMX6ULL)"]
        GPT["GPT (General Purpose Timer)\n24MHz, 32-bit"]
        EPIT["EPIT\n递减计数"]
        ARMTIMER["ARM Arch Timer\nCP15 CNTPCT"]
    end

    subgraph ABS["抽象层 (kernel/time/)"]
        CS["clocksource\n只读计数源\nclocksource.c"]
        CE["clockevents\n可编程中断源\nclockevents.c"]
    end

    subgraph TICK["tick 层"]
        TD["tick_device\ntick-common.c\n(每 CPU 一个)"]
        SCHED["tick-sched.c\nNOHZ / dyntick"]
    end

    subgraph LOWRES["低分辨率定时 (legacy)"]
        JIF["jiffies\nHZ=100/250\njiffies.h"]
        TL["timer_list\n哈希轮 (timer wheel)\ntimer.c"]
    end

    subgraph HIGHRES["高分辨率定时"]
        HR["hrtimer\n红黑树\nhrtimer.c"]
    end

    subgraph API["用户/内核 API"]
        KAPI["msleep / schedule_timeout\nmod_timer / add_timer"]
        UAPI["nanosleep / setitimer\nclock_gettime / gettimeofday"]
    end

    GPT --> CE
    EPIT -. "备选" .-> CE
    ARMTIMER --> CS
    ARMTIMER -. "per-CPU" .-> CE

    CE --> TD
    TD -- "tick 中断" --> JIF
    TD --> SCHED
    JIF --> TL
    TL --> KAPI

    CE --> HR
    CS --> HR
    HR --> UAPI
    HR --> KAPI

    CS --> UAPI

    classDef hw fill:#ffe4b5,stroke:#333,color:#000
    classDef abs fill:#d4f1d4,stroke:#333,color:#000
    classDef hr fill:#e0e0ff,stroke:#333,color:#000
    class GPT,EPIT,ARMTIMER hw
    class CS,CE,TD abs
    class HR,HIGHRES hr
```

> **4.9.88 现实**：低分辨率路径 `clockevents → tick_device → tick 中断 → jiffies++ → timer_list 到期` 始终存在；打开 `CONFIG_HIGH_RES_TIMERS` 后，`tick_device` 切为 oneshot 模式，`hrtimer` 接管下一次中断编程。

---

## 3. 典型 tick 一次完整流程

```mermaid
sequenceDiagram
    autonumber
    participant HW as "GPT 硬件"
    participant GIC as "GIC-400"
    participant CE as "clockevents"
    participant TD as "tick_device"
    participant JIF as "jiffies / timer wheel"
    participant HR as "hrtimer"
    participant SCHED as "scheduler_tick"

    rect rgb(220, 240, 255)
    HW ->> GIC: 比较匹配 → IRQ
    GIC ->> CE: 触发 event_handler
    CE ->> TD: tick_handle_periodic()
    TD ->> JIF: do_timer() → jiffies++
    JIF ->> JIF: run_local_timers()\n→ raise TIMER_SOFTIRQ
    TD ->> HR: hrtimer_interrupt()\n运行到期高精度定时器
    TD ->> SCHED: update_process_times()\n→ scheduler_tick()
    end
```

---

## 4. IMX6ULL 时间硬件落地

| 硬件 | 角色 | 备注 |
|------|------|------|
| **GPT** | 主 clockevent device | `arch/arm/boot/dts/imx6ul.dtsi` 中 `gpt1`, 24MHz 输入，用作 tick 源 |
| **EPIT** | 备选 timer | Enhanced Periodic Interrupt Timer，早期 BSP 曾用作 clockevent |
| **ARM Arch Timer** | 可作 clocksource/clockevent | Cortex-A7 CP15 `CNTPCT_EL0`（per-CPU），IMX6ULL 单核场景意义较小 |
| **SNVS RTC** | 墙上时间 | 不参与 tick，仅供 `hwclock` |

DTS 中常见节点：`gpt1: gpt@2098000 { compatible = "fsl,imx6ul-gpt" };`，驱动 `drivers/clocksource/timer-imx-gpt.c` 同时注册 clocksource + clockevents。

---

## 5. 分辨率与 HZ

- `HZ` 在 ARM 默认 **100**，即一个 jiffy = 10ms
- `jiffies` 为 32 位（`jiffies_64` 64 位），回绕需用 `time_after()` / `time_before()` 宏
- `timer_list` 精度受限于 HZ；**hrtimer** 精度可达硬件分辨率（GPT 24MHz ≈ 41ns）

---

## 6. 笔记导航

| 文件 | 内容 |
|------|------|
| [`01-jiffies-HZ.md`](./01-jiffies-HZ.md) | `jiffies` / `HZ` / 回绕比较宏 / `time_after` |
| [`02-soft-timer.md`](./02-soft-timer.md) | `timer_list` 哈希轮、`mod_timer`、TIMER_SOFTIRQ |
| [`03-hrtimer.md`](./03-hrtimer.md) | `hrtimer` 红黑树、oneshot 模式、`schedule_hrtimeout` |
| [`04-tick-nohz.md`](./04-tick-nohz.md) | tick_device、周期 vs oneshot、NOHZ/dyntick |
| [`05-imx6ull-gpt.md`](./05-imx6ull-gpt.md) | IMX6ULL GPT 控制器寄存器与 timer-imx-gpt.c 剖析 |

交叉引用：[`../defer/00-overview.md`](../defer/00-overview.md)（定时器属于“延迟执行”家族的一员）。
