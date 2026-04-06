---
title: i.MX6ULL Initramfs 状态与配置分析
tags: [imx6ull, initramfs, Kernel, Buildroot, SDK]
desc: 结合 100ask SDK 源码及实车验证，分析 i.MX6ULL 未使用 initramfs 的原因及如何开启。
update: 2026-04-07

---


# i.MX6ULL Initramfs 状态与配置分析

> [!note]
> **Ref**:
> 1. Board SSH: `cat /proc/cmdline` & `ls /boot`
> 2. SDK Source: `100ask_imx6ull-sdk/Linux-4.9.88/.config`


## 1. 实测结论：当前系统未使用 Initramfs

通过交叉验证开发板状态与 SDK 源码，确认当前的 i.MX6ULL 系统是**直接挂载真实根分区（直挂模式）**的。

### 1.1 板端验证 (`/proc/cmdline`)
```text
console=ttymxc0,115200 root=/dev/mmcblk1p2 rootwait rw
```
- 内核参数明确指示 `root=/dev/mmcblk1p2`，告知内核直接读取 SD 卡第二个分区。
- **未见** `initrd=` 或 `rdinit=` 参数。

### 1.2 源码验证 (`Linux-4.9.88/.config`)
```text
CONFIG_INITRAMFS_SOURCE=""
```
- 内核配置项为空。这意味着在编译 `zImage` 时，内核没有将任何 `cpio` 压缩包或根目录树“揉进”自己的镜像体内。


## 2. 为什么 i.MX6ULL 默认不开启？

在通用 Linux（如 Ubuntu PC）中，`initramfs` 是标配，因为内核不知道用户的硬盘是 NVMe 还是 SCSI，必须借助 `initramfs` 里的通用驱动去“摸盲”。

而在嵌入式 SDK（如百问网）中，默认关闭是基于以下考量：

1. **硬件高度确定**: 内核是为这块板子**专门定制编译**的，SD/eMMC 驱动已经被静态编译进了内核（Built-in，而不是模块化 `.ko`）。内核启动后就能直接认出硬盘。
2. **NFS 调试友好**: 开发阶段，内核经常通过 `root=/dev/nfs` 直接挂载网络文件系统。跳过 `initramfs` 能让调试过程更纯粹。
3. **节约资源**: 虽然 `initramfs` 存在于内存中，但对于只有 256M 或 512M RAM 的板子，省下这份内存更有价值。


## 3. 如何在 SDK 中开启 (Built-in Initramfs)?

如果你准备将产品固化发布（如打包为一个单文件镜像通过 U-Boot 烧写），开启嵌入式 `initramfs` 是一种极佳的防篡改方案。

### 步骤 1：准备 RootFS
使用 Buildroot 编译好你的最小根文件系统，它通常会输出一个 `rootfs.cpio` 或 `rootfs.tar` 文件。

### 步骤 2：修改内核配置
在 SDK 的 Linux 目录下执行：
```bash
make menuconfig
```
导航至：
`General setup` -> `[*] Initial RAM filesystem and RAM disk (initramfs/initrd) support`

在子菜单中找到 `Initramfs source file(s)`（即 `CONFIG_INITRAMFS_SOURCE`），填入你第一步生成的 `rootfs.cpio` 路径（或存放 RootFS 的文件夹路径）。

### 步骤 3：重新编译内核
```bash
make zImage
```
此时生成的 `zImage` 相比之前会**明显变大**，因为它已经把整个操作系统“吞”进肚子里了。

### 步骤 4：修改 U-Boot 启动参数
将启动参数从 `root=/dev/mmcblk...` 修改为：
```text
console=ttymxc0,115200 rdinit=/sbin/init
```
- 此时不需要 `root=` 了，因为根系统就在内核自己体内。
- `rdinit=` 告诉内核在解压完自身体内的 RAMFS 后，去执行哪个程序。


## 4. 总结

在当前的开发板状态下，系统处于**“无中间商赚差价”**模式：`Bootloader -> Kernel -> SD卡根分区`。理解这一点，有助于在排查“挂载失败”或“找不到 init” 等启动故障时，直接锁定真实的物理介质而非虚拟的 RAM 空间。
