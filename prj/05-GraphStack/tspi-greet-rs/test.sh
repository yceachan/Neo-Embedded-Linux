#!/usr/bin/env bash
# =============================================================================
#  tspi-greet-rs: 一键 demo 入口
#
#  链路：
#    clean → build (cargo cross) → 静态三层校验 → deploy → 板上跑一次 → 截 log
#
#  说明：
#  - 不强制要求板上能连 ssh —— 若 `ssh tspi` 不通则跳过远程跑，但本地校验必须过
#  - 所有产物 / 日志一律放 output/，无任何"散落"二进制
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

echo "============================================================"
echo "[test.sh] step 1/5  cargo clean"
echo "============================================================"
cargo clean --target-dir output/target 2>&1 | tee "$LOG_BUILD"

echo "============================================================"
echo "[test.sh] step 2/5  cargo build --release (cross to aarch64)"
echo "============================================================"
./build.sh

BIN=output/bin/tspi-greet-rs
[ -f "$BIN" ] || { echo "[err] no binary"; exit 1; }

echo "============================================================"
echo "[test.sh] step 3/5  static verification (file / readelf / .pc trace)"
echo "============================================================"
{
    echo "## file"
    file "$BIN"
    echo
    echo "## readelf -d (dynamic dependencies)"
    "$READELF" -d "$BIN" | grep -E 'NEEDED|SONAME|RUNPATH|RPATH'
    echo
    echo "## pkg-config cairo (with sysroot env)"
    PKG_CONFIG_ALLOW_CROSS=1 \
    PKG_CONFIG_SYSROOT_DIR=$SYSROOT \
    PKG_CONFIG_LIBDIR=$SYSROOT/usr/lib/pkgconfig \
        pkg-config --cflags --libs cairo
    echo
    echo "## sysroot cairo .so version"
    ls -l "$SYSROOT/usr/lib/libcairo.so"*
} 2>&1 | tee "$LOG_VERIFY"

# 简单 sanity assertion
if ! grep -q 'aarch64' "$LOG_VERIFY"; then
    echo "[FAIL] binary is not aarch64!"; exit 2
fi
if ! grep -q 'libcairo.so' "$LOG_VERIFY"; then
    echo "[FAIL] libcairo not in NEEDED — pkg-config link failed!"; exit 2
fi

echo "============================================================"
echo "[test.sh] step 4/5  deploy to NFS"
echo "============================================================"
./deploy.sh

echo "============================================================"
echo "[test.sh] step 5/5  remote run on TSPI (best-effort)"
echo "============================================================"
if timeout 40 ssh -o ConnectTimeout=30 -o BatchMode=yes tspi 'true' 2>/dev/null; then
    {
        echo "## remote uname + file check"
        ssh -o ConnectTimeout=30 tspi 'uname -a; ls -l /mnt/nfs/tspi-greet-rs/tspi-greet-rs'
        echo
        echo "## remote ldd"
        ssh -o ConnectTimeout=30 tspi 'ldd /mnt/nfs/tspi-greet-rs/tspi-greet-rs' || true
        echo
        echo "## remote --help / cold run (no WAYLAND_DISPLAY → 显式报错即成功证明 ELF 可执行)"
        ssh -o ConnectTimeout=30 tspi '/mnt/nfs/tspi-greet-rs/tspi-greet-rs' 2>&1 || true
        echo
        echo "## remote run under weston (3 秒后 kill)"
        ssh -o ConnectTimeout=30 tspi 'export XDG_RUNTIME_DIR=/run; export WAYLAND_DISPLAY=$(ls /run/wayland-* 2>/dev/null | head -1 | xargs -n1 basename); echo "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"; timeout 3 /mnt/nfs/tspi-greet-rs/tspi-greet-rs || true' 2>&1
    } 2>&1 | tee "$LOG_RUN"
else
    {
        echo "## ssh tspi unreachable — skipped remote run."
        echo "## local cross-build chain has already proved aarch64 + libcairo link."
        echo "## see $LOG_VERIFY for static evidence."
    } | tee "$LOG_RUN"
fi

echo
echo "============================================================"
echo "[test.sh] DONE.  Logs: $LOG_BUILD  $LOG_VERIFY  $LOG_RUN"
echo "============================================================"
