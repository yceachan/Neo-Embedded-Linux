#!/usr/bin/env bash
# tspi-greet-rs: Rust 交叉编译入口 (aarch64-unknown-linux-gnu)
#
# 关键点：
#   .cargo/config.toml 已注入 build.target / linker / pkg-config 三件套，
#   因此本脚本只需要 `cargo build --release` —— 不再显式 export 任何变量。
#   这与 C 工程 Makefile 必须 CC=/CFLAGS=/LDFLAGS= 形成强烈对比。
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p output/log output/bin

LOG=output/log/build.log
echo "[build] start  $(date +%H:%M:%S)" | tee "$LOG"

cargo build --release 2>&1 | tee -a "$LOG"

BIN_TGT="output/target/aarch64-unknown-linux-gnu/release/tspi-greet-rs"
[ -f "$BIN_TGT" ] || { echo "[err] binary not found: $BIN_TGT" | tee -a "$LOG"; exit 1; }

# 复制到统一的 output/bin/ 位置，方便 deploy / 文档引用
install -m 755 "$BIN_TGT" output/bin/tspi-greet-rs

echo "[ok]    $(file output/bin/tspi-greet-rs | cut -d, -f1-2)"     | tee -a "$LOG"
echo "[size]  $(du -h output/bin/tspi-greet-rs | cut -f1)"         | tee -a "$LOG"
echo "[deps]" | tee -a "$LOG"
SR=/home/pi/imx/sdk/tspi-rk3566-sdk/buildroot/output/rockchip_rk3566_taishanpi_1m_v10/rockchip_rk3566
"$SR/host/bin/aarch64-buildroot-linux-gnu-readelf" -d output/bin/tspi-greet-rs \
    | grep -E 'NEEDED|SONAME' | sed 's/^/        /' | tee -a "$LOG"
echo "[build] done   $(date +%H:%M:%S)" | tee -a "$LOG"
