#!/usr/bin/env bash
# tspi-hello-rs: 部署到 NFS 共享目录（板上挂在 /mnt/nfs）
set -euo pipefail
cd "$(dirname "$0")"

BIN="target/aarch64-unknown-linux-gnu/release/tspi-hello-rs"
NFS_LOCAL=/home/pi/imx/prj/nfs_tspi
DST="$NFS_LOCAL/tspi-hello-rs"

[ -f "$BIN" ] || { echo "[err] $BIN not built. run ./build.sh first."; exit 1; }

mkdir -p "$DST"
install -m 755 "$BIN" "$DST/tspi-hello-rs"
echo "[ok] deployed to $DST"
ls -la "$DST"
