---
title: IMX Devp Environment Configuration
tags: [env, sdk, imx6ull, nfs]
desc: Setup instructions for the i.MX6ULL development environment.
update: 2026-04-07
---


# IMX Devp Environment

> [!note]
> **Ref:** [imx-env.sh](imx-env.sh)

## SDK Environment
To activate the cross-compilation toolchain and kernel source environment:
```bash
source ~/imx/imx-env.sh
```

## Hardware & Debug
Connect to the i.MX6ULL development board via SSH:
```bash
ssh imx
```

## NFS Mounting
The local project mount directory is already synchronized with the board's NFS:
- **Local:** `prj/mount/`
- **Board:** `/mnt/`

## Note Naming Convention (workspace rule)

- Notes under `note/**` MUST use `NN-topic.md` format (two-digit serial + short English topic).
- Chinese title goes in YAML frontmatter `title:`, **not** in the filename.
- Observation/trace/transcript notes use prefix `trail-` and are NOT part of the serial sequence.
- Canonical example: `note/SysCall/IO/` (see `plan/refact-20260409.md`).
