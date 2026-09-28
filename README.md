# A Dark Room, for Xteink

*awake. head throbbing. vision blurry. come light the fire.*

A Dark Room as firmware for the Xteink e-paper readers: the **X4 Pro**, the **X4 Classic** and the
**X3** (and the first, ESP32-C3 **X4**, which shares the X3's binary). It is the whole game — the
room and its fire, the stranger who builds, the village and its workers, the traders and the
thieves, the dusty path, the barren world and everything in it, the ravaged battleship and its
fabricator, the old starship and the ascent — ported from
[the original](https://github.com/doublespeakgames/adarkroom)'s JavaScript to C++ on the
[FreeInk SDK](https://github.com/subtlepath/freeink-sdk), and redrawn for a glass that holds its
image without power and changes it slowly.

| | X4 Pro | X4 Classic | X3 | X4 (C3) |
|---|---|---|---|---|
| chip | ESP32-S3 | ESP32-S3 | ESP32-C3 | ESP32-C3 |
| glass | 800 × 480, 220 ppi | 800 × 480 | 792 × 528, 259 ppi | 800 × 480 |
| played by | touch, two side keys, Home | seven keys | seven keys | seven keys |
| firmware | `adarkroom-x4pro.bin` | `adarkroom-x4c.bin` | `adarkroom-x3.bin` | `adarkroom-x3.bin` |

## Installing

**From the browser**: the web flasher, at <https://subtlepath.github.io/adarkroom/>, writes the firmware over USB from Chrome or Edge on
a computer (Web Serial). Choose the device, wake it, plug it in, press *flash*. It checks the chip
before it writes, leaves the save alone when updating, and can first read the whole flash to a
file, to go back to exactly what was there.

**By hand**: `make firmware` builds, for each device, `build/adarkroom-<device>-factory.bin` (bootloader,
partition table and app: flash it at `0x0`) and `build/adarkroom-<device>.bin` (the app alone:
flash it at `0x10000`, keeping the device's own partitions). With the device on USB:

```sh
make flash-x4pro            # or flash-x4c, flash-x3 (PlatformIO's upload)
esptool.py --chip esp32s3 write_flash 0x0 build/adarkroom-x4pro-factory.bin   # or by hand
```

The way back, without a computer: hold **Up and Down** while pressing **Power** to wake it, and
the firmware flashes `/update.bin` from the SD card (the SDK's recovery boot) — put another
firmware there first.

## Playing

The game is the original's, word for word: its text, its costs, its odds and its timings come from
`game/script/`. What the browser did with a mouse and a window, the device does with its keys, its
touch and its glass.

**On the X4 Classic, the X3 and the X4** (the seven keys):

| | in a place | in the world | in an event or a fight |
|---|---|---|---|
| Up / Down (side keys) | the previous / next place | walk north / south | move the focus |
| Left / Right | move the focus; while changing a worker or a supply, one fewer / more | walk west / east | move the focus; on loot, Right takes all |
| Confirm | press what has the focus; on a worker or a supply, start (and finish) changing it | the whole map | press it; held on loot, take all |
| held Left / Right | the focus runs on; while changing, ten at a time | keep walking | |
| Back | the menu (the world pauses); while changing, done | the menu | the menu |
| Power | sleep (the world pauses; wake to where it slept) | | |

**On the X4 Pro**: touch what you would click. A **long press** on anything shows what it costs or
does in the footer (the browser's tooltips); on loot it takes all of it. **Swipe** left and right
to go between places, up and down to turn a page. The **side keys** go to the previous and next
place (in the world, they show the whole map). **Home** is the menu. In the world, tap toward
where you want to walk, or swipe; in the ascent, hold a finger where the ship should go.

The footer always says what the focused thing costs, or does.

### The menu

| | |
|---|---|
| lights | on (black on white) or off (white on black, the browser's "lights off") |
| hyper | the original's x2 speed |
| fights | **brisk** (the original's timings), **measured** (x1.5, the default) or **slow** (x2): both sides slowed alike, so an e-paper panel and a thumb can keep up |
| sleep | how long untouched before the device sleeps: 5, 10, 15 (default), 30, 60 minutes or never. It never sleeps in the middle of a fight or the ascent |
| glass | tall (portrait) or wide (landscape) |
| frontlight | the X4 Pro's: off, **firelight** (as bright, and as warm, as the fire in the room: dark when it is dead, brightest when it roars), dim, half, full |
| restart | a new game (the prestige of a finished one is kept, as the original keeps it) |

## Made for e-paper

- **The sleeping glass shows the keys.** Asleep, the screen points an arrow at each of the device's
  own keys, where they are on its body, with what it does: the side keys, Power, the four keys
  below the glass or the X4 Pro's Home pad. The places come from Xteink's user guides for each
  device (the X4 Pro and X4 Classic share a body; the first X4 has its keys down the right side), and
  the arrows follow the glass when it is turned wide.
- **The world waits.** Every timer the original ran — the fire cooling, the builder, the income,
  the strangers arriving, the next event, a mysterious wanderer's return, even a fight — is part
  of the save. Sleep, and the game pauses exactly where it was, mid-expedition or mid-fight;
  wake, and it goes on. (In a browser, closing the tab lost the timers and the expedition.)
- **Refreshes only when something shows.** A frame is drawn when the game changes and pushed to
  the glass only if it differs from what is there; a new screen gets a clean refresh, small
  changes a fast one, with a clean one every so often against ghosting. A cooldown's grey bar
  shrinks in steps the panel can show (about a second each), not continuously.
- **The log fades** as the browser's does, older lines in a dithered grey.
- **Stores show their income**: the net per ten seconds beside each count, what the browser
  kept in a tooltip.
- **The map** is a window on the wanderer, in a monospace as the browser's is, the land grey
  and the landmarks bold, with a key to the landmarks in sight. The whole map, drawn to the pixel,
  is one key away.
- **Late in the game** a place may hold more than the glass: its lists run on to a second page,
  which the focus, a swipe or the footer turns. Buildings that can be built no more are left off
  the room's buttons (the village lists them).
- **The ascent** is a grid: the sky steps twice a second, a key moves the ship a cell, held keys
  move it as far as its thrusters allow. The altitudes, the waves thickening at 10, 20 and 40, the
  hull and the minute to space are the original's; the sky darkens as the browser's background did.
- **Type**: Times, as the browser asks for, from the Adobe X11 bitmap strikes: the body, and
  everything else, at 25 px (Times 18 at 100 dpi, CrossPoint's "Times 12"), titles at 34 px, and
  only a longer caption (a footer's hint, a perk's description) smaller, at 20 px. UW ttyp0 for the
  map and the sky. On the X4s held tall, a place is one column (two of this size do not fit), its
  lists running on over pages.

### Where it differs from the browser

- No sound (none of these devices has a speaker), no sharing, links, Dropbox or app store, and
  English only.
- Space, as above.
- The executioner's "rush through" costs the 10 hp it says (the original meant to, but read an
  undefined value, so it was free); it never takes the last one.
- In a fight, eat, meds and hypo are greyed out when there are none to use.
- A message meant for a place you are not in waits for you there, as the original intended (its
  shared queue printed every waiting message at the next place visited, whichever it was for).
- The supplies packed for the path never exceed the stores (the original deducted a stale outfit
  on embarking, which could duplicate items).

## Building and testing

Two submodules, pinned: `freeink-sdk/` (the [FreeInk SDK](https://github.com/subtlepath/freeink-sdk))
and `game/` ([the original](https://github.com/doublespeakgames/adarkroom), whose events the
tables are generated from). Then a C++17 compiler, Python 3, and Node (to regenerate the event
tables). PlatformIO installs itself under `build/` on first use (or point
`PLATFORMIO_CORE_DIR` at an existing one).

```sh
git clone --recurse-submodules https://github.com/subtlepath/adarkroom
```

```sh
make test       # the host tests, with ASan and UBSan: the opening, the village, every event's
                # scenes, fights won and lost, the world, the ascent to the ending, save round
                # trips from every kind of moment, every screen on every device both ways up
make shots      # every screen, every device, tall and wide, as PNG in build/shots/
make firmware   # the three firmwares (and the factory images)
make sim-test   # the firmware on the FreeInk simulator: keys, touch, Home, Power, sleep and wake
make data       # regenerate src/generated/ from game/script/events/*.js and fonts/*.bdf
make web        # the firmwares, and a copy of each in the web flasher (web/public/firmware/)
```

`build/host/adr_host` drives the app on the host from a script and saves the glass as PNG, e.g.
`adr_host --device x3 --out /tmp key confirm wait 120 snap room`. `scripts/sim.sh run-x3` opens the
X3 in the simulator (a window when SDL2 is present), driven from another shell with
`freeink-sdk/tools/simulator/cli/freeink-sim`. The simulator can also emulate the flashable
images themselves (`freeink-simd --device X3 build/adarkroom-x3-factory.bin`): they boot, draw and
play there, but its emulated flash does not yet carry NVS, so an emulated image does not keep a save.

## How it is made

```
game/script/events/*.js ──tools/extract_events.cjs──▶ src/generated/events_data.cpp  (47 events, text as is)
fonts/*.bdf ──────────────tools/gen_fonts.py────────▶ src/generated/fonts_data.cpp   (9 strikes)

src/core/          the game and its screens, freestanding C++ (the host tests run it)
  game*.cpp        the rules: room.js, outside.js, path.js, world.js, events.js, ship.js, space.js …
  events.h         the shape of the generated event tables; the ~80 functions inside the
                   events are named by extract_events.cjs and ported in game_events.cpp
  state.h, save.*  the whole state, and its save (a few KB in NVS; the map as its seed and changes)
  app.*, ui_*.cpp  the screens, focus and touch, the menu, refresh policy
src/hal/           NVS, the battery, the frontlight
src/main.cpp       boot, keys and touch, the refresh loop, sleep
test/host/         the tests and the scripted driver
scripts/, sim/     PlatformIO, the simulator (and its patches), the screenshots
web/               the web flasher
```

A function in `game/script/events/*.js` that `extract_events.cjs` does not recognise stops the build,
so a change to the game's events cannot slip into the firmware unported.

## The web flasher

`web/public/` is a static site, published to GitHub Pages by `.github/workflows/pages.yml` on every
push to `main` that changes it. It is the whole of the flasher: the page, `flash.js` (which drives Espressif's
[esptool-js](https://github.com/espressif/esptool-js), vendored in `web/public/vendor/`), the
screenshots, and `firmware/`, which `make web` fills from `build/`: each device's factory image
and a `manifest.json` of their sizes and SHA-256s, which the page checks before it writes. The
firmware is committed, so the site publishes as it stands, with no build step.

```sh
make web        # after a change to the game: rebuild, and refresh web/public/firmware/
make web-dev    # the site on http://localhost:8000 (Web Serial works on localhost as on https)
```

What it writes: the factory image at `0x0` (bootloader, partition table, OTA selector, app). With
*keep the save* on (the default) it leaves out the NVS partition, read from the image's own
partition table, where the game keeps its save and settings, so an update goes on where the game
was. *Save what is on it first* reads the whole flash (16 MB) to a file before writing, and
*write a backup back…* writes such a file back from `0x0`.

## Licences

The game is © doublespeak games, under the Mozilla Public License 2.0 (`LICENSE.md`); this
port is derived from it and is under the same licence. Times: © Adobe Systems and Digital
Equipment Corporation (`fonts/x11/COPYING-Adobe`). UW ttyp0: © Uwe Waldmann (`fonts/ttyp0/LICENSE`).
The FreeInk SDK is MIT-licensed. esptool-js is © Espressif Systems, under the Apache License 2.0
(`web/public/vendor/esptool-js-LICENSE`). This project is not made by or affiliated with Xteink.
