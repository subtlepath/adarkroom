#!/bin/sh
# The web flasher's firmware: each device's factory image (from `make firmware`) copied into
# web/public/firmware/, with the manifest the page reads (the build, and each image's size and
# SHA-256, which the page checks before it writes).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/web/public/firmware"
mkdir -p "$OUT"
rm -f "$OUT"/*.bin
for dev in x4pro x4c x3; do
  [ -f "$ROOT/build/adarkroom-$dev-factory.bin" ] || { echo "no build/adarkroom-$dev-factory.bin: make firmware first" >&2; exit 1; }
  cp "$ROOT/build/adarkroom-$dev-factory.bin" "$OUT/"
done
VERSION=$(git -C "$ROOT" describe --tags --always --dirty 2>/dev/null || true)
python3 - "$OUT" "$VERSION" <<'EOF'
import datetime, hashlib, json, os, sys
out, version = sys.argv[1], sys.argv[2]
chips = {"x4pro": "ESP32-S3", "x4c": "ESP32-S3", "x3": "ESP32-C3"}
firmware = {}
for dev, chip in chips.items():
    name = f"adarkroom-{dev}-factory.bin"
    data = open(os.path.join(out, name), "rb").read()
    firmware[dev] = {"file": name, "chip": chip, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
manifest = {"version": version or None, "built": datetime.date.today().isoformat(), "firmware": firmware}
with open(os.path.join(out, "manifest.json"), "w") as f:
    json.dump(manifest, f, indent=2)
    f.write("\n")
EOF
ls -l "$OUT"
