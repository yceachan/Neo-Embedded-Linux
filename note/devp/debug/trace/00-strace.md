---
title: strace 速成 — 从入门到日常够用
tags: [debug, strace, syscall, ptrace, observability]
desc: strace 的核心 flag、过滤语法、输出解读、典型使用模式与坑,日常 90% 的 syscall 排错够用
update: 2026-04-08
---


# strace 速成 — 从入门到日常够用

`strace` 把一个进程对内核发起的所有 **系统调用** 拦截下来打印,是 Linux 上最直接的 "看内核在干什么" 的工具。下面按 "会用 → 用得准 → 用得快" 三层讲。

---

## 1. 最小可用形态

```sh
strace ./prog              # 打到 stderr,刷屏
strace -o trace.log ./prog # 落到文件,推荐
strace -p 1234             # attach 到已有进程 (Ctrl-C 脱离)
```

**核心两件事:目标进程 + 输出去向**。光这两个就够看 90% 的卡死/失败问题。

---

## 2. 必备 flag(按重要性排序)

| flag | 作用 | 何时必加 |
|---|---|---|
| `-f` | **跟踪 fork/clone 出来的所有子线程子进程** | 一旦目标会 fork,就必须加,否则只看到父进程 |
| `-o FILE` | 输出到文件而非 stderr | 长跑、多进程、避免污染程序自己的 stderr |
| `-e trace=SET` | 只看指定 syscall(过滤) | 减噪 90% |
| `-tt` | 时间戳精确到微秒(`-t` 只到秒,`-ttt` 是 epoch) | 看时序、算耗时 |
| `-T` | 每行末尾追加该 syscall 耗时 `<0.000123>` | 找慢调用 |
| `-s N` | 字符串参数显示长度,默认 32 太短 | 看 read/write 的真正内容,常用 `-s 96` 或 `-s 256` |
| `-y` | 把 fd 翻译成路径,例如 `read(3</etc/passwd>, ...)` | 不再瞎猜 fd 是谁 |
| `-yy` | `-y` 的加强版,sockets 显示 ip:port | 调网络 |
| `-c` | **统计模式**,只输出 syscall 计数表,不打 trace | 跑完看分布 |
| `-C` | 既打 trace 又打统计 | `-c` 和 trace 都要 |

### 默认套餐

```sh
strace -f -tt -T -s 96 -y -o /tmp/t.log ./prog
```

跟子进程、有时间戳、有耗时、字符串够长、fd 翻译、落文件。基本每次都先这样起手,再按需收紧。

---

## 3. 过滤 — `-e trace=` 的几种形态

```sh
-e trace=read,write,openat            # 列出具体名字
-e trace=file                         # 文件相关一类 (open/stat/unlink/...)
-e trace=process                      # fork/clone/exec/wait/exit
-e trace=network                      # socket/bind/connect/sendto/...
-e trace=signal                       # rt_sigaction/kill/...
-e trace=ipc                          # shm/sem/msg
-e trace=memory                       # mmap/mprotect/brk
-e trace=desc                         # 所有跟 fd 打交道的
-e trace=%stat                        # 所有 stat 家族变种 (-e trace=stat 只匹配字面)
-e trace=!futex,clock_gettime         # 排除噪声 (注意 ! 在 shell 要转义)
```

**记住:`file` `process` `network` 这三个分类组覆盖了 80% 的调试场景。**

---

## 4. 其它常用补丁

```sh
-e signal=!SIGCHLD            # 不打印 SIGCHLD
-e status=failed              # 只看返回失败的 syscall  ← 调 ENOENT/EACCES 神器
-e read=3                     # dump fd 3 上 read 的全部数据 (hex+ascii)
-e write=1,2                  # 同上,写方向
-k                            # 每个 syscall 附栈回溯 (需 libunwind)
-i                            # 显示发起 syscall 的指令地址
-DD                           # 把 strace 自身放到独立进程,降低对目标的干扰
-x / -xx                      # 非可打印字符按 \xNN 显示
-A                            # 追加到 -o 文件而非覆盖
-z                            # 只看成功的 syscall (-Z 反之, 与 -e status 等价但更简短)
```

---

## 5. 解读输出 — 一行看懂

```
2718  17:40:38.534363 openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = -1 ENOENT (No such file or directory) <0.000019>
└──┘ └──────────────┘ └────┘└──────────────────────────────────────────────┘   └──────────────────────────────────┘ └────────┘
 pid    时间戳(-tt)   名字          参数(逗号分隔)                                返回值 + errno符号 + 解释        耗时(-T)
```

特殊行:

```
--- SIGIO {si_signo=SIGIO, si_code=SI_KERNEL} ---     ← 信号送达
+++ exited with 0 +++                                  ← 进程退出
3078  read(3, <unfinished ...>                         ← 阻塞中,被切走
3078  <... read resumed>"\0", 16) = 1                  ← 醒来续上
```

`<unfinished/resumed>` 在 `-f` 多进程模式下很常见,**不是 bug**,是 strace 把交错的多线程 syscall 拼回去的方式。

---

## 6. 三个真实使用模式

### 6.1 "我的程序卡哪了"

```sh
strace -f -tt -p $(pgrep myprog)
```

看最后一个没返回的 syscall(往往是 `futex` `read` `poll` `connect`),立刻知道是锁、IO 还是网络在阻塞。

### 6.2 "为什么 open 失败"

```sh
strace -f -e trace=file -e status=failed ./prog
```

只剩失败的文件操作,errno 一目了然(`ENOENT` 路径错;`EACCES` 权限;`ENOTDIR` 中间路径不是目录...)。

### 6.3 "性能瓶颈在哪个 syscall"

```sh
strace -f -c ./prog
```

输出表:

```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------
 92.31    0.001234         617         2           read
  5.21    0.000070          70         1           clone
  ...
```

`% time` 列直接告诉你时间花在谁身上。配合 `-w`(wall clock 模式)可以看真实墙钟而非 syscall 内部 CPU。

---

## 7. 性能与坑

- **strace 让目标程序慢 10×~100×**(每个 syscall 进出 ptrace 两次)。看算法性能不要用 strace,用 `perf trace`(更轻)或 eBPF(`bpftrace`)。
- **可能改变时序** —— race condition 在 strace 下消失或恶化都常见,这是 heisenbug 调试时要警觉的。
- **不能跟踪静态链接的 musl 程序的某些 vDSO 调用**(`gettimeofday` `clock_gettime` 走 vDSO,不进内核,strace 看不到)。
- **`-f` + 大量短命子进程** 会让 log 爆炸,先用 `-e trace=` 收紧。
- **容器/setuid** 程序需要 `CAP_SYS_PTRACE` 或 `--security-opt seccomp=unconfined`。

---

## 8. 替代/进阶工具

| 工具 | 优势 | 劣势 |
|---|---|---|
| `ltrace` | 跟 **库函数** 而非 syscall(看 malloc/printf) | 不维护得好,有时崩 |
| `perf trace` | 内核原生,**慢得少** | 输出不如 strace 详细 |
| `bpftrace` / `bcc-tools` | 几乎零开销,可自定义聚合 | 需要写 BPF 脚本,门槛略高 |
| `ftrace` (`/sys/kernel/tracing`) | 最快、最深,看内核内部函数 | 配置繁琐 |

**初学就死磕 strace 即可**。等到 strace 因为太慢或太吵满足不了再升级到 `perf trace` / `bpftrace`。

---

## 9. 最值得背下来的 5 条命令

```sh
# 1. 通用起手式
strace -f -tt -T -s 96 -y -o /tmp/t.log ./prog

# 2. 只看失败的文件操作
strace -f -e trace=file -e status=failed ./prog

# 3. attach 已经卡死的进程
strace -f -tt -p $(pgrep myprog)

# 4. syscall 频次/耗时统计
strace -f -c ./prog

# 5. 追踪特定 fd 的数据流
strace -f -e read=3 -e write=3 -s 256 ./prog
```

把这 5 条贴到 cheat sheet 里,90% 的日常排错就够了。其它 flag 真用到再查 `man strace`。
