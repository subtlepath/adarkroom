#!/bin/sh
# The screens, drawn on the host into build/shots/<device>-<tall|wide>/<name>.png, for looking at.
# (make test builds build/host/adr_host.)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
H="$ROOT/build/host/adr_host"
[ -x "$H" ] || { echo "run make test first (it builds adr_host)" >&2; exit 1; }
rm -rf "$ROOT/build/shots"
for dev in x4pro x4c x4 x3; do
  for rot in 1 0; do
    name=$([ $rot = 1 ] && echo tall || echo wide)
    D="$ROOT/build/shots/$dev-$name"
    mkdir -p "$D"
    "$H" --device $dev --rot $rot --out "$D" snap 01-dark key confirm tap 72 80 wait 200 snap 02-firelit \
      late snap 03-room key down snap 04-village key down snap 05-path key down snap 06-fabricator key down snap 07-ship \
      key back snap 08-menu key back > /dev/null
    "$H" --device $dev --rot $rot --out "$D" late embark snap 10-world walk n 3 snap 11-world-north key confirm \
      snap 12-overview key back event house snap 13-house event encounter.9 wait 1.2 snap 14-fight win wait 2 \
      snap 15-loot > /dev/null
    "$H" --device $dev --rot $rot --out "$D" late liftoff wait 8 snap 20-space wait 30 snap 21-space-high ending \
      snap 22-ending > /dev/null
    "$H" --device $dev --rot $rot --out "$D" late sleep 30-sleep > /dev/null
  done
done
echo "shots in $ROOT/build/shots"
