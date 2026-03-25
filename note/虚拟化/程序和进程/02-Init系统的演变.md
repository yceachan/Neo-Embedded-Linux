---
title: Init 系统的演变：SysV、systemd 与 BusyBox
tags: [Init, systemd, SysV, BusyBox, Embedded]
desc: 探讨 Linux PID 1 进程的演变历程，对比服务器与嵌入式场景下的不同选择
update: 2026-03-25

---


# Init 系统的演变：SysV、systemd 与 BusyBox

在 Linux 系统中，内核完成初始化后，会将控制权交给用户态的第一个进程（PID = 1），即 `init` 进程。它是所有用户态进程的祖先。随着 Linux 生态的发展，`init` 进程的具体实现经历了巨大的演变。

如果你在现代的 Ubuntu 或 WSL 环境下执行 `ls -l /sbin/init`，你会发现它通常是一个软链接：
`/sbin/init -> /lib/systemd/systemd`。但这并非 Linux 的唯一选择，特别是在像 IMX6ULL 这样的嵌入式平台上。

## 1. 经典时代：SysV init (System V)

这是最传统的 Linux 初始化系统，源自古老的 Unix System V。

- **工作原理:** 严重依赖 Shell 脚本。配置文件主要在 `/etc/inittab`，启动脚本存放在 `/etc/init.d/`，并根据不同的运行级别（Runlevel，如 `rc3.d`, `rc5.d`）通过软链接的命名（如 `S90mysql`, `K10apache`）来决定启动和停止的顺序。
- **缺点:** **串行启动**。必须等上一个服务（脚本）执行完毕，才能执行下一个。在多核 CPU 时代，这导致了极大的性能浪费，开机速度缓慢。
- **应用场景:** 早期桌面/服务器，目前在部分老旧系统或对启动时间要求不苛刻的轻量系统中使用。

## 2. 现代霸主：systemd

为了解决 SysV init 启动缓慢和逻辑复杂的问题，Red Hat 的 Lennart Poettering 开发了 systemd。它现在是绝大多数 Linux 发行版（Ubuntu, CentOS, Debian, Arch 等）的默认选项。

- **核心优势:** 
  - **并行启动:** 通过 Socket 激活和 D-Bus 激活机制，解耦了服务依赖，最大化利用多核 CPU 并行启动服务，极大地缩短了开机时间。
  - **按需启动:** 服务可以在需要时才被拉起，而不是开机加载“全家桶”。
  - **Cgroups 深度集成:** systemd 利用 Linux 控制组（Cgroups）将每个服务及其所有子进程“关”在一起，彻底解决了“孤儿进程”逃逸的问题，能干净利落地杀掉一个服务及其所有的衍生残留进程。
  - **统一的日志:** 引入了 `journald` 收集全系统的二进制格式日志，方便结构化查询。
- **配置文件:** 摒弃了面条式的 Shell 脚本，改用声明式的 Unit 文件（如 `sshd.service`）。
- **缺点:** 体系过于庞大复杂（常被批评“违背了 Unix 哲学”），侵入性强，占用磁盘和内存较多。

## 3. 嵌入式首选：BusyBox init

在资源受限的嵌入式平台（如我们的 IMX6ULL 开发板，通常只有 256M/512M 内存和几十兆的 Flash），运行庞大的 systemd 显得过于奢侈。

- **工作原理:** 极简主义。BusyBox 提供了一个微型的 `init` 程序。它通常只读取一个简单的配置文件 `/etc/inittab`，然后依次执行 `/etc/init.d/rcS` 里的初始化命令，最后启动一个 Shell（如 `ash`）供用户登录。
- **优点:** 体积极小（整个 BusyBox 只有几 MB，包含了 init 以及数百个常用命令如 ls, cp, grep），内存占用极低。
- **应用场景:** 路由器、物联网设备、工控机（通过 Buildroot 或 Yocto 构建的极简根文件系统通常默认使用它）。

## 4. 横向对比：如何选择？

| 特性 | SysV init | systemd | BusyBox init |
| :--- | :--- | :--- | :--- |
| **启动方式** | 串行（Shell 脚本） | 并行（C 语言，Socket 激活） | 串行（轻量级脚本） |
| **配置方式** | 复杂的 Shell 脚本 | 声明式 Unit 文件 (`.service`) | 极简的 `inittab` 和 `rcS` |
| **资源占用** | 中等 | 高（肥大） | 极低（几 MB 搞定全套指令） |
| **适用场景** | 历史遗留系统 | 现代桌面、服务器（如 WSL/Ubuntu） | 资源受限的嵌入式系统 (IMX6ULL) |
| **进程追踪** | 弱（依靠 PID 文件） | 强（基于 Cgroups 隔离） | 无（依靠开发者手动管理） |

> [!note]
> **Ref:**
> - **PC/WSL 观察**: Ubuntu 22.04 下为指向 systemd 的软链接 (`/sbin/init -> /lib/systemd/systemd`)。
> - **嵌入式板卡实测 (IMX6ULL)**:
>   ```bash
>   [root@imx6ull:~]# ls -l /sbin/init
>   -rwxr-xr-x    1 root     root         35152 Jun 24  2020 /sbin/init
>   ```
>   *(仅 35KB 的独立二进制文件，通常为 Buildroot 编译的 `sysvinit` 包，或者剥离出来的精简版 C 语言 init 程序。这种极致的“瘦身”正是嵌入式系统的核心诉求。)*