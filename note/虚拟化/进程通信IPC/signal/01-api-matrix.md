---
title: 信号 API 家族矩阵详解
tags: [IPC, Signal, API, C-Language]
desc: 详细梳理 Linux 信号机制相关的 C 语言标准库函数与系统调用，包含函数原型、参数解析与核心标志位。
update: 2026-03-26

---

# 信号 API 家族矩阵详解

> [!note]
> 包含头文件: `#include <signal.h>` (部分系统调用需要 `<sys/types.h>` 和 `<unistd.h>`)

在 Linux 环境下，信号的 API 形成了一套完整的操作体系。我们将它们分为五大类：**信号集操作**、**信号发送**、**信号注册**、**信号屏蔽与挂起**、**多线程与定时**。

---

## 1. 信号集操作 API (Signal Sets)

信号集 `sigset_t` 是一个位图（Bitmask），用于表示一组信号。内核通过它来管理进程的**阻塞信号掩码 (Blocked Mask)** 和 **挂起信号队列 (Pending)**。

```c
// 1. 初始化信号集为空 (全 0)
int sigemptyset(sigset_t *set);

// 2. 初始化信号集为包含所有信号 (全 1)
int sigfillset(sigset_t *set);

// 3. 将指定信号 signum 添加到信号集中
int sigaddset(sigset_t *set, int signum);

// 4. 将指定信号 signum 从信号集中移除
int sigdelset(sigset_t *set, int signum);

// 5. 判断 signum 是否在信号集中 (返回 1 表示在，0 表示不在)
int sigismember(const sigset_t *set, int signum);
```

---

## 2. 信号发送 API (Sending)

### 2.1 基础发送：`kill`
```c
int kill(pid_t pid, int sig);
```
`kill` 函数不仅可以发给单个进程，其 `pid` 参数有四种极其重要的语义分流：
- `pid > 0`: 发送给 PID 为 `pid` 的具体进程。
- `pid == 0`: 发送给与发送者**同在一个进程组 (Process Group)** 的所有进程。
- `pid == -1`: 广播！发送给当前进程有权限发送的所有进程（除 `init` 进程外）。
- `pid < -1`: 发送给进程组 ID 等于 `|pid|` (pid 的绝对值) 的所有进程。

### 2.2 携带数据的精准打击：`sigqueue`
```c
int sigqueue(pid_t pid, int sig, const union sigval value);

// union sigval 的定义：
union sigval {
    int   sival_int;  // 携带一个整型
    void *sival_ptr;  // 携带一个指针 (仅限同进程内有意义，跨进程通常无效)
};
```
- **特点**: 专为实时信号 (Real-time signals, `SIGRTMIN` ~ `SIGRTMAX`) 设计，信号不仅会排队（不丢失），还能携带数据负载。

### 2.3 自我了断：`raise` 与 `abort`
```c
int raise(int sig);   // 等同于 kill(getpid(), sig) 或 pthread_kill(pthread_self(), sig)
void abort(void);     // 极其粗暴，给自己发送 SIGABRT，强制终止并生成 Core Dump。
```

---

## 3. 信号注册 API (Handling)

### 3.0 传统注册：`signal`

```c
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
```

`handler` 有三种取值：

| `handler` 值 | 含义 |
|---|---|
| 函数指针 | 信号到达时调用该函数 |
| `SIG_DFL` | 恢复默认行为（大多数信号默认终止进程） |
| `SIG_IGN` | 忽略该信号（`SIGKILL`/`SIGSTOP` 不可忽略） |

返回值为**上一个** handler，可用于保存和恢复原始行为。

**完整用法示例：**

```c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/* ── 1. 自定义 handler ───────────────────────────────────────── */
static void on_sigint(int sig)
{
    /* 注意：handler 内只能调用 async-signal-safe 函数 */
    write(STDOUT_FILENO, "\n[signal] SIGINT caught, exiting...\n", 36);
    _exit(0);                           /* 不能用 exit()，非 async-signal-safe */
}

int main(void)
{
    /* ── 2. 注册自定义 handler ───────────────────────────────── */
    sighandler_t prev = signal(SIGINT, on_sigint);
    if (prev == SIG_ERR) {
        perror("signal");
        return 1;
    }

    /* ── 3. 保存并临时忽略 SIGTERM ───────────────────────────── */
    sighandler_t prev_term = signal(SIGTERM, SIG_IGN);
    printf("[main] SIGTERM ignored temporarily\n");
    sleep(2);

    /* ── 4. 恢复 SIGTERM 默认行为 ────────────────────────────── */
    signal(SIGTERM, prev_term);         /* 用返回值还原 */
    printf("[main] SIGTERM restored, waiting for Ctrl+C...\n");

    /* ── 5. 等待信号 ─────────────────────────────────────────── */
    while (1)
        pause();                        /* 挂起，直到任意信号到达 */

    return 0;
}
```

> **为何生产代码应优先用 `sigaction` 而非 `signal`：**
> - `signal()` 在 handler 执行期间对同一信号的行为是**未定义的**（POSIX 允许实现自动重置为 `SIG_DFL`，即只触发一次）。
> - `signal()` 无法设置 `SA_RESTART`，被信号打断的系统调用会返回 `EINTR`，需要手动重试。
> - `signal()` 无法在 handler 内通过 `siginfo_t` 获取发送者信息。
>
> `signal()` 适合快速原型和一次性脚本工具；正式项目请使用 `sigaction`。

---

### 3.1 现代标准注册：`sigaction`
```c
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
```
这是替代老旧 `signal()` 函数的绝对主力。它的强大在于 `struct sigaction` 结构体：

```c
struct sigaction {
    void     (*sa_handler)(int);                          // 标准回调函数
    void     (*sa_sigaction)(int, siginfo_t *, void *);   // 高级回调函数 (携带上下文)
    sigset_t   sa_mask;                                   // 执行回调期间，需额外阻塞的信号集
    int        sa_flags;                                  // 控制标志位
    void     (*sa_restorer)(void);                        // 废弃，由系统内部 libc 使用 (rt_sigreturn)
};
```

**关键 `sa_flags` 解析：**
- `SA_SIGINFO`: 标志内核使用 `sa_sigaction` 而非 `sa_handler`，此时回调参数会包含 `siginfo_t`（内含发送者 PID、UID 及 `sigqueue` 发送的数据 `si_value`）。
- `SA_RESTART`: 被该信号打断的系统调用（如 `read`、`wait`），在信号处理完成后**自动重启**，而不是返回 `-1` 并置 `errno=EINTR`。
- `SA_NODEFER`: 在处理某信号时，**不自动阻塞**该信号本身（允许嵌套重入）。

---

## 4. 信号屏蔽与挂起 API (Masking & Suspend)

### 4.1 进程信号屏蔽字：`sigprocmask`
```c
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
```
用于显式修改进程的**阻塞掩码**。即使信号到达，也会被挂起在 `pending` 队列中，直到解蔽。
- `how = SIG_BLOCK`: 将 `set` 中的信号加入到现有的阻塞列表中（位或）。
- `how = SIG_UNBLOCK`: 从现有的阻塞列表中解除 `set` 中的信号。
- `how = SIG_SETMASK`: 简单粗暴，直接用 `set` 替换掉当前的阻塞列表。

### 4.2 终极防竞态挂起：`sigsuspend`
```c
int sigsuspend(const sigset_t *mask);
```
**原子操作**。它将进程的信号屏蔽字临时替换为 `mask`，然后挂起进程。当被信号唤醒并处理完毕后，屏蔽字会自动恢复为调用前的值。用于解决“解蔽并 pause 等待”过程中的竞态条件死锁问题。

### 4.3 基础挂起：`pause`
```c
int pause(void);
```
让进程进入可中断睡眠 (`TASK_INTERRUPTIBLE`)，直到任何一个未被屏蔽的信号到达。

---

## 5. 定时器与多线程专属 (Timers & Threads)

### 5.1 POSIX 高级定时器
```c
#include <time.h>
int timer_create(clockid_t clockid, struct sigevent *sevp, timer_t *timerid);
int timer_settime(timer_t timerid, int flags, const struct itimerspec *new_value, struct itimerspec * old_value);
```
通过 `struct sigevent`，你可以配置定时器到期时：
1. 发送哪个具体的信号。
2. 携带什么样的数据 (`sigval`)。
3. 甚至可以选择不发信号，而是直接在内核新建一个线程来执行回调 (`SIGEV_THREAD`)。

### 5.2 线程级定向
```c
#include <pthread.h>
int pthread_kill(pthread_t thread, int sig);
```
向同进程内的指定线程发送信号。多用于多线程状态机的同步或强制打断某线程的阻塞系统调用。
