// save.h — the game, as a few kilobytes for the device's NVS.
//
// Field by field, in a fixed order, each number a varint: a firmware that changes the state adds
// to the end and bumps kSaveVersion, and still reads the older saves it knows. The world map is
// kept as its seed and the tiles that differ from what the seed grows (the wanderer's roads, the
// places cleared, the landmarks visited); an expedition, as its differences from the world at
// home. The whole is closed by a CRC-32.
#pragma once

#include <cstddef>
#include <cstdint>

namespace adr {

class Game;

constexpr size_t kMaxSave = 12 * 1024;
constexpr uint8_t kSaveVersion = 1;

size_t saveGame(Game& g, uint8_t* out, size_t cap);
bool loadGame(Game& g, const uint8_t* in, size_t n);
// Only the prestige (a finished game's legacy), from a save that will not load whole.
bool loadPrestige(Game& g, const uint8_t* in, size_t n);

}  // namespace adr
