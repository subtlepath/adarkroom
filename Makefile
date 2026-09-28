# A Dark Room for the Xteink X4 Pro, X4 Classic and X3 — the checks, in the order they get slower.
#
#   make data           regenerate the event tables (game/script/events/*.js) and the fonts (fonts/*.bdf)
#   make test           the host tests (sanitized), which also build build/host/adr_host
#   make shots          every screen on every device, both ways up, into build/shots/ (PNG)
#   make firmware       all three firmwares with PlatformIO: build/adarkroom-<device>[-factory].bin
#   make sim-test       the firmware on the FreeInk simulator, driven by keys, touch and Power
#   make all-tests      all of the above
#   make flash-x4pro    build and upload over USB (also flash-x4c, flash-x3)
#   make web            the firmwares, copied into the web flasher (web/public/firmware/)
#   make web-dev        the web flasher on http://localhost:8000
#
# Needs the submodules (git submodule update --init: the FreeInk SDK and the original game), Node
# (for `make data`), Python 3 and a C++17 compiler. PlatformIO installs itself
# under build/ on first use.

.PHONY: data test shots firmware sim-test all-tests flash-x4pro flash-x4c flash-x3 web web-dev clean

data:
	node tools/extract_events.cjs
	python3 tools/gen_fonts.py

test:
	sh test/host/run.sh

shots: test
	sh scripts/shots.sh

firmware:
	sh scripts/pio.sh run
	sh scripts/factory.sh

sim-test:
	sh scripts/sim-test.sh

all-tests: test shots firmware sim-test

flash-x4pro:
	sh scripts/pio.sh run -e x4pro -t upload

flash-x4c:
	sh scripts/pio.sh run -e x4c -t upload

flash-x3:
	sh scripts/pio.sh run -e x3 -t upload

web: firmware
	sh scripts/web.sh

web-dev:
	python3 -m http.server 8000 --directory web/public

clean:
	rm -rf build/host build/shots build/sim build/sim-src build/sim-run-* build/pio-work build/adarkroom-*.bin
