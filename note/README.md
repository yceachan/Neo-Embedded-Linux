此 knowledge base 是 yceachan 面向 Agent，探索 Linux 内核机制与驱动开发过程中的札记产出。0 基础入门过程札记已归档至 `Legacy/`。

## 顶层导览

| 目录 | 主题 | 入口 |
|------|------|------|
| `KernelLearning最佳实践.md` | 学习方法论与阶段计划 | — |
| `Kbuild/` | 内核构建系统治理 | `Kbuild/` 8 篇 |
| `DTS/` | 设备树：用法、语法、binding、DTS→驱动映射 | `DTS/` |
| `SoC-Arch/` | IMX6ULL 硬件架构（Cortex-A7、GICv2、内存接口） | `SoC-Arch/` |
| `SysCall/` | 系统调用、IO 范式、进程地址空间、MMU | `SysCall/IO/04-IO范式总览.md` |
| `FS/` & `VFS/` | 虚拟文件系统、字符设备源码剖析 | `FS/` |
| `Subsystem/` | 子系统（Interrupt、gpio…） | `Subsystem/` |
| `kernel/` | **内核执行与并发**（五子域，见下） | `kernel/plan-20260407.md` |
| `BSP-Dev/` | 100ask BSP 开发实践 | `BSP-Dev/` |
| `虚拟化/` | 进程虚拟化、IPC（pipe/signal）、程序加载 | `虚拟化/` |
| `Legacy/` | 已归档的入门札记 | — |

## `kernel/` 

按"代码在什么上下文运行 / 何时被调度 / 如何延迟 / 如何同步 / 何时计时"重组：

| 子域 | 职责 | 入口 |
|------|------|------|
| `kernel/context/` | 五类内核执行上下文、`preempt_count`、ARM 调度触发点 | `context/00-overview.md` |
| `kernel/sched/` | 进程调度器（`sched_class` / CFS / 抢占模型 / wake_up 路径） | `sched/00-overview.md` |
| `kernel/defer/` | 延迟/异步执行机制：softirq、tasklet、workqueue、threaded-irq、wait_queue、completion、kthread | `defer/00-overview.md` |
| `kernel/time/` | 时间子系统：clocksource/clockevents/tick、jiffies、timer_list、hrtimer、IMX6ULL GPT | `time/00-overview.md` |
| `kernel/sync/` | 同步原语：spinlock、mutex/semaphore、RCU、atomic/memory barrier | `sync/00-overview.md` |

治理过程见 `kernel/plan-20260407.md`。

## 驱动项目（`prj/`）

| 项目 | 主题 |
|------|------|
| `prj/01_hello_drv` | 树外字符设备模板 + 三层 Makefile 模式 |
| `prj/02_mmap_drv` | mmap 驱动：内核内存映射到用户空间 |
| `prj/03_advanced_IO_cdev` | 高级 IO 范式综合示例（阻塞/非阻塞/poll/SIGIO/AIO） |
