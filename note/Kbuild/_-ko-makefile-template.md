---
title: Makefile 模板 — 自动发现源文件 + 输出到 output/
tags: [makefile, build, gnu-make, pattern-rule, order-only]
desc: 一份用于 demo 类 C 项目的最小 Makefile,逐行讲清 wildcard/patsubst/模式规则/order-only 依赖等关键机制
update: 2026-04-08
---


# Makefile 模板 — 自动发现源文件 + 输出到 output/

> [!note]
> **Ref:** `note/SysCall/进程API/demo-fork/Makefile` (本模板的实际使用现场)

适用场景:一个目录里有多份独立的 `*.c` demo,希望:
- 加新文件**零修改** Makefile
- elf 全部输出到 `output/`,源码目录保持干净
- 支持 host / cross 编译切换
- `make clean` 一行搞定

## 源文件

```makefile
# demo-fork — host build (x86_64), elf 输出到 output/
CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra
OUTDIR  := output

SRCS    := $(wildcard *.c)
BINS    := $(patsubst %.c,$(OUTDIR)/%,$(SRCS))

.PHONY: all clean run
all: $(BINS)

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/%: %.c | $(OUTDIR)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OUTDIR)
```

---

## 逐行讲解

```makefile
CC      ?= gcc
```
**`?=`** 是"如果未定义才赋值"。允许外部覆盖,例如 `make CC=clang` 或交叉编译 `make CC=arm-linux-gnueabihf-gcc`。普通 `=` 会强制覆盖外部传入。

```makefile
CFLAGS  ?= -O0 -g -Wall -Wextra
```
默认编译选项,同样允许外部覆盖。
- `-O0`:不优化,方便 gdb 单步,变量不被寄存器化
- `-g`:生成 DWARF 调试信息
- `-Wall -Wextra`:开主流警告集

```makefile
OUTDIR  := output
```
**`:=`** 是"立即展开"赋值(简单变量)。与 `=` (递归展开) 相对。这里是常量,用 `:=` 更高效、无副作用。

```makefile
SRCS    := $(wildcard *.c)
```
`$(wildcard pattern)` 是 make 内建函数,展开当前目录所有 `.c` 文件 → 例如 `fork4.c`。**这一步让 Makefile 自动发现新源文件**,不需要每加一个 demo 就改 Makefile。

```makefile
BINS    := $(patsubst %.c,$(OUTDIR)/%,$(SRCS))
```
`$(patsubst pattern,replacement,text)` 是模式替换。把 `fork4.c` → `output/fork4`。结果 `BINS = output/fork4`(以及未来 `output/forkN`)。这是后续 `all:` 目标依赖的列表。

```makefile
.PHONY: all clean run
```
**`.PHONY`** 声明这些名字**不是文件**,而是逻辑动作。否则如果当前目录恰好存在叫 `clean` 的文件,`make clean` 会因为"目标已是最新"而什么都不做。

```makefile
all: $(BINS)
```
默认目标(Makefile 第一个非 `.` 开头的目标即默认)。依赖展开为 `all: output/fork4`,触发下面的模式规则去构建每个 elf。

```makefile
$(OUTDIR):
	mkdir -p $@
```
一条**目录创建规则**。`$@` 是自动变量 = 当前目标名 (`output`)。`-p` 保证目录已存在时不报错。

```makefile
$(OUTDIR)/%: %.c | $(OUTDIR)
	$(CC) $(CFLAGS) $< -o $@
```
核心**模式规则**(pattern rule),把任意 `xxx.c` 编成 `output/xxx`:
- `%` 是通配,匹配 `fork4` 这种 stem
- `$(OUTDIR)/%: %.c` 表示目标 `output/fork4` 依赖源文件 `fork4.c`
- **`| $(OUTDIR)`** 中的 `|` 是 **order-only prerequisite**(仅顺序依赖)。意思是"`output/` 目录必须存在,但它的时间戳变化**不会**触发重新编译"。如果不加 `|`,每次 `mkdir` 后目录 mtime 更新,会让所有 elf 都被错误地认为过期而重编。
- `$(CC) $(CFLAGS) $< -o $@`:`$<` = 第一个依赖 (`fork4.c`),`$@` = 目标 (`output/fork4`)。展开后就是 `gcc -O0 -g -Wall -Wextra fork4.c -o output/fork4`。
- 注意配方行**必须以 TAB 开头**(make 的硬约定),不能用空格。

```makefile
clean:
	rm -rf $(OUTDIR)
```
直接干掉整个 `output/`,因为所有产物都集中在那一个目录。比逐个 `rm` 干净也安全(没有误删源码的风险)。

---

## 设计要点回顾

| 机制 | 目的 |
|---|---|
| `?=` 三处 (CC/CFLAGS) | 允许外部覆盖,方便切换 host/cross 编译 |
| `wildcard + patsubst` | 加新 demo 零修改 Makefile |
| 模式规则 `output/%: %.c` | 一条规则编译所有源文件 |
| `\| $(OUTDIR)` order-only | 目录依赖不污染产物时间戳 |
| `.PHONY` | 防止与同名文件冲突 |
| 全部产物入 `output/` | `clean` 一行搞定,源码目录干净 |
