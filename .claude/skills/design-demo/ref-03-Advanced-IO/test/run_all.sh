#!/bin/sh
# ==============================================================
# prj/03-Advanced-IO — 上板一键回归脚本
# 用法（在 EVB 上，/mnt/03-Advanced-IO 目录下）:
#   sh test/run_all.sh                  # 跑全部用例
#   sh test/run_all.sh block poll       # 仅跑指定用例
#   STRACE=1 sh test/run_all.sh         # 用 strace 追踪每个用例的系统调用
#   STRACE=1 STRACE_FILTER=read,write,poll,rt_sigaction,io_submit \
#       sh test/run_all.sh fasync       # 自定义过滤的 syscall 集合
# 约定: 脚本须在含 output/ 与 test/ 的项目根目录执行
# ==============================================================

set -u

# --- strace 包装 ---------------------------------------------
# STRACE=1 时, 每个用例下的测试程序通过 strace 启动, 输出落到
# /tmp/adv_strace_<case>.log, 同时把关键 syscall 摘要打印到屏幕
STRACE=${STRACE:-0}
STRACE_FILTER=${STRACE_FILTER:-read,write,openat,close,poll,ppoll,select,fcntl,ioctl,rt_sigaction,rt_sigreturn,kill,io_setup,io_submit,io_getevents,io_destroy}
STRACE_BIN=$(command -v strace 2>/dev/null || true)
if [ "$STRACE" = "1" ] && [ -z "$STRACE_BIN" ]; then
    echo "WARN: STRACE=1 but strace not found, falling back to plain run"
    STRACE=0
fi

# wrap_strace <case-tag> <cmd...>
wrap_strace() {
    _tag=$1; shift
    if [ "$STRACE" = "1" ]; then
        _slog=/tmp/adv_strace_${_tag}.log
        : > "$_slog"
        "$STRACE_BIN" -f -tt -s 96 -e trace="$STRACE_FILTER" -o "$_slog" "$@"
        _rc=$?
        echo "    -- strace summary ($_slog) --"
        # 打印 syscall 计数 + 末尾 8 行
        awk '{
            for (i=1;i<=NF;i++) if ($i ~ /^[a-z_0-9]+\(/) { sub(/\(.*/,"",$i); c[$i]++; break }
        } END { for (k in c) printf "      %-16s %d\n", k, c[k] }' "$_slog" | sort -k2 -nr | head -10 | sed 's/^/    /'
        tail -8 "$_slog" | sed 's/^/    | /'
        return $_rc
    else
        "$@"
    fi
}

DEV=/dev/adv_io
KO=./output/adv_io_drv.ko
BIN=./output
LOG=/tmp/adv_io_runall.log
: > "$LOG"

PASS=0
FAIL=0
SKIP=0

color() { printf '\033[%sm%s\033[0m' "$1" "$2"; }
ok()    { echo "  $(color '1;32' PASS) $1"; PASS=$((PASS+1)); }
ng()    { echo "  $(color '1;31' FAIL) $1"; FAIL=$((FAIL+1)); }
sk()    { echo "  $(color '1;33' SKIP) $1"; SKIP=$((SKIP+1)); }
hdr()   { echo; echo "== $(color '1;36' "$1") =="; }

# busybox 无 `timeout` → 用后台进程 + sleep + kill 模拟
run_to() {
    _to=$1; shift
    ( "$@" ) &
    _pid=$!
    ( sleep "$_to"; kill -TERM $_pid 2>/dev/null ) &
    _killer=$!
    wait $_pid 2>/dev/null
    kill $_killer 2>/dev/null
    wait $_killer 2>/dev/null
    return 0
}

# --- 加载模块 -------------------------------------------------
load_mod() {
    hdr "load module"
    if [ ! -f "$KO" ]; then ng "ko not found: $KO"; exit 2; fi
    lsmod | grep -q '^adv_io_drv' && rmmod adv_io_drv 2>/dev/null
    insmod "$KO" || { ng "insmod"; exit 2; }
    # 等 udev/mdev 建节点
    i=0; while [ ! -c "$DEV" ] && [ $i -lt 20 ]; do i=$((i+1)); sleep 0.1; done
    [ -c "$DEV" ] && ok "insmod + $DEV ready" || { ng "no $DEV"; exit 2; }
    dmesg | tail -3 | sed 's/^/    /'
}

unload_mod() {
    hdr "unload module"
    rmmod adv_io_drv 2>/dev/null && ok "rmmod" || ng "rmmod"
}

# --- 用例 -----------------------------------------------------
# 每个 case: 超时 N 秒内运行测试程序, 期望其自退出或被 kill 后产出预期字样

# 1. 阻塞 read: 2 秒内应至少收到 2 个字节 (producer 500ms/byte)
case_block() {
    hdr "case: blocking read"
    run_to 3 wrap_strace block "$BIN/test_block" >/tmp/adv_block.out 2>&1
    sed 's/^/    /' /tmp/adv_block.out
    n=$(grep -c . /tmp/adv_block.out 2>/dev/null); n=${n:-0}
    [ "$n" -ge 2 ] && ok "blocking read produced $n lines" \
                   || ng "blocking read produced $n lines (<2)"
}

# 2. 非阻塞: 专用小程序, O_NONBLOCK + 空 ring 期望 EAGAIN
case_nonblock() {
    hdr "case: non-blocking EAGAIN"
    if [ ! -x "$BIN/test_nonblock" ]; then sk "test_nonblock missing"; return; fi
    out=$(wrap_strace nonblock "$BIN/test_nonblock" 2>&1)
    echo "$out" | sed 's/^/    /'
    echo "$out" | grep -q EAGAIN && ok "EAGAIN on empty ring" \
                                 || ng "expected EAGAIN"
}

# 3. poll 多路复用
case_poll() {
    hdr "case: poll / POLLIN"
    run_to 3 wrap_strace poll "$BIN/test_poll" >/tmp/adv_poll.out 2>&1
    sed 's/^/    /' /tmp/adv_poll.out
    n=$(grep -c . /tmp/adv_poll.out 2>/dev/null); n=${n:-0}
    [ "$n" -ge 2 ] && ok "poll woke $n times" \
                   || ng "poll woke $n times (<2)"
}

# 4. SIGIO
case_fasync() {
    hdr "case: SIGIO / fasync"
    run_to 4 wrap_strace fasync "$BIN/test_fasync" >/tmp/adv_fasync.out 2>&1
    sed 's/^/    /' /tmp/adv_fasync.out
    n=$(grep -ci 'sigio\|got' /tmp/adv_fasync.out 2>/dev/null); n=${n:-0}
    [ "$n" -ge 2 ] && ok "SIGIO delivered $n times" \
                   || ng "SIGIO delivered $n times (<2)"
}

# 5. AIO
case_aio() {
    hdr "case: POSIX AIO"
    run_to 3 wrap_strace aio "$BIN/test_aio" >/tmp/adv_aio.out 2>&1
    sed 's/^/    /' /tmp/adv_aio.out
    grep -qiE 'completed|byte|aio_return' /tmp/adv_aio.out \
        && ok "aio completed" \
        || ng "aio did not complete"
}

# 6. 写回环: write 后阻塞 read 至少拿到 magic 里某个字节
case_write_read() {
    hdr "case: write-then-read"
    # drain producer stragglers
    run_to 1 dd if=$DEV of=/dev/null bs=256 count=1 >/dev/null 2>&1
    magic=$(printf 'Z%s' $(date +%s | tail -c 4))
    printf '%s' "$magic" > $DEV
    run_to 2 dd if=$DEV of=/tmp/adv_rr.out bs=64 count=1 >/dev/null 2>&1
    out=$(tr -d '\0' < /tmp/adv_rr.out)
    echo "    wrote=$magic  read=$out"
    # 由于 producer 也在注入随机字节, magic 与 producer 字节会混在一起
    # 只要 read 回来的内容包含 magic 的首字符 'Z' 即视为回环 OK
    echo "$out" | grep -q Z \
        && ok "write-read round trip" \
        || ng "round trip mismatch"
}

# --- 派发 -----------------------------------------------------
run_one() {
    case "$1" in
        block)    case_block;;
        nonblock) case_nonblock;;
        poll)     case_poll;;
        fasync)   case_fasync;;
        aio)      case_aio;;
        wr)       case_write_read;;
        *)        sk "unknown case: $1";;
    esac
}

load_mod
if [ $# -eq 0 ]; then
    for c in block nonblock poll fasync aio wr; do run_one $c; done
else
    for c in "$@"; do run_one "$c"; done
fi
unload_mod

echo
echo "============================================================"
echo " result: $(color '1;32' "PASS=$PASS")  $(color '1;31' "FAIL=$FAIL")  $(color '1;33' "SKIP=$SKIP")"
echo "============================================================"
[ "$FAIL" -eq 0 ]
