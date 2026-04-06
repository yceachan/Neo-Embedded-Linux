---
title: Linux 根文件系统 (RootFS) 结构概览
tags: [RootFS, FHS, Linux, Storage]
desc: 基于 FHS 标准的 Linux 根目录层次结构及核心目录功能说明。
update: 2026-04-07

---


# Linux 根文件系统 (RootFS) 结构概览

> [!note]
> **FHS (Filesystem Hierarchy Standard)**: 文件系统层次结构标准。它定义了 Unix-like 系统中主要目录及目录内容的布局，确保了不同 Linux 发行版之间路径的一致性与软件的可移植性。
>
> **Ref:** [Filesystem Hierarchy Standard (FHS)](https://refspecs.linuxfoundation.org/fhs.shtml), Local System `ls -l /` output.


## 1. 核心目录架构图

- **`/` (Root)**
    - 📂 **核心系统 (UsrMerge)**
        - `bin -> usr/bin`: 用户二进制程序(ls, cp, etc.)
        - `sbin -> usr/sbin`: 系统二进制程序 (ip, fdisk,init etc.)
        - `lib -> usr/lib`: 核心系统库 (libc, kernel modules)
        - `lib64 -> usr/lib64`: 64 位系统库
    - 📂 **配置与启动**
        - `/etc`: 系统全局配置文件 (network, users, fstab)
        - `/boot`: 引导加载程序文件 (vmlinuz, initrd, grub)
        - `/init`: 初始执行程序 (init process)
    - 📂 **虚拟/设备文件系统**
        - `/dev`: 设备节点 (tty, sda, null)
        - `/proc`: 进程与内核信息 (伪文件系统)
        - `/sys`: 内核设备树与驱动管理 (伪文件系统)
    - 📂 **用户与应用数据**
        - `/usr`: （Unix Shared Resource）用户软件主体 (bin, include, lib, share)
        - `/var`: 可变数据 (log, cache, spool)
        - `/home`: 普通用户家目录
        - `/root`: 管理员 (root) 家目录
        - `/opt`: 第三方软件包挂载点
    - 📂 **运行时与挂载**
        - `/run`: 运行时数据 (PID files, sockets)
        - `/tmp`: 临时文件 (重启通常清理)
        - `/mnt`: 临时挂载点
        - `/media`: 可移动介质挂载点 (USB, CD-ROM)
    - 📂 **其他**
        - `/lost+found`: 文件系统修复找回的文件
        - `/srv`: 本机服务提供的数据
        - `/snap`: Snap 软件包挂载点


## 2. 目录详细说明

依据您提供的系统快照，该系统遵循 **UsrMerge** 方案，将基础二进制文件和库统一存放于 `/usr` 中。

| 目录 | 类型 | 说明 |
| :--- | :--- | :--- |
| **`/bin`** | Symlink | 指向 `usr/bin`。包含所有用户可用的基本命令（如 `ls`, `cp`）。 |
| **`/boot`** | Directory | 包含静态引导加载程序文件（内核镜像 `vmlinuz`, `initrd`）。 |
| **`/dev`** | Directory | 包含设备节点文件（如 `sda`, `tty`, `null`），是硬件的逻辑表示。 |
| **`/etc`** | Directory | **系统配置中心**。存放所有系统管理所需的本地配置文件。 |
| **`/home`** | Directory | 普通用户的个人目录，存储用户数据和个人配置。 |
| **`/init`** | File | 系统启动时的初始执行程序（在某些发行版或 WSL 中常见）。 |
| **`/lib*`** | Symlink | 指向 `usr/lib*`。核心系统库，供 `/bin` 和 `/sbin` 中的二进制文件使用。 |
| **`/proc`** | Pseudo-FS | 虚拟文件系统，提供内核与进程状态的接口。 |
| **`/root`** | Directory | 系统管理员（root）的家目录。 |
| **`/run`** | Directory | 存放自系统启动以来的运行时信息（如 PID 文件、锁文件）。 |
| **`/sbin`** | Symlink | 指向 `usr/sbin`。系统管理员使用的基本系统管理命令。 |
| **`/sys`** | Pseudo-FS | 导出内核对象的层次结构，用于设备驱动管理。 |
| **`/tmp`** | Directory | 临时文件存放处，通常在系统重启时清理。 |
| **`/usr`** | Directory | **只读用户数据主体**。包含绝大多数用户工具、库、文档。 |
| **`/var`** | Directory | **可变数据区**。存放日志 (`/var/log`)、邮件、数据库等。 |


## 4. 深度解析：两个 Init 的区别

在您的系统中存在两个 `init`，它们分工完全不同：

### `/init` (基础引导 Init)
- **类型**: `ELF 64-bit executable, statically linked` (静态链接)
- **角色**: **内核直接启动的第一个进程 (PID 1)**。
- **特点**:
    - **静态链接**: 它不依赖任何外部库（如 `libc.so`），因为在它启动时，系统的库目录可能还没挂载好。
    - **环境搭建**: 在 WSL2 或引导加载阶段，它负责最基础的任务：挂载物理分区、设置网络桥接、初始化与宿主机的通信。
    - **职责**: 它是“修路工”，修好路后，再去叫真正的“管家”过来。

### `/sbin/init` (系统管理 Init)
- **类型**: `symbolic link to /lib/systemd/systemd` (符号链接)
- **角色**: **操作系统级管家 (Systemd)**。
- **特点**:
    - **动态链接**: 它依赖大量的库文件来支持复杂的功能（如网络管理、用户登录、日志系统）。
    - **服务管理**: 负责启动 SSH、数据库、图形界面等所有用户服务。
    - **职责**: 它通过 `systemctl` 与用户交互，维持系统的日常运行。

### 对比总结

| 特性 | `/init` | `/sbin/init` (systemd) |
| :--- | :--- | :--- |
| **启动顺序** | **最先启动** (由内核调用) | **随后启动** (通常由 /init 调用) |
| **依赖性** | 独立运行 (Statically Linked) | 依赖系统库 (Dynamically Linked) |
| **主要功能** | 基础设施搭建、环境桥接 | 服务进程管理、系统状态维持 |
| **可见性** | 用户很少直接操作 | 用户通过 `systemctl` 频繁操作 |

---

## 5. 常见问题 (FAQ)

**Q: 为什么 `/sbin/init` 是个链接？**
A: 这是为了兼容性。历史上 Linux 有多种 init 系统（SysVinit, Upstart, Systemd）。通过把 `/sbin/init` 指向当前生效的 init 程序，脚本和内核可以永远通过 `/sbin/init` 这个标准路径来启动系统，而不需要关心具体用的是哪种管家。
