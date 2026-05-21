#!/usr/bin/env bash
# tspi-greet: 交叉编译入口
set -euo pipefail
cd "$(dirname "$0")"
make clean >/dev/null
make -j"$(nproc)"
echo "[ok] built: $(file tspi-greet | cut -d, -f1-2)"
