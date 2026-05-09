#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
MODE="${1:---fast}"

run_fast() {
  "$ROOT_DIR/scripts/run-host-tests.sh"
  "$ROOT_DIR/scripts/check-repo-contracts.sh"
}

run_full() {
  run_fast
  if [ -x "$ROOT_DIR/.venv/bin/pio" ]; then
    PLATFORMIO_CORE_DIR="$ROOT_DIR/.platformio" \
      PLATFORMIO_SETTING_ENABLE_TELEMETRY=no \
      "$ROOT_DIR/.venv/bin/pio" run
  elif command -v pio >/dev/null 2>&1; then
    PLATFORMIO_CORE_DIR="$ROOT_DIR/.platformio" \
      PLATFORMIO_SETTING_ENABLE_TELEMETRY=no \
      pio run
  else
    echo "verify-repo: no PlatformIO executable found (.venv/bin/pio or pio on PATH)" >&2
    exit 1
  fi
}

case "$MODE" in
  --fast)
    run_fast
    ;;
  --full)
    run_full
    ;;
  *)
    echo "usage: $0 [--fast|--full]" >&2
    exit 2
    ;;
esac
