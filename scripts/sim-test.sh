#!/bin/sh
# The firmware on the FreeInk simulator, driven through its real input and display paths:
# the X4 Classic and the X3 by their seven keys, the X4 Pro by touch, its side keys and its Home
# key; each lights the fire, wakes the stranger, sleeps, and wakes where it slept.
# Captures land in build/sim-run-<device>/.
#
#   scripts/sim-test.sh
#
# Every step waits for the firmware's own serial line ("[adr] key: A Firelit Room ..."), so the
# checks read what the firmware did, not what a script hoped.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
sh "$ROOT/scripts/sim.sh" build
case "$(uname -s)" in Darwin) EXT=dylib ;; *) EXT=so ;; esac
SDK=$(cd "$ROOT/freeink-sdk" && pwd)
SIMCLI="$SDK/tools/simulator/cli/freeink-sim"
SOCKET="$ROOT/build/sim/t$$.sock"
sim() { "$SIMCLI" --socket "$SOCKET" "$@"; }

pass=0
fail=0
ok() { echo "  ok   $1"; pass=$((pass + 1)); }
bad() { echo "  FAIL $1"; fail=$((fail + 1)); }
DAEMON_PID=""
stop() {
  [ -n "$DAEMON_PID" ] && kill "$DAEMON_PID" 2>/dev/null || true
  [ -n "$DAEMON_PID" ] && wait "$DAEMON_PID" 2>/dev/null || true
  DAEMON_PID=""
  rm -f "$SOCKET"
}
trap stop EXIT INT TERM
fresh() { sim log-clear > /dev/null; }
step() {  # step NAME REGEX [TIMEOUT]
  if sim expect "$2" --timeout "${3:-20000}" -q > /dev/null 2>&1; then ok "$1"; else
    bad "$1 (never logged /$2/)"
    sim log --channel serial | tail -6
  fi
}
key() { sim press "$1" > /dev/null; sleep 0.3; }
capture() { sim capture "$WORK/$1.png" --wait-refresh > /dev/null 2>&1 || sim capture "$WORK/$1.png" > /dev/null; }
start() {  # start DEVICE BUNDLE
  WORK="$ROOT/build/sim-run-$2"
  rm -rf "$WORK"
  mkdir -p "$WORK/state" "$WORK/card"
  "$ROOT/build/sim/freeink-simd" --headless --rate 4 --seed 1 --socket "$SOCKET" --device "$1" \
    --state-dir "$WORK/state" --sd "$WORK/card" "$ROOT/build/sim/$2.$EXT" >> "$WORK/daemon.log" 2>&1 &
  DAEMON_PID=$!
  waited=0
  while [ ! -S "$SOCKET" ] && [ $waited -lt 100 ]; do sleep 0.1; waited=$((waited + 1)); done
}
# Portrait page (x, y) -> panel (px, py), for the default rotation (R90): px = y, py = 479 - x.
tap() { sim tap --x "$2" --y $((479 - $1)) --hold 120 > /dev/null; sleep 0.3; }
home() { sim tap --x 0 --y 10000 --hold 150 > /dev/null; sleep 0.4; }

keys_device() {  # keys_device DEVICE BUNDLE NAME-AS-LOGGED
  echo "── the $3, by its keys ─────────────────────────────"
  start "$1" "$2"
  step "boots as the $3" "\\[adr\\] $4" 60000
  step "a new game in the dark room" '\[adr\] ready: A Dark Room focus 0201' 60000
  capture dark
  fresh; key confirm; step "Confirm lights the fire" 'key: A Firelit Room'
  step "the fire is told" 'log: the fire is burning'
  sim clock --rate 40 > /dev/null
  step "the stranger stumbles in" 'log: a ragged stranger stumbles through the door' 120000
  step "the wind howls outside" 'log: the wind howls outside' 120000
  sim clock --rate 4 > /dev/null
  capture firelit
  fresh; key down; step "the lower side key goes out to the forest" 'key: A Silent Forest'
  step "the forest is described" 'log: the sky is grey and the wind blows relentlessly'
  fresh; key confirm; step "Confirm gathers wood" 'log: dry brush and dead branches litter the forest floor'
  capture forest
  fresh; key up; step "the upper side key goes back to the room" 'key: A Firelit Room'
  fresh; key back; step "Back opens the menu (the world paused)" 'key: A Firelit Room overlay 1'
  capture menu
  fresh; key down; key confirm; step "the lights go out" 'key: A Firelit Room overlay 1'
  capture dark-mode
  key confirm
  fresh; key back; step "Back resumes" 'key: A Firelit Room focus'
  fresh; sim press power > /dev/null; step "Power sleeps" '\[adr\] sleep' 20000
  sleep 1
  capture asleep
  fresh; sim press power > /dev/null; step "Power wakes to the save" '\[adr\] the save is back' 60000
  step "the room as it was left" '\[adr\] ready: A Firelit Room' 60000
  capture woken
  stop
}

keys_device X4CLASSIC x4c "X4 Classic" "xteink_x4_classic"
keys_device X3 x3 "X3" "xteink_x3"

echo "── the X4 Pro, by touch ────────────────────────────"
start X4PRO x4pro
step "boots as the X4 Pro" '\[adr\] xteink_x4_pro' 60000
step "a new game in the dark room" '\[adr\] ready: A Dark Room' 60000
fresh; tap 72 80; step "a tap on light fire lights it" 'tap: A Firelit Room'
sim clock --rate 40 > /dev/null
step "the wind howls outside" 'log: the wind howls outside' 120000
sim clock --rate 4 > /dev/null
fresh; sim press down > /dev/null; sleep 0.3; step "the lower side key goes out" 'key: A Silent Forest'
capture forest
fresh; sim press up > /dev/null; sleep 0.3; step "the upper side key comes back" 'key: A Firelit Room'
fresh; home; step "Home opens the menu" 'home: A Firelit Room overlay 1'
capture menu
fresh; home; step "Home again resumes" 'home: A Firelit Room'
fresh; sim press power > /dev/null; step "Power sleeps" '\[adr\] sleep' 20000
sleep 1
fresh; sim press power > /dev/null; step "Power wakes to the save" '\[adr\] the save is back' 60000
step "the room as it was left" '\[adr\] ready: A Firelit Room' 60000
stop

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
