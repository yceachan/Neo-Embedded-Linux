# Neo-Embedded-Linux

> **A modern, AI-assisted playbook for mastering Linux Driver,System,APP development.**
>
> Hardware Platform : NXP i.MX6ULL. ; RK 3566;

![License](https://img.shields.io/badge/license-GPLv3.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-NXP%20i.MX6ULL;RK3566-green.svg)
![Kernel](https://img.shields.io/badge/kernel-Linux%204.9.88;Linux6.1-orange.svg)
![Status](https://img.shields.io/badge/status-Active-brightgreen.svg)

**Neo-Embedded-Linux** is a next-generation repository dedicated to embedded Linux learning and driver development. Unlike traditional tutorials, this project leverages **AI Agents ** to govern knowledge bases, standardize code quality, and accelerate the learning curve from "Hello World" to complex subsystem drivers.

## 🚀 Key Features

- **🧠 AI-Governed Knowledge Base**:

  **📘 Deep-Dive Documentation**:

- **⚡ Modern Workflow**

- **📂 Structured Engineering**

## 📂 Project Structure

| Directory | Description |
| :--- | :--- |
| `note/` | KnowLedges |
| `prj/` | Code Workspace |
| `sdk/` | Vendor SDK |
| `docs/` | **The PDF Reference.** |

## 🛠️ Getting Started 

### Prerequisites

- **Hardware**: NXP i.MX6ULL(100ASK_IMX6ULL_PRO); TSPI-1M RK3566.
- **SDK:**
  - [100ask-support BSP](./sdk/Setup-imx.md])
  - [TSPI RK3566 SDK](./sdk/Setup-tspi.md)
- **Host**: Linux (Ubuntu 22.04+) on Windows (WSL2).
- **Toolchain**: `arm-linux-gnueabihf-` (provided in i.MX6 SDK) / `aarch64-none-linux-gnu-` (provided in TSPI SDK).

### Start-Route

#### 0. SDK & Dev-Env

- Kernel pre-build & out-of-tree module flow → [`note/sdk/02_内核预编译与树外module开发流程.md`](./note/sdk/02_内核预编译与树外module开发流程.md)
- `compile_commands.json` via Bear → [`note/sdk/03_使用Bear生成编译数据库.md`](./note/sdk/03_使用Bear生成编译数据库.md)
- NFS rootfs mount → [`note/sdk/00_NFS挂载准备.md`](./note/sdk/00_NFS挂载准备.md)
- Kernel debug / klog → [`note/devp/kdebug.md`](./note/devp/kdebug.md), [`note/devp/klog-session.md`](./note/devp/klog-session.md)

#### 1. C Runtime & Process Virtualization

- Virtual process address space, user/kernel split → [`note/虚拟化/进程地址空间/00-进程地址空间.md`](./note/虚拟化/进程地址空间/00-进程地址空间.md)
- Page table & MMU → [`note/虚拟化/进程地址空间/02-进程地址空间-页表与MMU.md`](./note/虚拟化/进程地址空间/02-进程地址空间-页表与MMU.md), [`03-虚实之间：MMU的无感与负担.md`](./note/虚拟化/进程地址空间/03-虚实之间：MMU的无感与负担.md)
- vDSO / vvar → [`01-vDSO与vvar.md`](./note/虚拟化/进程地址空间/01-vDSO与vvar.md)
- ABI & linker config → [`note/虚拟化/ABI/01-ABI-ld-cfg.md`](./note/虚拟化/ABI/01-ABI-ld-cfg.md), [`混合编程-ABI.md`](./note/虚拟化/ABI/混合编程-ABI.md)
- ELF parsing demo → [`note/虚拟化/ABI/Demo-ELF地址空间解析.md`](./note/虚拟化/ABI/Demo-ELF地址空间解析.md)
- Observability tooling: gdb / objdump / perf → [`04-gdb-quickstart.md`](./note/虚拟化/进程地址空间/04-gdb-quickstart.md), [`05-elf-debug-info-deep-dive.md`](./note/虚拟化/进程地址空间/05-elf-debug-info-deep-dive.md), [`06-disassembly-and-decompilation.md`](./note/虚拟化/进程地址空间/06-disassembly-and-decompilation.md), [`07-linux-observability-tools.md`](./note/虚拟化/进程地址空间/07-linux-observability-tools.md)

#### 2. Process Lifecycle & IPC

- task_struct → [`note/虚拟化/程序和进程/01-进程控制块：task_struct-的解剖.md`](./note/虚拟化/程序和进程/01-进程控制块：task_struct-的解剖.md)
- fork / COW / exec / wait → [`03-进程创建的艺术：fork-与写时拷贝-(COW).md`](./note/虚拟化/程序和进程/03-进程创建的艺术：fork-与写时拷贝-(COW).md), [`04-进程生命周期管理：fork-exec-wait族详解.md`](./note/虚拟化/程序和进程/04-进程生命周期管理：fork-exec-wait族详解.md)
- exec → main 参数/envp 传递 → [`demo-exec与子程序main参数/`](./note/虚拟化/程序和进程/demo-exec与子程序main参数/)
- Process group / session / TTY-PTS → [`08-控制的边界：进程组与会话.md`](./note/虚拟化/程序和进程/08-控制的边界：进程组与会话.md), [`09-终端的虚幻与真实：TTY与PTS详解.md`](./note/虚拟化/程序和进程/09-终端的虚幻与真实：TTY与PTS详解.md)
- IPC overview → [`note/虚拟化/进程通信IPC/`](./note/虚拟化/进程通信IPC/) (pipe / signal / semaphore / mmap)

#### 3. SysCall & Char-Device IO

- Syscall panorama & table → [`note/SysCall/00-系统调用全景.md`](./note/SysCall/00-系统调用全景.md), [`01-系统调用表详解.md`](./note/SysCall/01-系统调用表详解.md)
- libc syscall wrapping → [`note/SysCall/libc原理-syscall封装.md`](./note/SysCall/libc原理-syscall封装.md)
- read/write panorama, ioctl, poll → [`note/SysCall/IO/01-read-write全景.md`](./note/SysCall/IO/01-read-write全景.md), [`02-ioctl范式.md`](./note/SysCall/IO/02-ioctl范式.md), [`03-poll机制详解.md`](./note/SysCall/IO/03-poll机制详解.md)
- IO paradigm summary → [`04-IO范式总览.md`](./note/SysCall/IO/04-IO范式总览.md)
- Advanced fops: poll & fasync → [`05-进阶fops实现：poll与fasync.md`](./note/SysCall/IO/05-进阶fops实现：poll与fasync.md)

#### 4. VFS & Device Model

- kobject → /sys, /dev, /proc → [`note/FS/VFS与设备模型/01-kobject.md`](./note/FS/VFS与设备模型/01-kobject.md)
- VFS deep-dive → [`2026-04-07-VFS-deep-dive.md`](./note/FS/VFS与设备模型/2026-04-07-VFS-deep-dive.md)
- Out-of-tree virtual cdev → [`pre-树外虚拟字符设备源码实现与VFS机制探讨.md`](./note/FS/VFS与设备模型/pre-树外虚拟字符设备源码实现与VFS机制探讨.md)

### Progress

#### DTS

- Usage: linux & devicetree, syntax, DTS→driver API, dtc/sysfs debug → [`note/DTS/Usage/`](./note/DTS/Usage/)
- Bindings (intro, common props, coding-style, YAML schema) → [`note/DTS/Bindings/`](./note/DTS/Bindings/)
- DT overlays → [`note/DTS/Overlays/01-dt-overlays.md`](./note/DTS/Overlays/01-dt-overlays.md)
- SoC-Arch (i.MX6ULL / Cortex-A) → [`note/SoC-Arch/00-Imx6ullArch.md`](./note/SoC-Arch/00-Imx6ullArch.md)

#### Kbuild

- Makefile / flags / Kconfig / external modules / kbuild architecture → [`note/Kbuild/`](./note/Kbuild/)

#### RTFS / Boot

- PC UEFI vs Embedded U-Boot chains → [`note/FS/RTFS与boot/PC_Boot_Process_Initramfs.md`](./note/FS/RTFS与boot/PC_Boot_Process_Initramfs.md), [`imx_Boot_Process_Direct.md`](./note/FS/RTFS与boot/imx_Boot_Process_Direct.md)
- initramfs → mount rootfs → [`Initramfs.md`](./note/FS/RTFS与boot/Initramfs.md), [`imx-initramfs.md`](./note/FS/RTFS与boot/imx-initramfs.md)
- /sbin/init → fstab; bdev vs mnt; NFS storage → [`VFS_and_Fstab_Analysis.md`](./note/FS/RTFS与boot/VFS_and_Fstab_Analysis.md), [`BDEVvs_MNT.md`](./note/FS/RTFS与boot/BDEVvs_MNT.md), [`Storage_Mounting_NFS.md`](./note/FS/RTFS与boot/Storage_Mounting_NFS.md)
- Buildroot rootfs → [`imx-buildroot-RTFS.md`](./note/FS/RTFS与boot/imx-buildroot-RTFS.md), [`Ubu-RootFS_Overview.md`](./note/FS/RTFS与boot/Ubu-RootFS_Overview.md)

#### Kernel Core

- Context (process / irq / softirq) → [`note/kernel/context/00-overview.md`](./note/kernel/context/00-overview.md)
- Interrupt: GIC hw → kernel framework → driver API → [`note/Subsystem/Interrupt/`](./note/Subsystem/Interrupt/)
- Defer: softirq / tasklet / workqueue / threaded-irq / waitqueue / completion → [`note/kernel/defer/`](./note/kernel/defer/)
- Time: jiffies/HZ, soft-timer, hrtimer, tick-nohz, i.MX6ULL GPT → [`note/kernel/time/`](./note/kernel/time/)
- Sync: spinlock, mutex/sem, cheatsheet → [`note/kernel/sync/`](./note/kernel/sync/)
- Sched overview → [`note/kernel/sched/00-overview.md`](./note/kernel/sched/00-overview.md)

#### Advanced IO

- non-block / multi-poll → [`note/SysCall/IO/03-poll机制详解.md`](./note/SysCall/IO/03-poll机制详解.md)
- signal-driven (fasync) / AIO → [`note/SysCall/IO/05-进阶fops实现：poll与fasync.md`](./note/SysCall/IO/05-进阶fops实现：poll与fasync.md), [`04-IO范式总览.md`](./note/SysCall/IO/04-IO范式总览.md)

#### Subsystems

- GPIO: overview / VFS impl / cdev example → [`note/Subsystem/gpio/`](./note/Subsystem/gpio/)
- BSP-Dev LCD/Touch (MIPI-DSI) → [`note/BSP-Dev/LCD-Touch/MIPI-DSI.md`](./note/BSP-Dev/LCD-Touch/MIPI-DSI.md)

### Advanced

## 📜 License

This project is licensed under the GPLv3.0 License - see the [LICENSE](LICENSE) file for details.

Copyright (c) 2026 @yceachan
