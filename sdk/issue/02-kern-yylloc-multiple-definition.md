---
title: "Host GCC ≥10: dtc 链接 yylloc 重复定义"
tags: [dtc, GCC, Linux-4.9, toolchain, issue]
desc: Host GCC ≥10 的 -fno-common 默认行为导致 Linux 4.9 内核树 scripts/dtc 链接失败。
update: 2026-04-11

---


# Host GCC ≥10: dtc 链接 yylloc 重复定义

> [!note]
> **Ref:** `scripts/dtc/dtc-lexer.lex.c_shipped:640` · `scripts/dtc/dtc-parser.tab.c_shipped:1199`

## Issue

| 项 | 值 |
|----|----|
| 环境 | Ubuntu 22.04 / WSL2, Host GCC 11.4, 100ask-BSP Linux-4.9.88 |
| 触发 | 任何需要编译 `scripts/dtc/dtc` 的目标：`make dtbs`、`make modules_prepare`、`make` |
| 影响 | Host 工具编译失败，内核本体与交叉编译产物不受影响 |

### 复现

```bash
source ~/imx/imx-env.sh
cd $KERN_DIR
make 100ask_imx6ull_defconfig
make dtbs
```

### 报错

```text
HOSTLD  scripts/dtc/dtc
/usr/bin/ld: scripts/dtc/dtc-parser.tab.o:(.bss+0x10): multiple definition of `yylloc';
             scripts/dtc/dtc-lexer.lex.o:(.bss+0x0): first defined here
collect2: error: ld returned 1 exit status
```

### 原因

GCC 10 起默认 `-fno-common`。flex 生成的暂定定义 `YYLTYPE yylloc;`（lexer）与 bison 生成的强定义 `YYLTYPE yylloc = {1,1,1,1};`（parser）在两个 `.o` 中各占一份 `.bss`，链接时冲突。旧版 GCC 默认 `-fcommon`，暂定定义进 COMMON 段由链接器合并，不报错。

`scripts/dtc/dtc` 是 Host 工具（`HOSTCC = gcc`），与交叉编译器 `arm-buildroot-linux-gnueabihf-gcc 7.5.0` 无关。

## Fix

### Fix A：patch `_shipped` 源文件（根治）

必须改 git 跟踪的 `_shipped` 文件。`dtc-lexer.lex.c` 是构建时从 `_shipped` 复制的产物，`make clean` 后修复丢失。

```diff
--- a/scripts/dtc/dtc-lexer.lex.c_shipped
+++ b/scripts/dtc/dtc-lexer.lex.c_shipped
@@ -640 +640 @@
-YYLTYPE yylloc;
+extern YYLTYPE yylloc;
```

改后所有 `make` 目标均不再需要额外 flag。

### Fix B：编译时传 flag（临时）

```bash
make HOSTCFLAGS="-fcommon" dtbs
make HOSTCFLAGS="-fcommon" modules_prepare
```

不改源码，但每次编译都须携带，遗漏即复现。
