#!/usr/bin/env bash
# tspi-greet: 部署到 NFS 共享目录（板上挂在 /mnt/nfs）
set -euo pipefail
cd "$(dirname "$0")"

NFS_LOCAL=/home/pi/imx/prj/nfs_tspi
DST="$NFS_LOCAL/tspi-greet"

[ -f tspi-greet ] || { echo "[err] tspi-greet not built. run ./build.sh first."; exit 1; }

mkdir -p "$DST"
install -m 755 tspi-greet "$DST/tspi-greet"
install -m 644 etc/00-kiosk.ini "$DST/00-kiosk.ini"
echo "[ok] deployed to $DST"
ls -la "$DST"
