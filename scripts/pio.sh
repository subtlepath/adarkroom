#!/bin/sh
# PlatformIO for this firmware, kept under build/ (its toolchains are large).
#
#   scripts/pio.sh                     build every device's firmware
#   scripts/pio.sh run -e x3 -t upload anything else is passed to `pio`
#
# PLATFORMIO_CORE_DIR may point at another project's PlatformIO core to reuse its toolchains.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK="$ROOT/freeink-sdk"
[ -f "$SDK/libs/hardware/BoardConfig/library.json" ] || {
  echo "the FreeInk SDK is not at $SDK: git submodule update --init" >&2
  exit 1
}
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$ROOT/build/pio-core}"
export PLATFORMIO_WORKSPACE_DIR="$ROOT/build/pio-work"
PIO="${PIO:-$ROOT/build/pio-venv/bin/pio}"
if [ ! -x "$PIO" ]; then
  python3 -m venv "$ROOT/build/pio-venv"
  "$ROOT/build/pio-venv/bin/pip" -q install platformio
  PIO="$ROOT/build/pio-venv/bin/pio"
fi
cd "$ROOT"
[ $# -gt 0 ] || set -- run
exec "$PIO" "$@"
