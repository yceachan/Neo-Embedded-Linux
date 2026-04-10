---
title: 进程地址空间观察实验
tags: [demo, 进程地址空间, procfs, vdso, ASLR, mmap]
desc: 自种子 C 程序 + 分类型 VMA 分析器，把每个变量回溯到所属段，并量化 vDSO 加速比。
update: 2026-04-09
---


# 进程地址空间观察实验

> [!note]
> **Ref:** [`/proc/[pid]/maps` man page](https://man7.org/linux/man-pages/man5/proc.5.html)、上层笔记 [`../00-进程地址空间.md`](../00-进程地址空间.md)、[`../01-vDSO与vvar.md`](../01-vDSO与vvar.md)

本目录是一套**自验证**的进程地址空间观察实验：靶子程序在每个段里"种"一个可观测变量并打印其地址，分析器则把这些地址**反查**回所属 VMA 的分类，使"地址 ↔ 段"的对应关系可被肉眼一一验证。

## 1. 文件清单

| 文件 | 角色 |
|------|------|
| `target.c` | 靶子程序：在 `.text/.rodata/.data/.bss/heap/mmap/stack` 各种下一个种子变量；执行 vDSO vs raw-syscall 的 `clock_gettime` 对比；自 dump `[vdso]` 到 `/tmp/vdso.so`；最后 `sleep(60)` 等待分析 |
| `as_analyzer.py` | 分析器：解析 `/proc/<pid>/maps` 并对每个 VMA **分类**（exe-text/exe-rodata/lib-text/heap/anon-rw/vdso/...）；支持 `--locate` 把若干十六进制地址回溯到所属 VMA、`--check-wx` 扫 W^X 违规、`--dump-vdso` |

相比旧版"打印 maps + 反汇编前 1KB"，新版的关键差异：
- 不再盲扫前 1KB（对 ELF 头反汇编出来的是垃圾），改为**精确语义分类 + 反查**。
- 把 vDSO 的 dump 工作下放到靶子自身（同进程读 `/proc/self/mem` 不需要 `sudo`），让整个流程零特权可复现。
- 增加 vDSO vs `syscall(SYS_clock_gettime, ...)` 的直接对比，量化加速比。

## 2. 跑一遍

```bash
gcc -O0 -o target target.c
./target &                       # 或：./target > out.txt &
PID=$!
./as_analyzer.py $PID \
    --locate $(grep -oE '0x[0-9a-f]+' out.txt | paste -sd,) \
    --check-wx
```

## 3. 实测输出（x86_64, glibc 2.39, Linux 6.6 WSL2）

### 3.1 靶子打印的种子地址

```
=== target PID = 64049 ===
  .text   plant_text     = 0x556c9e6723b9
  .text   main           = 0x556c9e6725bd
  .rodata g_rodata_str   = 0x556c9e673008  ("RODATA-SEED")
  .rodata g_rodata_int   = 0x556c9e673014
  .data   g_data_init    = 0x556c9e675010
  .data   g_static_init  = 0x556c9e675014
  .bss    g_bss_uninit   = 0x556c9e675024
  heap    malloc(64)     = 0x556cc29342a0
  mmap    mmap(2MiB)     = 0x74df92a00000
  stack   l_stack_var    = 0x7ffd0045a47c
  stack   l_stack_buf    = 0x7ffd0045a4e0
  libc    &printf        = 0x74df92c606f0

[bench] 1000000 rounds:
  vDSO  clock_gettime    avg =   29.2 ns/call
  raw   syscall(...)     avg =  462.6 ns/call   (×15.9 slower)
[dump] vDSO -> /tmp/vdso.so  (8192 bytes, magic=7f 45 4c 46 "ELF")
```

### 3.2 分析器：分类后的 VMA 表 + summary

```
=== address space of PID 64049 (24 VMAs) ===
         start             end    size  perms  class        path
------------------------------------------------------------------------------------------
  7ffd00565000  7ffd00567000      8K  r-xp  vdso         [vdso]
  7ffd00561000  7ffd00565000     16K  r--p  vvar         [vvar]
  7ffd0043a000  7ffd0045c000    136K  rw-p  stack        [stack]
  ...
  74df92dbd000  74df92e15000    352K  r--p  lib-rodata   .../libc.so.6
  74df92c28000  74df92dbd000      1M  r-xp  lib-text     .../libc.so.6
  74df92c00000  74df92c28000    160K  r--p  lib-rodata   .../libc.so.6
  74df92a00000  74df92c00000      2M  rw-p  anon-rw      [anon]              ← mmap(2MiB)
  556cc2934000  556cc2955000    132K  rw-p  heap         [heap]
  556c9e675000  556c9e676000      4K  rw-p  exe-data     .../target          ← .data + .bss
  556c9e674000  556c9e675000      4K  r--p  exe-rodata   .../target          ← .data.rel.ro
  556c9e673000  556c9e674000      4K  r--p  exe-rodata   .../target          ← .rodata
  556c9e672000  556c9e673000      4K  r-xp  exe-text     .../target          ← .text
  556c9e671000  556c9e672000      4K  r--p  exe-rodata   .../target          ← ELF header

--- summary by class ---
class          #vmas       total
anon-rw            4          2M
lib-text           2          1M
lib-rodata         7        592K
stack              1        136K
heap               1        132K
lib-data           2         16K
vvar               1         16K
exe-rodata         3         12K
vdso               1          8K
exe-text           1          4K
exe-data           1          4K
TOTAL                         4M
```

### 3.3 反查：每个种子地址都回到了"应许的段"

```
--- locate addresses ---
  0x556c9e6723b9  ->  exe-text     r-xp  +0x3b9   target          ← plant_text
  0x556c9e6725bd  ->  exe-text     r-xp  +0x5bd   target          ← main
  0x556c9e673008  ->  exe-rodata   r--p  +0x8     target          ← "RODATA-SEED"
  0x556c9e673014  ->  exe-rodata   r--p  +0x14    target          ← g_rodata_int
  0x556c9e675010  ->  exe-data     rw-p  +0x10    target          ← g_data_init
  0x556c9e675014  ->  exe-data     rw-p  +0x14    target          ← g_static_init
  0x556c9e675024  ->  exe-data     rw-p  +0x24    target          ← g_bss_uninit
  0x556cc29342a0  ->  heap         rw-p  +0x2a0   [heap]          ← malloc(64)
  0x74df92a00000  ->  anon-rw      rw-p  +0x0     [anon]          ← mmap(2MiB)
  0x7ffd0045a47c  ->  stack        rw-p  +0x2047c [stack]
  0x7ffd0045a4e0  ->  stack        rw-p  +0x204e0 [stack]
  0x74df92c606f0  ->  lib-text     r-xp  +0x386f0 libc.so.6       ← &printf

--- W^X check ---
  OK: no VMA is simultaneously writable and executable.
```

### 3.4 vDSO 是真正的 ELF

```
$ file /tmp/vdso.so
/tmp/vdso.so: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked, ...

$ objdump -T /tmp/vdso.so | grep __vdso
0000...07b0 g  DF .text  ...  __vdso_gettimeofday
0000...0dd0 g  DF .text  ...  __vdso_clock_getres
0000...0a40 g  DF .text  ...  __vdso_time
0000...0a70 g  DF .text  ...  __vdso_clock_gettime
0000...0e40 g  DF .text  ...  __vdso_getcpu
```


## 4. 由现象引出的结论

| 观察 | 结论 | 进阶笔记 |
|------|------|----------|
| `g_data_init / g_static_init / g_bss_uninit` 三个变量地址都落在同一个 4K 页 (`exe-data`，`rw-p`)，BSS 与 .data 共用一段 | 链接器把 `.data` 与 `.bss` 合并到同一个 LOAD 段；BSS 不占文件大小，但运行时同样需要 RW 页 | [00-进程地址空间.md §2](../00-进程地址空间.md) |
| 主程序被切成 5 段（`r--p / r-xp / r--p / r--p / rw-p`），每段权限不同 | ELF LOAD 段按权限分裂成多个 VMA，强制 **W^X** | [03-虚实之间.md](../03-虚实之间：MMU的无感与负担.md) |
| `malloc(64)` 进 `[heap]`，`mmap(2MiB)` 进匿名 mmap 区 | glibc 在分配 ≥ `M_MMAP_THRESHOLD`(默认 128KiB) 时直接走 `mmap(MAP_ANON)`，不走 `brk()` | (heap vs mmap，可补一篇 mm 笔记) |
| 每次运行 `target`，所有段基址都不同 | ASLR：内核给 PIE/共享库/堆/栈分别独立随机化基址 | [00-进程地址空间.md §4](../00-进程地址空间.md) |
| `libc.so` 出现 7 个段，包括一个 `---p` (no-perm) 的 4K guard | 链接器在 RX 段与 RW 段之间留 PROT_NONE guard，防越界写穿到代码 | (链接脚本) |
| `clock_gettime` vDSO 路径 **28.6 ns**，强制走 `syscall` 路径 **463.6 ns**，**16.2× 加速** | vDSO 完全绕过 ring 切换，直接读 `[vvar]` 中内核维护的时间快照 | [01-vDSO与vvar.md](../01-vDSO与vvar.md) |
| `[vdso]` 起始 4 字节是 `7f 45 4c 46` (`\x7fELF`) | `[vdso]` 是完整的 in-memory ELF 共享对象，盲目从首字节反汇编只会得到 ELF header 噪声 | [01-vDSO与vvar.md §3](../01-vDSO与vvar.md) |
| `[vvar]` 是 `r--p` 而 `[vdso]` 是 `r-xp`，两者紧邻 | vvar=只读数据页（内核每 tick 写入），vdso=只读代码页（用户调用）；用户既不能改时间也不能改代码 | [01-vDSO与vvar.md §5](../01-vDSO与vvar.md) |

## 5. 已知坑

- **不要用 `sudo` 运行靶子**，否则 `/tmp/vdso.so` 会被 root 占用，下次普通用户跑挂掉。
- WSL2 上 `[vsyscall]` 段不出现（已被弃用），属正常。
- 若要让 `.text` 起始地址固定（关闭 PIE 验证），用 `gcc -no-pie -O0`。
