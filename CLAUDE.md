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

## Build Commands

### Environment Setup (required before building)

```bash
source imx-env.sh          # Sets KERN_DIR, ARCH=arm, CROSS_COMPILE, PATH for toolchain
dump_armgcc                # Verify toolchain config
```

### Driver Development (in `prj/01_hello_drv/`)

```bash
make                       # Cross-compile driver (.ko) + test app
make compile_commands      # Generate compile_commands.json for clangd/LSP
make clean                 # Remove build/ and output/ directories
```

Build outputs land in `output/`: `hello_drv.ko` (kernel module) and `hello_drv_test` (userspace test app).

### Deploy to Board

```bash
# SCP artifacts then on target:
insmod hello_drv.ko
./hello_drv_test
rmmod hello_drv
```

## Architecture: Out-of-Tree Shadow Build Pattern

`prj/01_hello_drv/` is the canonical driver template. It demonstrates a three-layer Makefile pattern:

1. **Wrapper Makefile** — copies source into `build/`, invokes `$(MAKE) -C $(KERN_DIR) M=$(PWD)/build`
2. **Kbuild file** — lists objects for the kernel build system
3. **Output collection** — `.ko` and test app moved to `output/`

Source directories (`src/`, `include/`, `app/`) stay pristine; `build/` is ephemeral.

## Note-Writing Conventions

All notes in `note/` must follow the Agent CLI protocol defined in `GEMINI.md`:

**YAML frontmatter** (every markdown file begins with this, no H1 before it):

```markdown
---
title: Document Title
tags: [tag1, tag2]
desc: One sentence description
update: YYYY-MM-dd

---


# H1 Title
```

**Mermaid rules:**
- Always quote node names containing `/`, `\`, `()`, `（）`, or Chinese characters: `Node["名称/path"]`
- Sequence diagrams must include `autonumber`
- `rect rgb(...)` fill colors: all RGB components > 200 (light background for black text)

**Language:** Final documentation in Chinese, preserving English technical terms. Persist high-value knowledge to files immediately rather than leaving it in context.

**References:** Cite source of truth (local code/notes/docs AND web) immediately below H1:

```markdown
> [!note]
> **Ref:** [kernel docs](url), [`sdk/Linux-4.9.88/...`](relative/path)
```

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
