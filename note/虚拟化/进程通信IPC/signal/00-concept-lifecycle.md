---
title: 信号机制 (Signal) 概念与生命周期
tags: [IPC, Signal, Linux-Kernel]
desc: 探讨信号的异步本质、完整生命周期全景图，以及信号与硬件中断的异同。
update: 2026-03-25

---

# 信号机制 (Signal) 概念与生命周期

> [!note]
> **Ref:** [The Linux Kernel API](https://www.kernel.org/doc/htmldocs/kernel-api/) , `note/Legacy/中断子系统.md`

信号是 Linux 系统中最古老的异步通知机制，本质上是一种**软件中断**。它允许内核或进程异步地通知目标进程发生了某种事件。

## 1. 信号全景时序图 (Lifecycle)

结合 Demo 实验，信号的完整生命周期包含：**注册、挂起、捕获分流、恢复/中止**。

> 若app 未注册这个信号量SIGUSR1，则会默认走exit流程。否则，仅会执行回调并resume现场。

```mermaid
sequenceDiagram
    autonumber
    participant P1 as "发送端 (Bash/kill)"
    participant K as "内核 (Kernel)"
    participant P2 as "接收端 (signal_demo)"

    rect rgb(240, 248, 255)
    Note over P2, K: 阶段 1: 信号注册与挂起等待
    P2 ->> K: sigaction(SIGUSR1, &sa) 注册自定义回调
    K -->> P2: 注册成功
    P2 ->> K: pause() 系统调用，进入可中断睡眠 (TASK_INTERRUPTIBLE)
    end

    rect rgb(245, 245, 220)
    Note over P1, P2: 阶段 2: 产生与递送 (自定义捕获)
    P1 ->> K: kill(pid, SIGUSR1)
    K ->> K: 挂入信号队列，设置 TIF_SIGPENDING 标志并唤醒 P2
    Note over K: 调度 P2 运行，在返回用户态关卡执行 do_signal()
    K ->> P2: 修改用户栈和 PC 指针，跳转执行 handle_sigusr1()
    P2 ->> K: 执行 rt_sigreturn() 恢复上下文
    K -->> P2: 恢复原始执行流 (pause 返回 EINTR)
    end

    rect rgb(255, 228, 225)
    Note over P1, P2: 阶段 3: 致命信号触发终止 (Abort 机制)
    P1 ->> K: kill(pid, SIGINT) 
    K ->> K: do_signal() 提取发现 SIGINT 动作为默认 (SIG_DFL)
    K ->> K: 拦截！拒绝交还 PC 指针，直接切入 do_group_exit()
    Note over K: P2 状态置为: 僵尸 (EXIT_ZOMBIE)
    end
```

## 2. 信号 vs 硬件中断

信号是软件对硬件中断机制的一种模拟。

| 特性 | 软中断 (Signal) | 硬中断 (Interrupt) |
| :--- | :--- | :--- |
| **触发源** | 进程或内核 (软件) | 硬件设备 (硬件) |
| **处理位置** | 目标进程上下文 | 中断上下文 (非进程) |
| **异步性** | 进程级异步 | 指令级异步 |
| **嵌套性** | 受 `sa_mask` 控制 | 受中断屏蔽位控制 |

## 3. 核心数据结构

- `task_struct->pending`: 挂起信号队列。
- `task_struct->sighand`: 信号处理函数映射表。
- `task_struct->blocked`: 信号屏蔽字（阻塞位图）。
