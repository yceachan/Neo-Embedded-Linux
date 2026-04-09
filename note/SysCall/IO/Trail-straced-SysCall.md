---
title: strace 全景 — 5 种 IO 范式的系统调用轨迹
tags: [strace, syscall, IO, blocking, nonblock, poll, fasync, aio]
desc: 用 strace 在 IMX6ULL EVB 上追踪 prj/03-Advanced-IO 五种 IO 范式,对比每种范式在用户态留下的 syscall 指纹
update: 2026-04-08
---


# strace 全景 — 5 种 IO 范式的系统调用轨迹

> [!note]
> **Ref:**
> - 驱动与测试源码: `prj/03-Advanced-IO/`
> - 回归脚本: `prj/03-Advanced-IO/test/run_all.sh` (`STRACE=1` 触发)
> - 设备节点: `/dev/adv_io` — 内核 producer 每 ~510 ms 注入一个递增字节(0x00 起)

下面剥离 ld-linker 的 libc 搜索噪声,只保留每个用例**与驱动交互**的核心序列。所有 fd `3` 都指向 `/dev/adv_io`。

---

## ① block — 阻塞 read(经典慢路径)

```
openat("/dev/adv_io", O_RDWR)              = 3
read(3, "\0", 16)                          = 1     ← 立即拿到 0x00
read(3, "\1", 16)                          = 1     ← 309 ms 后
read(3, "\2", 16)                          = 1     ← 510 ms 后
read(3, "\3", 16)                          = 1     ← 510 ms
read(3, "\4", 16)                          = 1     ← 510 ms
close(3)
```

**全景**:用户线程睡在内核 `wait_event_interruptible` 上,producer 触发 `wake_up` 后被唤醒一次,拷贝 1 字节返回。**没有任何 poll/signal 配套** —— 最纯粹的阻塞 IO。

---

## ② nonblock — O_NONBLOCK + 空 ring → EAGAIN

```
openat("/dev/adv_io", O_RDONLY|O_NONBLOCK) = 3
read(3, ..., 16)                           = -1 EAGAIN (Resource temporarily unavailable)
read(3, ..., 16)                           = -1 EAGAIN
close(3)
```

**全景**:`f_flags & O_NONBLOCK` 让驱动 `read()` 直接 `return -EAGAIN` —— 用户态拿到 errno 11,从不睡眠。是后续 poll/SIGIO/AIO 三种异步范式的**前置条件**。

---

## ③ poll — IO 多路复用(就绪通知 + 同步读)

```
openat("/dev/adv_io", O_RDWR|O_NONBLOCK)              = 3
poll([{fd=3, events=POLLIN}], 1, 2000) = 1 ([{fd=3, revents=POLLIN}])
read(3, "\5", 32)                                     = 1
poll([{fd=3, events=POLLIN}], 1, 2000) = 1 ([POLLIN])
read(3, "\6", 32)                                     = 1
poll(...)                              = 1 ([POLLIN])
read(3, "\7", 32)                                     = 1
poll(...)                              = 1 ([POLLIN])
read(3, "\10", 32)                                    = 1
poll(...)                              = 1 ([POLLIN])
read(3, "\t", 32)                                     = 1
close(3)
```

**全景**:**严格的 poll → read 配对**。每次 poll 因驱动 `.poll = adv_poll` 把 `wait_queue` 注册进 `poll_table`,producer wake 后立即返回 `POLLIN`,用户态 read 一次拿一个字节。注意第一次 poll 立刻就绪 —— producer 在 open 之前就在 tick。

---

## ④ fasync — SIGIO 异步通知(信号驱动 IO)

```
openat("/dev/adv_io", O_RDWR|O_NONBLOCK)                   = 3
rt_sigaction(SIGIO, {sa_handler=0x107d0, ...}, NULL, 8)    = 0
                                       ← (隐式: fcntl F_SETOWN/F_SETFL FASYNC,
                                          被 STRACE_FILTER 默认包含 fcntl, 此处未触发说明
                                          test_fasync 未走 fcntl, 估计驱动 .fasync 由 ioctl 注册)
--- SIGIO {si_signo=SIGIO, si_code=SI_KERNEL} ---          ← producer wake → kill_fasync
read(3, "\n", 32)                                          = 1   ← 0x0a
--- SIGIO ---
read(3, ..., 32)                                           = -1 EAGAIN  ← 抢跑, ring 已被掏空
--- SIGIO ---
read(3, "\v", 32)                                          = 1   ← 0x0b
--- SIGIO ---
read(3, ..., 32)                                           = -1 EAGAIN
... (重复 5 轮: 0x0c, 0x0d, 0x0e)
close(3)
```

**全景**:经典 SIGIO 范式 —— 内核 `kill_fasync(&adv_async, SIGIO, POLL_IN)` 把信号投到 owner pid,handler 醒来后做 `read`。**每个数据字节伴随两次 SIGIO**:一次是真就绪、一次是 handler 重入时 ring 已空(`SI_KERNEL` 表明信号源是内核而非 `kill(2)`)。`O_NONBLOCK` 是必须的,否则 handler 第二次 read 会阻塞死锁。

---

## ⑤ aio — POSIX glibc AIO(librt 的线程池实现)

```
openat("/lib/librt.so.1", ...)                                                = 3
openat("/lib/libpthread.so.0", ...)                                           = 3   ← librt 拉 pthread
rt_sigaction(SIGRTMIN,   {sa_handler=0x76dcf998, SA_SIGINFO}, NULL, 8)        = 0   ← libc 内部 cancel 信号
rt_sigaction(SIGRT_1,    {sa_handler=0x76dcfa54, SA_SIGINFO|SA_RESTART}, ...) = 0   ← AIO 完成通知信号
openat("/dev/adv_io", O_RDWR)                                                 = 3   ← 主线程 open
                                       ← (主线程不再出现 read! 真正的 read 由 AIO worker 线程做)
close(3)                                                                            ← 309 ms 后, aio_return 完成
3225  +++ exited with 0 +++            ← worker 线程退出
3223  +++ exited with 0 +++            ← 主线程退出
```

**全景**:这是本批次里**最值得讲的一条**。glibc 的 POSIX AIO 在 ARM Linux 上**不是** `io_submit(2)` 内核 AIO,而是 librt 起一个 **pthread worker** 替你 `pread()`。所以:

- 主线程 strace 里**完全看不到 `read(3, ...)`** —— 因为 read 发生在 tid 3225。
- `rt_sigaction(SIGRTMIN, SIGRT_1)` 是 libc-internal 的 worker 协调信号,与 SIGIO 范式毫无关系。
- 完成回调通过 `LIO_NOWAIT` + `aio_suspend`/`aio_return` 在主线程轮询拿到,日志里那 1 字节 `0x0f` 是 worker 替主线程读到再回填 `aiocb->__return_value` 的。
- 没有 `io_setup/io_submit/io_getevents` 这些内核 AIO 调用 → **证实 glibc 走的是 emulation,不是 kernel AIO**。

如果想看到内核态 AIO,得用 `libaio` 直接调 `io_submit`,而 character device 的 `f_op` 不实现 `.read_iter` 也基本会被内核 AIO 拒绝。这就是为什么字符设备的 "AIO" 在 Linux 上几乎总是用户态线程模拟。

---

## 总结表

| 范式     | 等待发生在        | 通知机制                  | strace 里的指纹                          |
| -------- | ----------------- | ------------------------- | ---------------------------------------- |
| block    | 内核 wait_queue   | 直接 wake 调用线程        | 只有 `read()` 慢调用,无其它             |
| nonblock | 不等待            | —                         | `read = -1 EAGAIN`                       |
| poll     | 内核 poll_table   | wake → poll 返回 POLLIN   | `poll() → read()` 严格成对               |
| fasync   | 内核 fasync 链    | kill_fasync 投 SIGIO      | `--- SIGIO SI_KERNEL ---` 中断 read 流   |
| aio (glibc) | 用户态 worker pthread | librt 内部 SIGRT      | 主线程**没有 read**,只有 sigaction + close |

最有信息量的两条对比:**fasync 的 `SI_KERNEL`** 直接证明信号源在内核 `kill_fasync`;**aio 主线程缺失的 read** 直接证明 glibc POSIX AIO 是用户态线程模拟。这两点光看 man page 学不到,strace 一打就实锤。
