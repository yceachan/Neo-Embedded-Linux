#!/usr/bin/env bash
# tspi-greet-egl: 交叉编译入口 (aarch64, buildroot sysroot)
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p output/log output/bin
LOG=output/log/build.log
echo "[build] start $(date +%H:%M:%S)" | tee "$LOG"

make clean 2>&1 | tee -a "$LOG" >/dev/null
make -j"$(nproc)" 2>&1 | tee -a "$LOG"

BIN=output/bin/tspi-greet-egl
[ -f "$BIN" ] || { echo "[err] $BIN not built" | tee -a "$LOG"; exit 1; }

SR=/home/pi/imx/sdk/tspi-rk3566-sdk/buildroot/output/rockchip_rk3566_taishanpi_1m_v10/rockchip_rk3566
READELF=$SR/host/bin/aarch64-buildroot-linux-gnu-readelf

echo "[ok]    $(file $BIN | cut -d, -f1-2)" | tee -a "$LOG"
echo "[size]  $(du -h $BIN | cut -f1)"     | tee -a "$LOG"
echo "[deps]" | tee -a "$LOG"
"$READELF" -d "$BIN" | grep -E 'NEEDED|SONAME' | sed 's/^/        /' | tee -a "$LOG"
echo "[build] done $(date +%H:%M:%S)" | tee -a "$LOG"
