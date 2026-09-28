# Simulator patches

`scripts/sim.sh` builds the FreeInk simulator from a private copy of `freeink-sdk/tools/simulator`
with these patches applied (the SDK checkout itself is never touched). Each fixes a gap in the
simulator's model of the hardware, not in this firmware:

- `clock-sleeper-ticks.patch` — a realtime sleep keeps its own appointment (frames stream at speed).
- `gt911-zhome-key.patch` — the X4 Pro's capacitive Home key: a touch at y >= 10000 stands for it.
- `sdfat-ram-card.patch` — SdFat's `Sector_t` and friends for the host card.
- `uc81xx-lut-duration.patch` — UC81xx waveform timing.
- `wake-latch.patch` — a short press still wakes a sleeping chip.
- `wire-requestfrom.patch` — Arduino's `requestFrom(uint8_t, uint8_t)`, which the gauge drivers call.

`sim_compat.h` is force-included into every bundle unit for the few Arduino names the simulator's
shims lack.
