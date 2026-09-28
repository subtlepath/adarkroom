#!/bin/sh
# Whole-flash images, one per device: the bootloader, the partition table, the OTA selector and
# the app, merged at their offsets. Flash one at 0x0 (esptool, or a web flasher) to install A Dark
# Room on a device whatever it ran before; the app alone (build/adarkroom-<device>.bin) goes at
# 0x10000, keeping the device's own partition table.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CORE="${PLATFORMIO_CORE_DIR:-$ROOT/build/pio-core}"
PY="$CORE/penv/bin/python"
[ -x "$PY" ] || PY="${PIO_PYTHON:-python3}"
ESPTOOL="$CORE/packages/tool-esptoolpy/esptool.py"
APP0="$CORE/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
for pair in x3:esp32c3 x4c:esp32s3 x4pro:esp32s3; do
  dev=${pair%%:*}
  chip=${pair##*:}
  B="$ROOT/build/pio-work/build/$dev"
  cp "$B/firmware.bin" "$ROOT/build/adarkroom-$dev.bin"
  "$PY" "$ESPTOOL" --chip "$chip" merge-bin -o "$ROOT/build/adarkroom-$dev-factory.bin" \
    0x0 "$B/bootloader.bin" 0x8000 "$B/partitions.bin" 0xe000 "$APP0" 0x10000 "$B/firmware.bin" > /dev/null
done
ls -l "$ROOT"/build/adarkroom-*.bin
