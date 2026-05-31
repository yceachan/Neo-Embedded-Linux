#!/usr/bin/env bash
set -euo pipefail

# ── gtk4-rs demo: one-click test chain ──────────────────
# clean → build → run (with tee to log) → assert
# Local-only demo (no deploy needed — GTK4 GUI runs on host).

DEMO_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG_DIR="$DEMO_DIR/output/log"
mkdir -p "$LOG_DIR"

TIMESTAMP=$(date +%Y%m%d-%H%M%S)
LOG_FILE="$LOG_DIR/run-$TIMESTAMP.log"

echo "=== gtk4-rs demo test chain ==="
echo "Log: $LOG_FILE"

# ── clean ──────────────────────────────────────────
echo "[1/3] cargo clean"
cd "$DEMO_DIR"
cargo clean 2>&1 | tee -a "$LOG_FILE"

# ── build ──────────────────────────────────────────
echo "[2/3] cargo build"
cargo build 2>&1 | tee -a "$LOG_FILE"

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo "❌ BUILD FAILED — see $LOG_FILE"
    exit 1
fi
echo "   ✅ build OK"

# ── run (headless check) ───────────────────────────
# GTK4 needs a display.  If $DISPLAY is set we try a 3-second smoke test.
# Otherwise we skip the run and just report build success.
echo "[3/3] cargo run (smoke test)"

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "   ⚠  No display available — skipping run (build-only verification)."
    echo "   Run manually: cd $DEMO_DIR && cargo run"
    exit 0
fi

# Launch app in background, let it run 4 seconds, then kill.
cargo run &
APP_PID=$!
sleep 4

if kill -0 "$APP_PID" 2>/dev/null; then
    echo "   ✅ App still running after 4s — smoke test passed."
    kill "$APP_PID" 2>/dev/null || true
    wait "$APP_PID" 2>/dev/null || true
else
    wait "$APP_PID" 2>/dev/null || EXIT_CODE=$?
    if [ "${EXIT_CODE:-0}" -ne 0 ]; then
        echo "❌ App exited with code $EXIT_CODE — check $LOG_FILE"
        exit 1
    fi
fi

echo "=== ✅ test.sh complete ==="
