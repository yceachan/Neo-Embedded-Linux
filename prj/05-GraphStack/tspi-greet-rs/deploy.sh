#!/usr/bin/env bash
# tspi-greet-rs: 部署到 NFS 共享目录（板上挂在 /mnt/nfs）
set -euo pipefail
cd "$(dirname "$0")"

NFS_LOCAL=/home/pi/imx/prj/nfs_tspi
DST="$NFS_LOCAL/tspi-greet-rs"
BIN=output/bin/tspi-greet-rs

[ -f "$BIN" ] || { echo "[err] $BIN not built. run ./build.sh first."; exit 1; }

mkdir -p "$DST"
install -m 755 "$BIN" "$DST/tspi-greet-rs"
install -m 644 etc/00-kiosk.ini "$DST/00-kiosk.ini"
echo "[ok] deployed to $DST"
ls -la "$DST"
