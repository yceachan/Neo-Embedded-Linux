#!/usr/bin/env bash
# =============================================================================
#  tspi-greet-egl: 一键 demo 入口
#
#  链路（5 步）：
#    1) make clean
#    2) build (cross gcc + wayland-scanner)
#    3) 静态校验  —— file / readelf -d / pkg-config
#    4) deploy 到 NFS
#    5) 板上跑 + /proc/PID/{maps,fd} 取证 + GL_VENDOR/RENDERER 抓取 + 截图
#
#  退出码契约：
#    - 静态校验失败 → exit 2
#    - ssh tspi 不通 → 退化为仅本地校验通过即 exit 0 (与 tspi-greet-rs 对齐)
#    - ssh 通但远程跑挂 → exit 3
#    - 远程跑通但缺关键证据 (libmali / /dev/mali0 / GL_RENDERER 含 Mali) → exit 4
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p output/log output/bin
LOG_BUILD=output/log/build.log
LOG_VERIFY=output/log/verify.log
LOG_RUN=output/log/run.log

SR=/home/pi/imx/sdk/tspi-rk3566-sdk/buildroot/output/rockchip_rk3566_taishanpi_1m_v10/rockchip_rk3566
SYSROOT=$SR/host/aarch64-buildroot-linux-gnu/sysroot
READELF=$SR/host/bin/aarch64-buildroot-linux-gnu-readelf

# ── step 1 / 2 ──────────────────────────────────────────────────────────────
echo "============================================================"
echo "[test.sh] step 1/5  make clean"
echo "============================================================"
make clean 2>&1 | tee "$LOG_BUILD" >/dev/null

echo "============================================================"
echo "[test.sh] step 2/5  cross build (aarch64 + wayland-scanner)"
echo "============================================================"
./build.sh

BIN=output/bin/tspi-greet-egl
[ -f "$BIN" ] || { echo "[err] no binary"; exit 1; }

# ── step 3 静态校验 ─────────────────────────────────────────────────────────
echo "============================================================"
echo "[test.sh] step 3/5  static verification"
echo "============================================================"
{
    echo "## file"
    file "$BIN"
    echo
    echo "## readelf -d (dynamic deps)"
    "$READELF" -d "$BIN" | grep -E 'NEEDED|SONAME|RUNPATH|RPATH'
    echo
    echo "## pkg-config egl glesv2 wayland-egl (with sysroot env)"
    PKG_CONFIG_ALLOW_CROSS=1 \
    PKG_CONFIG_SYSROOT_DIR=$SYSROOT \
    PKG_CONFIG_LIBDIR=$SYSROOT/usr/lib/pkgconfig \
        pkg-config --cflags --libs egl glesv2 wayland-egl
    echo
    echo "## sysroot EGL/GLES .so list"
    ls -l "$SYSROOT/usr/lib/libEGL.so"* "$SYSROOT/usr/lib/libGLESv2.so"* \
         "$SYSROOT/usr/lib/libwayland-egl.so"* "$SYSROOT/usr/lib/libmali.so"* 2>/dev/null
} 2>&1 | tee "$LOG_VERIFY"

# 只在 NEEDED 段内 grep,避免 ls -l 行污染断言
NEEDED_BLOCK=$(sed -n '/^## readelf -d/,/^## /p' "$LOG_VERIFY")
grep -q 'aarch64' "$LOG_VERIFY"                       || { echo "[FAIL] binary not aarch64";              exit 2; }
echo "$NEEDED_BLOCK" | grep -q 'libmali.so.1'         || { echo "[FAIL] libmali not in NEEDED — sysroot 软链未收口到 blob"; exit 2; }
echo "$NEEDED_BLOCK" | grep -q 'libwayland-egl.so'    || { echo "[FAIL] libwayland-egl not in NEEDED";    exit 2; }
# 反向断言: 因为 sysroot 里 libEGL.so.1 / libGLESv2.so.2 都是 → libmali.so.1 的软链,
# SONAME 自动塌缩成 libmali.so.1 —— 我们 *期望* NEEDED 中没有它们
if echo "$NEEDED_BLOCK" | grep -qE 'libEGL\.so|libGLESv2\.so'; then
    echo "[WARN] NEEDED 中出现了 libEGL/libGLESv2 —— sysroot 软链未收口,与文档现象不一致"
fi

# ── step 4 deploy ───────────────────────────────────────────────────────────
echo "============================================================"
echo "[test.sh] step 4/5  deploy to NFS"
echo "============================================================"
./deploy.sh

# ── step 5 板上跑 + 取证 ─────────────────────────────────────────────────────
echo "============================================================"
echo "[test.sh] step 5/5  remote run + /proc evidence on TSPI"
echo "============================================================"
if ! timeout 35 ssh -o ConnectTimeout=30 -o BatchMode=yes tspi 'true' 2>/dev/null; then
    {
        echo "## ssh tspi unreachable — skipped remote run."
        echo "## local cross-build chain passed (see $LOG_VERIFY)."
    } | tee "$LOG_RUN"
    echo "[test.sh] DONE (local-only)."
    exit 0
fi

# 把整段远程脚本通过 stdin 喂给 bash -s
ssh tspi 'bash -s' <<'REMOTE' 2>&1 | tee "$LOG_RUN"
set +e
export XDG_RUNTIME_DIR=/run
WD=$(ls /run/wayland-* 2>/dev/null | head -1 | xargs -n1 basename || true)
export WAYLAND_DISPLAY="$WD"

echo "===env==="
echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
echo "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
echo "weston running: $(pgrep -a weston | head -1)"

echo
echo "===kill any pre-existing greet client (SHM 版同名 app-id 会抢占)==="
for p in tspi-greet tspi-greet-rs tspi-greet-egl; do
  pkill -TERM -f "$p" 2>/dev/null && echo "killed $p" || true
done
sleep 0.5

echo
echo "===launch tspi-greet-egl in background==="
nohup /mnt/nfs/tspi-greet-egl/tspi-greet-egl > /tmp/egl-stdout.log 2>&1 &
PID=$!
echo "spawned PID=$PID"
sleep 2

if ! kill -0 "$PID" 2>/dev/null; then
    echo
    echo "===CRASH==="
    cat /tmp/egl-stdout.log
    exit 11
fi

echo
echo "===PID $PID alive, scraping /proc evidence==="
echo "--- /proc/$PID/maps (mali / EGL / GLES / wayland-egl) ---"
grep -E "mali|libEGL|libGLES|libwayland-egl" /proc/$PID/maps | sort -u
echo
echo "--- /proc/$PID/fd (dri / mali) ---"
ls -l /proc/$PID/fd 2>/dev/null | grep -E "mali|dri" || echo "(no mali/dri fd!)"
echo
echo "--- ldd ---"
ldd /mnt/nfs/tspi-greet-egl/tspi-greet-egl

echo
echo "===screenshot via weston-screenshooter (best-effort)==="
if command -v weston-screenshooter >/dev/null 2>&1; then
    weston-screenshooter -o /tmp/tspi-greet-egl.png && ls -l /tmp/tspi-greet-egl.png || echo "screenshot failed"
else
    echo "(weston-screenshooter not installed)"
fi

sleep 2   # 让它再多渲染几帧
echo
echo "===killing PID $PID==="
kill -TERM "$PID" 2>/dev/null
sleep 0.5
kill -KILL "$PID" 2>/dev/null || true

echo
echo "===captured stdout (egl-stdout.log)==="
cat /tmp/egl-stdout.log
REMOTE

# 把截图（如果生成了）scp 回来
if ssh tspi 'test -f /tmp/tspi-greet-egl.png' 2>/dev/null; then
    scp -q tspi:/tmp/tspi-greet-egl.png output/log/screenshot.png || true
    echo "[ok] screenshot pulled → output/log/screenshot.png"
fi

# ── 关键证据断言 ─────────────────────────────────────────────────────────────
fail=0
grep -q 'libmali.so.1.9.0' "$LOG_RUN" || { echo "[FAIL] /proc/maps 未见 libmali.so.1.9.0 —— blob 未加载"; fail=1; }
grep -q -- '/dev/mali0'    "$LOG_RUN" || { echo "[FAIL] /proc/fd 未见 /dev/mali0 —— Mali 私有通道未开";   fail=1; }
grep -qiE 'GL_RENDERER.*(Mali|G52)' "$LOG_RUN" || { echo "[FAIL] stdout 未印 Mali GL_RENDERER";            fail=1; }
grep -qE 'frames=[1-9]'    "$LOG_RUN" || { echo "[FAIL] 渲染帧数 == 0";                                    fail=1; }

if [ $fail -ne 0 ]; then
    echo "============================================================"
    echo "[test.sh] FAIL — 关键证据缺失,见上方 FAIL 提示"
    echo "============================================================"
    exit 4
fi

echo
echo "============================================================"
echo "[test.sh] DONE.  All evidence captured."
echo "  build:  $LOG_BUILD"
echo "  verify: $LOG_VERIFY"
echo "  run:    $LOG_RUN"
[ -f output/log/screenshot.png ] && echo "  shot:   output/log/screenshot.png"
echo "============================================================"
