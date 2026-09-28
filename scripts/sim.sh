#!/bin/sh
# The firmware on the FreeInk simulator.
#
#   scripts/sim.sh build              the daemon (build/sim/freeink-simd) and the x4pro, x4c, x3 bundles
#   scripts/sim.sh run [ARGS...]      the X4 Pro bundle (a window when SDL2 is present)
#   scripts/sim.sh run-x4c [ARGS...]  the X4 Classic's
#   scripts/sim.sh run-x3 [ARGS...]   the X3's
#
# Drive a running simulator from another shell with freeink-sdk/tools/simulator/cli/freeink-sim.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK=$(cd "$ROOT/freeink-sdk" && pwd)
SIM="$SDK/tools/simulator"
CMD=${1:-build}
[ $# -gt 0 ] && shift
case "$(uname -s)" in Darwin) EXT=dylib ;; *) EXT=so ;; esac
OUT="$ROOT/build/sim"
DAEMON="$OUT/freeink-simd"
SRC="$ROOT/build/sim-src"

build() {
  mkdir -p "$OUT"
  if [ ! -x "$DAEMON" ] || [ ! -d "$SRC" ]; then
    rm -rf "$SRC"
    mkdir -p "$SRC/tools"
    cp -R "$SIM" "$SRC/tools/simulator"
    rm -rf "$SRC/tools/simulator/build"/*.dylib* "$SRC/tools/simulator/build"/*.so "$SRC/tools/simulator/build"/freeink-*
    ln -s "$SDK/libs" "$SRC/libs"
    for patch in "$ROOT"/sim/patches/*.patch; do
      (cd "$SRC" && patch -p1 -s < "$patch") || { echo "simulator patch $(basename "$patch") failed" >&2; exit 1; }
    done
    export FSIM_OUT="$DAEMON" FSIM_EMU_OUT="$OUT/freeink-emu"
    sh "$SRC/tools/simulator/build/build-daemon.sh" > "$OUT/build-daemon.log" 2>&1 ||
      FSIM_NO_SDL=1 sh "$SRC/tools/simulator/build/build-daemon.sh" > "$OUT/build-daemon.log" 2>&1 ||
      { echo "daemon build failed; see build/sim/build-daemon.log" >&2; exit 1; }
  fi
  HW="$SDK/libs/hardware"
  FW="$ROOT/src"
  bundle() {  # bundle DEVICE OUT
    CXX="${CXX:-c++} -include $ROOT/sim/sim_compat.h" \
    sh "$SRC/tools/simulator/build/build-firmware.sh" --device "$1" --out "$2" \
      --define ADR_SIM=1 --define EINK_DISPLAY_SINGLE_BUFFER_MODE=1 \
      --lib "$FW" --lib "$ROOT/include" \
      "$HW/XteinkDetect/src/XteinkDetect.cpp" \
      "$HW/PowerManager/src/PowerManager.cpp" \
      "$HW/BatteryMonitor/src/BatteryMonitor.cpp" \
      "$FW/main.cpp" "$FW"/core/*.cpp "$FW"/hal/*.cpp "$FW"/generated/*.cpp > "$OUT/build-bundle-$1.log" 2>&1 ||
      { tail -30 "$OUT/build-bundle-$1.log" >&2; echo "bundle build failed; see build/sim/build-bundle-$1.log" >&2; exit 1; }
  }
  bundle X4PRO "$OUT/x4pro.$EXT"
  bundle X4CLASSIC "$OUT/x4c.$EXT"
  bundle X3 "$OUT/x3.$EXT"
}

case "$CMD" in
  build) build ;;
  run) build; exec "$DAEMON" "$@" "$OUT/x4pro.$EXT" ;;
  run-x4c) build; exec "$DAEMON" --device X4CLASSIC "$@" "$OUT/x4c.$EXT" ;;
  run-x3) build; exec "$DAEMON" --device X3 "$@" "$OUT/x3.$EXT" ;;
  *) sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
