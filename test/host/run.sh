#!/bin/sh
# The host tests, with the address and undefined-behaviour sanitizers, and adr_host, the scripted
# driver that saves the screens as PNG.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/build/host"
mkdir -p "$OUT"
CXX=${CXX:-c++}
FLAGS="-std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined"
SRC="$ROOT/src/core/*.cpp $ROOT/src/generated/*.cpp"
$CXX $FLAGS -I"$ROOT/src/core" "$ROOT/test/host/tests.cpp" $SRC -o "$OUT/tests"
$CXX $FLAGS -I"$ROOT/src/core" "$ROOT/test/host/adr_host.cpp" $SRC -o "$OUT/adr_host"
"$OUT/tests" "$@"
