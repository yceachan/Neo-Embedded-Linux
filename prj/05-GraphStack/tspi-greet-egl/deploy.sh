#!/usr/bin/env bash
# tspi-greet-egl: 部署到 NFS 共享目录（板上挂在 /mnt/nfs）
set -euo pipefail
cd "$(dirname "$0")"

NFS_LOCAL=/home/pi/imx/prj/nfs_tspi
DST="$NFS_LOCAL/tspi-greet-egl"
BIN=output/bin/tspi-greet-egl

[ -f "$BIN" ] || { echo "[err] $BIN not built. run ./build.sh first."; exit 1; }

mkdir -p "$DST"
install -m 755 "$BIN" "$DST/tspi-greet-egl"
install -m 644 etc/00-kiosk-egl.ini "$DST/00-kiosk-egl.ini"
echo "[ok] deployed to $DST"
ls -la "$DST"
