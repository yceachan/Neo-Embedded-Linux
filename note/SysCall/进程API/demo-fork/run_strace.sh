#!/bin/sh
# run_strace.sh — 构建 fork4 并用 strace 观察其 fork/clone/write/exit 系统调用
# 用法:
#   sh run_strace.sh                # 默认跑 fork4
#   sh run_strace.sh fork4          # 指定程序名 (output/<name>)
#   FILTER=clone,write,exit_group sh run_strace.sh fork4
set -eu

PROG=${1:-fork4}
BIN=output/$PROG
LOG=output/${PROG}.strace.log
FILTER=${FILTER:-clone,fork,vfork,write,exit,exit_group,wait4,getpid}

cd "$(dirname "$0")"
make -s "$BIN"

OUT=output/${PROG}.stdout.log

echo "== program output (-> $OUT) =="
# 先 wait 把所有孤儿进程的 stdout 收齐,再展示文件
( "./$BIN" ) >"$OUT" 2>&1
wait
sort "$OUT"   # 按 pid 排序,顺序稳定; 想看原顺序去掉 sort

echo
echo "== strace ($FILTER) -> $LOG =="
( strace -f -tt -s 96 -e trace="$FILTER" -o "$LOG" "./$BIN" ) >/dev/null 2>&1
wait
echo "-- $(wc -l <"$LOG") lines logged --"

echo
echo "== syscall summary =="
awk '{
    for (i=1;i<=NF;i++) if ($i ~ /^[a-z_0-9]+\(/) { sub(/\(.*/,"",$i); c[$i]++; break }
} END { for (k in c) printf "  %-14s %d\n", k, c[k] }' "$LOG" | sort -k2 -nr
