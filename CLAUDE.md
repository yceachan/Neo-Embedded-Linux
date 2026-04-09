# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Purpose

**Neo-Embedded-Linux** — AI-assisted kernel driver learning and development for the NXP i.MX6ULL (Cortex-A7) embedded platform. This repo combines a structured knowledge base (`note/`) with working driver projects (`prj/`), governed by strict Agent CLI conventions.

## Resource Map

| Directory | Role |
|-----------|------|
| `docs/` | NXP datasheets, reference manuals, and the 100ask development handbook |
| `note/` | Agent-governed learning notes (Kbuild, DTS, SoC arch, VFS, SysCall, virtualization) |
| `prj/` | Out-of-tree kernel driver projects |
| `sdk/` | 100ask IMX6ULL BSP (Linux-4.9.88, U-Boot, Buildroot, toolchain) |

## Environment

| Task | Command / Path |
|------|----------------|
| **Activate SDK** | `source ~/imx/imx-env.sh` |
| **Link EVB** | `ssh imx` |
| **NFS Mount** | Local `prj/mount/` is mounted at EVB `/mnt/` |

## Note Naming Convention (workspace rule)

- Notes under `note/**` MUST use `NN-topic.md` format (two-digit serial + short English topic).
- Chinese title goes in YAML frontmatter `title:`, **not** in the filename.
- Observation/trace/transcript notes use prefix `trail-` and are NOT part of the serial sequence.
- See `note/SysCall/IO/plan/refact-20260409.md` for the canonical example of this layout.

## Note Knowledge Map

Learning progression in `note/`:

- `KernelLearning最佳实践.md` — entry point; methodology and phase plan
- `Kbuild/` — build system governance (8 files, phases 1–4)
- `DTS/` — Device Tree: usage model, syntax, bindings, DTS→driver mapping
- `SoC-Arch/` — IMX6ULL hardware architecture (Cortex-A7, GICv2, memory interfaces)
- `SysCall/` — System call deep-dive, syscall table, process address space, MMU
- `VFS/` — Virtual File System; out-of-tree character device source analysis
- `虚拟化/` — Process virtualization, IPC (pipe, signal), program execution
- `Legacy/` — Archived introductory notes (superseded)
