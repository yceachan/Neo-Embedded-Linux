---
title: 深入内核信号机制：以 kill_fasync 为例 (Signal-driven IO)
tags: [kernel, signal, ipc, io]
desc: 解析 Linux 内核中的 kill_fasync 实现机制，探讨信号驱动 IO (SIGIO) 从内核到用户态的投递全过程。
update: 2026-04-07
---

# 深入内核信号机制：以 kill_fasync 为例

> [!note]
> **Ref:** `fs/fcntl.c`

`kill_fasync` 是 Linux 内核信号驱动 IO (Signal-driven IO) 的核心分发者。它的工作本质是：**遍历订阅者名单，给每个进程投递异步信号（通常是 SIGIO）。**

在编写底层字符设备驱动时，我们通常在数据就绪时调用它：
```c
kill_fasync(&d->fasync_q, SIGIO, POLL_IN);
```

以下是它的底层工作流程及信号投递的闭环分析。

---

## 1. 订阅者名单是谁？ (`fasync_q`)

`d->fasync_q` 是一个单向链表，保存了所有订阅了该设备异步通知的进程信息。

- 当用户侧调用 `fcntl(fd, F_SETFL, O_ASYNC)` 开启异步模式时，内核会调用底层驱动的 `.fasync` 回调函数。
- 驱动内部通常只是一层薄薄的封装，调用内核提供的 `fasync_helper`。
- `fasync_helper` 负责将当前进程的 PID、文件指针等信息封装成 `struct fasync_struct`，并将其挂载到 `d->fasync_q` 链表上。

---

## 2. 源码深度解析

根据 `fs/fcntl.c` 的源码，信号投递逻辑分为两层：外层负责并发保护，内层负责遍历投递。

### 2.1 外层入口：`kill_fasync`

```c
void kill_fasync(struct fasync_struct **fp, int sig, int band) {
    /* 无锁的快速检测：绝大多数情况下链表是空的 */
    if (*fp) {
        rcu_read_lock(); // RCU 读侧加锁
        kill_fasync_rcu(rcu_dereference(*fp), sig, band);
        rcu_read_unlock();
    }
}
EXPORT_SYMBOL(kill_fasync);
```
- **快速检查**：如果 `*fp` 为空（即没有任何进程订阅），直接返回，极大地减少了内核级开销。
- **并发保护 (RCU)**：使用 `RCU` (Read-Copy-Update) 锁保护遍历过程。由于 `kill_fasync` 通常在中断上下文或自旋锁内被调用（如 Workqueue 中），使用 RCU 确保了极低的读开销，且绝对不会造成系统睡眠。

### 2.2 内层遍历：`kill_fasync_rcu`

```c
static void kill_fasync_rcu(struct fasync_struct *fa, int sig, int band) {
    while (fa) {
        if (fa->fa_file) {
            // 获取文件的拥有者 (即之前用户态 F_SETOWN 设置的进程信息)
            struct fown_struct *fown = &fa->fa_file->f_owner;
            // 核心投递动作：向进程发送 SIGIO
            send_sigio(fown, fa->fa_fd, band);
        }
        fa = rcu_dereference(fa->fa_next); // 安全地移动到下一个订阅节点
    }
}
```

---

## 3. 信号是如何最终送达用户态的？

`send_sigio` 仅仅是将信号标记在内核中，真正的执行涉及内核态到用户态的跳变机制：

1. **信号挂载**：`send_sigio` 最终调用内核的信号系统（如 `group_send_sig_info`）。内核根据 PID 找到目标进程的 `task_struct`，将 `SIGIO` 的位标记在该进程的**挂起信号位图（pending signal bitmap）**中。
2. **时机触发**：目标进程当前可能正在睡眠、在其他 CPU 上执行用户代码，或者正在执行其他系统调用。
3. **返回检查**：当下一次目标进程**从内核态返回用户态**前夕（例如系统调用结束、中断返回），或是由于调度器重新选中该进程运行时，内核强制执行 `do_notify_resume` 检查信号位图。
4. **强行跳变**：内核发现有未处理的 `SIGIO`，它会修改进程用户空间的栈和指令指针（PC寄存器），强制程序跳转到预先用 `sigaction` 注册的 `sigio_handler` 信号处理函数中去。

---

## 4. 参数 `band` (如 POLL_IN) 的意义

在调用 `kill_fasync(&d->fasync_q, SIGIO, POLL_IN);` 时，第三个参数是 `band` 掩码。

- 虽然标准的 `SIGIO` 信号是一个无数据载荷的简单中断号，但内核会将这个事件状态记录在文件描述符中。
- 如果用户空间使用更高级的 `sigaction` 配合 `SA_SIGINFO` 标志来注册信号处理函数，那么处理函数将接收到一个 `siginfo_t` 结构体指针。
- 这个结构体中的 `si_band` 字段就承载了 `POLL_IN` 的信息。
- **作用**：程序可以通过判断 `si_band` 来得知：是因为“有数据可读”(`POLL_IN`) 触发的信号，还是因为“有空间可写”(`POLL_OUT`) 触发的信号，从而避免在收到信号时进行无意义的盲读或盲写。

---

## 总结

`kill_fasync` 扮演了内核中的**异步广播电台**角色：
- **驱动**触发数据到达开关。
- **`kill_fasync`** 持有 RCU 锁，查询异步订阅者链表（`fasync_q`）。
- 给每个订阅者的信号位图打上标记。
- 操作系统利用内核态返回的空隙，强行打断用户程序的线性执行流，完成异步事件的通知。
