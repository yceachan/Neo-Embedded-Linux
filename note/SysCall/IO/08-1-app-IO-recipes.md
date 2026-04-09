---
title: 用户态 IO 编程模板 — 五种范式落地配方
tags: [Linux, syscall, IO, blocking, nonblocking, poll, select, epoll, fasync, recipe]
desc: 五种 Linux IO 范式的用户态编程模板,每种配独立时序图,机制原理见 03/05/06/07
update: 2026-04-09
---


# 用户态 IO 编程模板

> [!note]
> **Ref:**
> - 驱动侧模板:[`08-drv-fops-recipes.md`](./08-drv-fops-recipes.md)
> - 范式地图:[`04-io-models-overview.md`](./04-io-models-overview.md)
> - 机制详解:[`03-blocking-semantics.md`](./03-blocking-semantics.md)、[`05-poll-kernel.md`](./05-poll-kernel.md)、[`06-multiplex-compare.md`](./06-multiplex-compare.md)、[`07-fasync-sigio.md`](./07-fasync-sigio.md)
> - 实证:[`trail-strace.md`](./trail-strace.md)

本文是**纯用户态落地**配方。每种范式给出最小可运行模板 + 一份独立时序图。机制原因去查 Ref。所有示例假设打开的是 `/dev/my_drv`,该驱动支持 `.read/.write/.poll/.fasync`(见 [`08-drv-fops-recipes.md`](./08-drv-fops-recipes.md))。


## 1. 阻塞 IO — 最简单的开始

### 1.1 模板

```c
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/dev/my_drv", O_RDWR);   // 默认阻塞
    char buf[256];
    ssize_t n = read(fd, buf, sizeof buf);  // 没数据就睡到天荒地老
    if (n > 0) write(STDOUT_FILENO, buf, n);
    close(fd);
}
```

### 1.2 时序

```mermaid
sequenceDiagram
    autonumber
    participant App as "用户进程"
    participant Sys as "VFS / sys_read"
    participant Drv as "驱动 .read"
    participant WQ as "wait_queue"
    participant Src as "驱动唤醒源"

    App->>Sys: "read(fd, buf, n)"
    Sys->>Drv: "f_op->read"
    Drv->>WQ: "wait_event_interruptible(wq, readable)"
    Note over App,Drv: "进程 TASK_INTERRUPTIBLE 睡眠"
    Src->>WQ: "wake_up_interruptible(wq)"
    WQ->>Drv: "条件成立,继续"
    Drv->>Sys: "copy_to_user, return n"
    Sys->>App: "返回 n 字节"
```

**适用**:单 fd、不在乎主循环被卡住的场景(命令行工具、单传感器读取)。


## 2. 非阻塞 IO — 单独使用没意义

### 2.1 模板

```c
int fd = open("/dev/my_drv", O_RDWR | O_NONBLOCK);
char buf[256];

for (;;) {
    ssize_t n = read(fd, buf, sizeof buf);
    if (n > 0) {
        write(STDOUT_FILENO, buf, n);
    } else if (n < 0 && errno == EAGAIN) {
        /* 没数据,做点别的 */
        do_other_work();
    } else {
        perror("read"); break;
    }
}
```

### 2.2 时序

```mermaid
sequenceDiagram
    autonumber
    participant App as "用户进程"
    participant Drv as "驱动 .read"

    loop "忙轮询(应避免)"
        App->>Drv: "read(fd)"
        alt "无数据"
            Drv-->>App: "-EAGAIN 立返"
            App->>App: "do_other_work()"
        else "有数据"
            Drv-->>App: "return n"
        end
    end
```

> [!warning]
> **裸用 `O_NONBLOCK` = CPU 100% 忙等**。它的真正价值是**和 poll/epoll/SIGIO 配合**,作为"读就绪通知后,一次性把缓冲读尽到 EAGAIN"的工具。详见 [`03-blocking-semantics.md`](./03-blocking-semantics.md) §2。


## 3. select — 历史接口

### 3.1 模板

```c
#include <sys/select.h>

int fd = open("/dev/my_drv", O_RDWR);
fd_set rfds;
struct timeval tv;

for (;;) {
    FD_ZERO(&rfds); FD_SET(fd, &rfds);    // ⚠ 每次重建
    tv.tv_sec = 5; tv.tv_usec = 0;         // ⚠ 每次重置(Linux 会改写)

    int n = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (n < 0) { if (errno == EINTR) continue; perror("select"); break; }
    if (n == 0) { puts("timeout"); continue; }

    if (FD_ISSET(fd, &rfds)) {
        char buf[256];
        ssize_t r = read(fd, buf, sizeof buf);
        if (r > 0) write(STDOUT_FILENO, buf, r);
    }
}
```

### 3.2 时序

```mermaid
sequenceDiagram
    autonumber
    participant App as "用户进程"
    participant Sys as "fs/select.c\ndo_select"
    participant Drv as "驱动 .poll"
    participant WQ as "wait_queue"
    participant Src as "驱动唤醒源"

    App->>Sys: "select(nfds, &rfds, NULL, NULL, &tv)"
    rect rgb(220, 240, 255)
    loop "对每个 fd"
        Sys->>Drv: "f_op->poll(file, pt)"
        Drv->>WQ: "poll_wait 挂入 wq"
        Drv-->>Sys: "返回 mask"
    end
    end
    alt "已有就绪"
        Sys-->>App: "立即返回"
    else "全无就绪"
        Sys->>Sys: "schedule_hrtimeout"
        Src->>WQ: "wake_up_interruptible"
        WQ->>Sys: "唤醒"
        rect rgb(220, 240, 255)
        loop "再次扫描每个 fd"
            Sys->>Drv: "f_op->poll(pt=NULL)"
        end
        end
    end
    Sys->>App: "改写 rfds + 返回就绪数 + 改写 tv"
    App->>Drv: "read(fd, ...)"
```

**陷阱**:fd_set 大小硬编码 1024、tv 被改写、rfds 被改写。新代码不要再用。


## 4. poll — 中等规模、跨平台首选

### 4.1 模板(单 fd)

```c
#include <poll.h>

int fd = open("/dev/my_drv", O_RDWR);
struct pollfd pfd = { .fd = fd, .events = POLLIN };

for (;;) {
    int n = poll(&pfd, 1, 5000 /*ms*/);
    if (n < 0) { if (errno == EINTR) continue; perror("poll"); break; }
    if (n == 0) { puts("timeout"); continue; }

    if (pfd.revents & POLLIN) {
        char buf[256];
        ssize_t r = read(fd, buf, sizeof buf);
        if (r > 0) write(STDOUT_FILENO, buf, r);
    }
    if (pfd.revents & (POLLERR | POLLHUP)) break;
}
```

### 4.2 模板(多 fd)

```c
struct pollfd pfds[3] = {
    { .fd = fd_btn,   .events = POLLIN },
    { .fd = fd_uart,  .events = POLLIN },
    { .fd = STDIN_FILENO, .events = POLLIN },
};

int n = poll(pfds, 3, -1);   // -1 = 无限等待
for (int i = 0; i < 3 && n > 0; i++) {
    if (pfds[i].revents & POLLIN) {
        handle(pfds[i].fd);
        n--;
    }
}
```

### 4.3 时序

```mermaid
sequenceDiagram
    autonumber
    participant App as "用户进程"
    participant Sys as "do_sys_poll"
    participant Drv as "驱动 .poll"
    participant WQ as "wait_queue"
    participant Src as "驱动唤醒源"

    App->>Sys: "poll(pfds, n, timeout)"
    rect rgb(220, 240, 255)
    loop "对每个 pollfd"
        Sys->>Drv: "f_op->poll(file, pt)"
        Drv->>WQ: "poll_wait 挂入 wq(_qproc=__pollwait)"
        Drv-->>Sys: "返回 mask"
    end
    end
    alt "已有就绪"
        Sys-->>App: "立即返回"
    else "无就绪"
        Sys->>Sys: "set_current_state(INTERRUPTIBLE)\nschedule_hrtimeout"
        Src->>WQ: "wake_up_interruptible"
        WQ->>Sys: "pollwake → 唤醒"
        rect rgb(220, 240, 255)
        loop "二次扫描"
            Sys->>Drv: "f_op->poll"
        end
        end
    end
    Sys->>App: "拷回 revents + 返回就绪数"
    App->>Drv: "read(fd) 取数据"
```

详细机制见 [`05-poll-kernel.md`](./05-poll-kernel.md)。


## 5. epoll — 大量长连接首选

### 5.1 LT 模板(默认,简单)

```c
#include <sys/epoll.h>

int ep = epoll_create1(EPOLL_CLOEXEC);
int fd = open("/dev/my_drv", O_RDWR);

struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);     // ⚠ 只注册一次

struct epoll_event evs[64];
for (;;) {
    int n = epoll_wait(ep, evs, 64, -1);
    if (n < 0) { if (errno == EINTR) continue; perror("epoll_wait"); break; }

    for (int i = 0; i < n; i++) {
        int rfd = evs[i].data.fd;
        if (evs[i].events & EPOLLIN) {
            char buf[256];
            ssize_t r = read(rfd, buf, sizeof buf);
            if (r > 0) write(STDOUT_FILENO, buf, r);
        }
        if (evs[i].events & (EPOLLERR | EPOLLHUP)) {
            epoll_ctl(ep, EPOLL_CTL_DEL, rfd, NULL);
            close(rfd);
        }
    }
}
```

### 5.2 ET 模板(高吞吐,有铁律)

```c
int fd = open("/dev/my_drv", O_RDWR | O_NONBLOCK);   // ⚠ ET 必须非阻塞

struct epoll_event ev = { .events = EPOLLIN | EPOLLET, .data.fd = fd };
epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);

for (;;) {
    int n = epoll_wait(ep, evs, 64, -1);
    for (int i = 0; i < n; i++) {
        int rfd = evs[i].data.fd;
        if (evs[i].events & EPOLLIN) {
            /* ⚠ ET 必须循环 read 到 EAGAIN,否则边沿事件丢失 */
            for (;;) {
                char buf[256];
                ssize_t r = read(rfd, buf, sizeof buf);
                if (r > 0) write(STDOUT_FILENO, buf, r);
                else if (r < 0 && errno == EAGAIN) break;   // 正常,读尽
                else { perror("read"); goto out; }
            }
        }
    }
}
out: ;
```

> [!warning]
> **ET 两条铁律**(详见 [`06-multiplex-compare.md`](./06-multiplex-compare.md) §3.3):
> 1. fd 必须 `O_NONBLOCK`,否则最后一次 read 会阻塞死整个事件循环。
> 2. 必须循环读到 `EAGAIN`,否则边沿事件丢失,数据滞留直到下一次新事件。

### 5.3 时序

```mermaid
sequenceDiagram
    autonumber
    participant App as "用户进程"
    participant EP as "fs/eventpoll.c"
    participant Drv as "驱动 .poll"
    participant WQ as "wait_queue"
    participant RDY as "ep->rdllist"
    participant Src as "驱动唤醒源"

    Note over App,EP: "── 注册阶段(只一次)──"
    App->>EP: "epoll_create1()"
    App->>EP: "epoll_ctl(ADD, fd, ev)"
    EP->>Drv: "f_op->poll(file, pt)"
    Drv->>WQ: "poll_wait 挂入 wq\nwait_entry.func = ep_poll_callback"
    EP-->>App: "返回"

    Note over App,EP: "── 等待阶段(可重复)──"
    App->>EP: "epoll_wait(ep, evs, 64, -1)"
    EP->>RDY: "rdllist 空? 是 → schedule"
    Src->>WQ: "wake_up_interruptible"
    WQ->>EP: "ep_poll_callback()"
    rect rgb(220, 240, 255)
    EP->>RDY: "list_add(epi, rdllist)"
    EP->>EP: "唤醒 epoll_wait"
    end
    EP->>App: "拷出 rdllist (O(就绪数))"
    App->>Drv: "read(fd) 取数据(ET 需循环到 EAGAIN)"
```


## 6. 信号驱动 IO — 门铃模型

### 6.1 模板

```c
#include <signal.h>
#include <fcntl.h>

static int g_fd;

/* ⚠ handler 内只能调 async-signal-safe 函数 */
static void sigio_handler(int sig)
{
    char buf[256];
    ssize_t n = read(g_fd, buf, sizeof buf);    // syscall 是 AS-safe 的
    if (n > 0) write(STDOUT_FILENO, buf, n);    // write 也是
    /* 严禁 printf/malloc */
}

int main(void)
{
    g_fd = open("/dev/my_drv", O_RDWR | O_NONBLOCK);   // ⚠ 必须非阻塞

    signal(SIGIO, sigio_handler);                       // 1. 装 handler
    fcntl(g_fd, F_SETOWN, getpid());                    // 2. 告诉内核发给谁
    int flags = fcntl(g_fd, F_GETFL);
    fcntl(g_fd, F_SETFL, flags | O_ASYNC);              // 3. 开门铃

    while (1) pause();   // 主循环可以做别的事
}
```

### 6.2 时序

```mermaid
sequenceDiagram
    autonumber
    participant App as "用户进程"
    participant Sys as "VFS / fcntl"
    participant Drv as "驱动 .fasync"
    participant FA as "fasync_queue"
    participant Src as "驱动唤醒源"
    participant Sig as "Signal System"

    rect rgb(220, 240, 255)
    Note over App,Sig: "── 阶段 A: 同步订阅 ──"
    App->>Sys: "signal(SIGIO,h) + F_SETOWN + O_ASYNC"
    Sys->>Drv: "f_op->fasync(fd, file, on=1)"
    Drv->>FA: "fasync_helper 挂入链表"
    Drv-->>App: "返回"
    end

    rect rgb(255, 240, 220)
    Note over App,Sig: "── 阶段 B: 异步爆发 ──"
    Src->>Drv: "数据就绪"
    Drv->>FA: "kill_fasync(SIGIO, POLL_IN)"
    FA->>Sig: "send_sigio → pending bit 置位"
    Note left of Sig: "App 浑然不知"
    end

    rect rgb(220, 255, 220)
    Note over App,Sig: "── 阶段 C: 上下文劫持 ──"
    Sig->>App: "返回用户态前 do_notify_resume\n强行改写 PC → handler"
    App->>Drv: "read(fd) 取数据"
    Drv-->>App: "return n"
    App->>App: "sigreturn 恢复主流程"
    end
```

详见 [`07-fasync-sigio.md`](./07-fasync-sigio.md)。


## 7. 范式速查表

| 范式 | 关键 syscall | 必须 `O_NONBLOCK`? | 复杂度 | 主循环可做事? |
|------|-------------|-------------------|-------|--------------|
| 阻塞 | `read` | 否 | O(1) | 否 |
| 非阻塞轮询 | `read`+`EAGAIN` | 是 | O(1)/次 | 是(但 CPU 100%) |
| select | `select` | 否 | O(nfds) | 等待时不能 |
| poll | `poll` | 否 | O(nfds) | 等待时不能 |
| epoll LT | `epoll_wait` | 否 | O(就绪数) | 等待时不能 |
| epoll ET | `epoll_wait` | **是** | O(就绪数) | 等待时不能 |
| SIGIO | `fcntl(O_ASYNC)` | **是** | O(1) | **是** |


## 8. 范式选型 — 重申决策树

见 [`04-io-models-overview.md`](./04-io-models-overview.md) §3 决策树。一句话:

- **1 个 fd + 主循环可阻塞** → 阻塞 IO
- **1 个 fd + 主循环不能阻塞** → SIGIO
- **多个 fd, < 100** → poll
- **多个 fd, ≥ 100 长连接** → epoll(LT 起步,瓶颈再上 ET)
- **教学/兼容旧代码** → select
