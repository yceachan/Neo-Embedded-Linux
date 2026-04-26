---
title: Tspi RK3566 SDK build.sh 分析
tags: [sdk, rk3566, tspi, build]
desc: 解析泰山派 (Tspi) RK3566 SDK 中 build.sh 的功能、参数以及多线程编译的支持情况。
update: 2026-04-26
---

# Tspi RK3566 SDK build.sh 分析

> [!note]
> **Ref:** [sdk/tspi-rk3566-sdk/build.sh](sdk/tspi-rk3566-sdk/build.sh)

基于对 `tspi-rk3566-sdk` 中 `build.sh` 及底层相关脚本的分析，以下是关于该 SDK 编译脚本的功能、编译选项以及多线程编译情况的详细总结：

## 1. `build.sh` 功能解析
`build.sh` 是瑞芯微 (Rockchip) 体系 SDK 的顶层统一编译入口。它的核心设计并不直接包含具体每个模块的编译命令，而是**基于 Hook（钩子）机制**进行任务调度。
它主要负责：
*   **环境初始化**：构建统一的目录树变量（`RK_OUTDIR`、`RK_SDK_DIR` 等）。
*   **分发构建阶段**：自动依次执行 `init` -> `pre-build` -> `build` -> `post-build` 四个阶段的底层脚本（位于 `device/rockchip/common/scripts/` 中）。
*   **统一接口**：提供了一套非常规范的单独编译、镜像打包与调试指令。

## 2. 编译参数与选项 (全编译 / 单独编译)
可以通过 `./build.sh help` 或者直接阅读脚本解析出的指令列表来调用不同的功能。

**① 全局配置阶段**
*   `./build.sh <板级名称>_defconfig`：应用特定的设备配置文件（例如在编译前切换 Board 级配置）。
*   `./build.sh config`：唤出图形界面对 SDK 顶层特性进行裁剪配置。

**② 全编译选项**
*   `./build.sh all`：执行全编译。依次编译 U-Boot、Kernel、Rootfs、Recovery 并最终打包生成完整的固件（如 `update.img`）。

**③ 模块化单独编译（常用选项）**
*   **引导程序**：`./build.sh uboot` 或 `./build.sh loader`（编译并生成 U-Boot / miniloader 镜像）。
*   **内核**：`./build.sh kernel`（编译生成 `boot.img` / `kernel.img`）。
*   **文件系统**：`./build.sh rootfs`（按默认配置生成根文件系统），也可单独指定 `./build.sh buildroot` 或 `./build.sh debian`。
*   **清理模块**：`./build.sh cleanall` (清理所有产物)；或针对性清理如 `./build.sh clean-kernel`。

**④ Buildroot 专项配置与环境**
在修改 Buildroot 包（如添加 `nfs-utils`）或进行底层调试时，必须使用以下命令以确保环境变量（如 `O=` 输出目录）正确：
*   `./build.sh buildroot-config` (alias: `bconfig`)：
    *   **用途**：弹出 Buildroot 的 `menuconfig` 图形配置界面。
    *   **核心逻辑**：自动加载 SDK 指定的 output 目录和交叉编译链环境。
*   `./build.sh buildroot-shell` (alias: `bshell`)：
    *   **用途**：进入一个已经初始化好所有 Buildroot 环境变量的子 Shell。
    *   **场景**：进入后可以直接运行 `make`、`make <pkg>-rebuild` 等原生指令，而无需担心路径错误。

## 3. 核心 Target 深入解析

`build.sh` 的 Target 可以分为 **配置 (Config)**、**构建 (Build)**、**辅助 (Auxiliary)** 三大类：

### ① 配置类 (Configuration)
这些命令决定了“编什么”以及“怎么编”：
*   **`defconfig[:<config>]`**：核心中的核心。不带参数时列出所有可用配置；带参数时（如 `rockchip_rk3566_taishanpi_1m_v10_defconfig`）会切换整个 SDK 的软硬件适配层，包括分区表、内核 DTS 名、Bootloader 配置等。
*   **`config-usb-gadget` / `config-wifibt`**：专项快速配置。这些 Target 会调用专用的配置脚本，用于快速切换 USB 模式（Host/Device）或 Wi-Fi 模组型号，无需手动去改复杂的 DTS。
*   **`config-rootfs-overlay`**：管理文件系统覆盖层。如果项目有多个 Overlay 目录，可以通过此命令进行动态管理。

### ② 构建类 (Build & Packaging)
*   **`kernel[:dry-run]`**：编译内核。`dry-run` 模式非常有用，它仅打印出将要执行的完整 `make` 命令，而不实际执行，方便开发者排查交叉编译链路径或宏定义问题。
*   **`buildroot-make[:<arg1>:<arg2>]` (alias: `bmake`)**：**进阶高频命令**。
    *   用法示例：`./build.sh bmake nfs-utils-rebuild`。
    *   作用：直接向 Buildroot 传递参数。当你只想重新编译某一个包（如 `nfs-utils`）而不是整个 RootFS 时，这是最快的方法。
*   **`updateimg` / `ota-updateimg`**：
    *   `updateimg`：将当前 `rockdev/` 目录下的所有零散分区镜像（`boot.img`, `rootfs.img` 等）打包成一个统一的 `.img` 刷机包。
    *   `ota-updateimg`：生成用于 A/B 分区或 Recovery 模式下在线升级的 OTA 包。

### ③ 辅助与开发类 (Development & Debug)
*   **`shell` / `buildroot-shell`**：
    *   `shell`：进入 SDK 顶层开发环境，加载所有的 `RK_*` 环境变量。
    *   `buildroot-shell`：进入 Buildroot 特定的 chroot 或交叉编译环境。在解决链接库冲突或手动调试 `Makefile` 时非常有用。
*   **`post-rootfs <dir>`**：手动触发后处理钩子。如果你手动修改了 `rootfs` 目录里的文件，可以运行此 Target 来重新执行权限校验、剥离符号表（strip）等操作，而无需重新编译整个 RootFS。

## 4. 关于 `-j` 多线程编译的支持情况
**结论：不需要、也不能在 `./build.sh` 后面手动添加 `-jN` 参数。** SDK 底层已经**自动做到了满线程并发编译**。

如果在命令行输入类似 `./build.sh kernel -j16`，`build.sh` 会将其识别为“未知的模块指令”并抛出错误。它的多线程机制是在各子模块底层脚本里自动适配的：

*   **Kernel 编译机制** (参考 `device/rockchip/common/scripts/kernel-helper`)：
    底层硬编码了内核的 Make 命令，使用了系统 `nproc` 命令动态探测 CPU 核心数并默认开启核心数 + 1 的线程：
    `export KMAKE="make -C "$RK_SDK_DIR/kernel/" -j$(( $(nproc) + 1 )) ..."`
*   **U-boot 编译机制** (参考 `u-boot/make.sh`)：
    底层通过解析 `cpuinfo` 文件获得处理器的总逻辑核心数，并传递给 Make 工具：
    `JOB=$(sed -n "N;/processor/p" /proc/cpuinfo|wc -l)`
    `make --jobs=${JOB}`
*   **Rootfs (Buildroot等) 机制**：
    底层依靠 Buildroot 自身系统探测机制（通常通过配置 `BR2_JLEVEL=0`），自动使用最佳的核心数量。

**总结：** 开发过程中只需直接执行 `./build.sh kernel` 或 `./build.sh all` 等指令即可，它已经处于性能最佳化（满 CPU 核心调度）的编译状态了。
