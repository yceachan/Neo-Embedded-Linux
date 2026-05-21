#!/usr/bin/env bash
# tspi-hello-rs: Rust 交叉编译入口
# 目标: aarch64-unknown-linux-gnu (RK3566 / Cortex-A55)
# 链接器: buildroot 产出的 aarch64-buildroot-linux-gnu-gcc
set -euo pipefail
cd "$(dirname "$0")"

cargo build --release
BIN="target/aarch64-unknown-linux-gnu/release/tspi-hello-rs"

echo "[ok] built: $(file "$BIN" | cut -d, -f1-2)"
echo "[size]   : $(du -h "$BIN" | cut -f1)"
