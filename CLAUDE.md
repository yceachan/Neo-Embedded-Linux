# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Purpose

**Neo-Embedded-Linux** — AI-assisted kernel-driver / system / app learning across **two** embedded platforms:

| Platform | SoC | Kernel | Toolchain |
|---|---|---|---|
| 100ask IMX6ULL EVB | NXP i.MX6ULL (Cortex-A7) | Linux 4.9.88 | `arm-buildroot-linux-gnueabihf-` |
| TSPI RK3566 | Rockchip RK3566 (Cortex-A55) | Linux 6.1 | `aarch64-none-linux-gnu-` |

The repo pairs a structured knowledge base (`note/`) with out-of-tree driver/app projects (`prj/`). Notes are the authoritative output; code under `prj/` exists to be observed and written up.

## Resource Map

| Directory | Role |
|---|---|
| `docs/` | NXP datasheets, reference manuals, 100ask handbook (read-only references) |
| `note/` | Governed learning notes — see knowledge map below |
| `prj/` | Out-of-tree kernel modules + user-space apps, per platform |
| `sdk/100ask_imx6ull-sdk/` | IMX6ULL BSP (kernel, U-Boot, Buildroot, toolchain) |
| `sdk/tspi-rk3566-sdk/` | TSPI BSP — **contains nested git repos** (see caveat) |

## Environment

Source the matching env script *before* building. They both export `KERN_DIR`, `ARCH`, `CROSS_COMPILE`, and extend `PATH`; `dump_armgcc` prints the active values.

| Task | IMX6ULL | TSPI RK3566 |
|---|---|---|
| Activate SDK | `source ~/imx/imx-env.sh` | `source ~/imx/tspi_env.sh` |
| SSH to board | `ssh imx` | `ssh tspi` (via RNDIS — fails if RNDIS down; fall back to serial) |
| NFS share | local `prj/nfs_imx/` ↔ board `/mnt/` | local `prj/nfs_tspi/` ↔ board `/mnt/nfs` |

`tspi_env.sh` also defines `syncImage` (rsyncs `$SDK_DIR/rockdev/` to a Windows host path for flashing); `$SDK_DIR` only exists in the TSPI env.

## TSPI SDK Caveat — Nested Git Repos

`sdk/tspi-rk3566-sdk/{kernel-6.1,u-boot,buildroot}` are **independent git repositories** nested inside this repo (not submodules). When assessing branch state, recent changes, or running `git` commands, you must `cd` into each nested repo separately — the outer `git status` will not see their commits. This is a common source of "progress looks empty but isn't" mistakes.

## Note Conventions

- Files under `note/**` ,using `NN-topic.md`. 
- Addting notes like trail, using Appropriate Prefix.`prefix-topic.md`
- Global Writing Documents Rules.

## Note Knowledge Map

Top-level subdirs of `note/` (learning roughly progresses top-to-bottom):

- `SoC-Arch/` — i.MX6ULL hardware: Cortex-A7, GICv2, memory interfaces
- `Kbuild/` — kernel build system: flags, Kconfig, external modules, kbuild architecture
- `DTS/` — Device Tree: `Usage/`, `mechanism/`, `Bindings/`, `Overlays/`, `evb/`
- `虚拟化/` — process virtualization: address space, MMU, vDSO, ABI, fork/exec/wait, IPC
- `SysCall/` — syscall panorama, table, libc wrapping, IO (`read/write`, `ioctl`, `poll`, advanced fops)
- `FS/` — VFS & device model (`kobject`, cdev, out-of-tree virtual char devices) + `RTFS与boot/` (initramfs, fstab, boot chain)
- `kernel/` — kernel core: `context/`, `defer/` (softirq/tasklet/wq), `time/`, `sync/`, `sched/`
- `Subsystem/` — `Interrupt/` (GIC → framework → driver API), `gpio/`
- `BSP-Dev/` — board bring-up (e.g. `LCD-Touch/MIPI-DSI`)
- `devp/` — dev-tooling: `kdebug`, `klog-session`, etc.
- `sdk/` — SDK workflow: out-of-tree module flow, Bear / `compile_commands.json`, NFS rootfs

## Project Workspace (`prj/`)

| Path | Purpose |
|---|---|
| `xx-<topic>/` | Driver/app demos, numbered to match the note progression |
| `dts_imx/` / `dts_tspi/` | Per-platform DTS overlays / experiments |
| `nfs_imx/` / `nfs_tspi/` | Per-platform NFS share roots (see Environment table) |
| `prj.code-workspace` / `tspi.code-workspace` | VS Code multi-root workspaces |

- `~/.ssh/config` to see evb host.
