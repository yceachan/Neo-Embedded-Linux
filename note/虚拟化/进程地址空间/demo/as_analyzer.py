#!/usr/bin/env python3
"""
as_analyzer.py — 进程地址空间分析器 (重写版)

读取 /proc/<pid>/maps 与 /proc/<pid>/mem，对每个 VMA 进行**分类**而不是简单罗列：
  - 主程序的 .text/.rodata/.data
  - [heap] / [stack] / [vdso] / [vvar] / [vsyscall]
  - 共享库 (so) 的代码段 / 只读段 / 数据段
  - 匿名 mmap 区 (大块 malloc / 线程栈 / JIT)

并支持:
  --locate ADDR1,ADDR2,...   把若干十六进制地址映射回所属 VMA（验证种子地址）
  --dump-vdso PATH           把 [vdso] 整段从 /proc/mem dump 出来 (证明它是 ELF)
  --check-wx                 扫描所有 wx 同时置位的页 (W^X 违规)

用法:
  sudo ./as_analyzer.py <PID>
  sudo ./as_analyzer.py <PID> --locate 0x...,0x...  --dump-vdso /tmp/vdso.so
"""
import argparse
import os
import sys
from collections import defaultdict

PSEUDO = {"[heap]", "[stack]", "[vdso]", "[vvar]", "[vsyscall]"}


def parse_maps(pid):
    vmas = []
    with open(f"/proc/{pid}/maps") as f:
        for line in f:
            parts = line.split(maxsplit=5)
            if len(parts) < 5:
                continue
            addr_range, perms, offset, dev, inode = parts[:5]
            path = parts[5].strip() if len(parts) == 6 else ""
            start_s, end_s = addr_range.split("-")
            start, end = int(start_s, 16), int(end_s, 16)
            vmas.append({
                "start": start, "end": end, "size": end - start,
                "perms": perms, "offset": int(offset, 16),
                "inode": int(inode), "path": path,
            })
    return vmas


def classify(v):
    """给一个 VMA 贴一个语义标签."""
    p = v["perms"]
    path = v["path"]

    if path in PSEUDO:
        return path[1:-1]                       # heap / stack / vdso / vvar / vsyscall
    if not path:                                # 匿名映射
        return "anon-rw" if "w" in p else "anon-ro"
    if path.endswith(".so") or ".so." in path:
        if "x" in p: return "lib-text"
        if "w" in p: return "lib-data"
        return "lib-rodata"
    # 来自 ELF 主程序的映射
    if "x" in p: return "exe-text"
    if "w" in p: return "exe-data"   # .data + .bss 通常合并
    return "exe-rodata"               # 含 .rodata 与 ELF header


def fmt_size(n):
    for u in ("B", "K", "M", "G"):
        if n < 1024:
            return f"{n}{u}"
        n //= 1024
    return f"{n}T"


def print_table(vmas):
    print(f"{'start':>14}  {'end':>14}  {'size':>6}  perms  {'class':<11}  path")
    print("-" * 90)
    for v in vmas:
        cls = classify(v)
        path = v["path"] or "[anon]"
        # 长路径只留 basename
        if path.startswith("/"):
            path = ".../" + os.path.basename(path)
        print(f"  {v['start']:012x}  {v['end']:012x}  {fmt_size(v['size']):>6}  "
              f"{v['perms']}  {cls:<11}  {path}")


def summarize(vmas):
    by_cls = defaultdict(lambda: [0, 0])  # cls -> [count, total_bytes]
    for v in vmas:
        c = classify(v)
        by_cls[c][0] += 1
        by_cls[c][1] += v["size"]
    print("\n--- summary by class ---")
    print(f"{'class':<12}  {'#vmas':>6}  {'total':>10}")
    total = 0
    for c, (n, sz) in sorted(by_cls.items(), key=lambda x: -x[1][1]):
        print(f"{c:<12}  {n:>6}  {fmt_size(sz):>10}")
        total += sz
    print(f"{'TOTAL':<12}  {'':>6}  {fmt_size(total):>10}")


def locate(vmas, addrs):
    print("\n--- locate addresses ---")
    for a in addrs:
        for v in vmas:
            if v["start"] <= a < v["end"]:
                cls = classify(v)
                off = a - v["start"]
                path = v["path"] or "[anon]"
                print(f"  0x{a:012x}  ->  {cls:<11}  {v['perms']}  +0x{off:x}  {path}")
                break
        else:
            print(f"  0x{a:012x}  ->  <UNMAPPED>")


def check_wx(vmas):
    print("\n--- W^X check ---")
    bad = [v for v in vmas if "w" in v["perms"] and "x" in v["perms"]]
    if not bad:
        print("  OK: no VMA is simultaneously writable and executable.")
    else:
        for v in bad:
            print(f"  VIOLATION: {v['perms']}  {v['path']}")


def dump_vdso(pid, vmas, out_path):
    vdso = next((v for v in vmas if v["path"] == "[vdso]"), None)
    if not vdso:
        print("  no [vdso] in this process")
        return
    with open(f"/proc/{pid}/mem", "rb") as f:
        f.seek(vdso["start"])
        data = f.read(vdso["size"])
    with open(out_path, "wb") as f:
        f.write(data)
    magic = data[:4]
    print(f"\n--- vdso dump ---")
    print(f"  range : 0x{vdso['start']:x}-0x{vdso['end']:x}  ({fmt_size(vdso['size'])})")
    is_elf = magic == b"\x7fELF"
    print(f"  first4: {magic.hex()}  ({'ELF' if is_elf else 'NOT ELF'})")
    print(f"  saved : {out_path}")
    print(f"  hint  : objdump -d {out_path}   # 现在能看到 __vdso_clock_gettime 等符号")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pid", type=int)
    ap.add_argument("--locate", default="",
                    help="comma-separated hex addresses to locate in VMAs")
    ap.add_argument("--dump-vdso", metavar="PATH", default="",
                    help="dump [vdso] from /proc/<pid>/mem to PATH")
    ap.add_argument("--check-wx", action="store_true")
    args = ap.parse_args()

    vmas = parse_maps(args.pid)
    print(f"=== address space of PID {args.pid} ({len(vmas)} VMAs) ===")
    print_table(vmas)
    summarize(vmas)

    if args.locate:
        addrs = [int(x, 16) for x in args.locate.split(",") if x.strip()]
        locate(vmas, addrs)
    if args.check_wx:
        check_wx(vmas)
    if args.dump_vdso:
        dump_vdso(args.pid, vmas, args.dump_vdso)


if __name__ == "__main__":
    main()
