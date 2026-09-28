// ui.h — what the screens share: the ids of what can be pressed, the menu's rows, the measures.
#pragma once

#include <cstdint>

#include "canvas.h"
#include "game.h"

namespace adr {

class App;

namespace ui {

// A hot spot's id: the group in the high byte, which one in the low.
constexpr uint16_t A_TAB = 0x0100;  // + Module
constexpr uint16_t A_SINGLE = 0x0200;
constexpr uint16_t A_BUILD = 0x0300;
constexpr uint16_t A_BUY = 0x0400;
constexpr uint16_t A_FAB = 0x0500;
constexpr uint16_t A_WORKER = 0x0600;  // the row: Left and Right adjust it
constexpr uint16_t A_WORKER_DEC10 = 0x0700;
constexpr uint16_t A_WORKER_DEC = 0x0800;
constexpr uint16_t A_WORKER_INC = 0x0900;
constexpr uint16_t A_WORKER_INC10 = 0x0A00;
constexpr uint16_t A_OUTFIT = 0x0B00;
constexpr uint16_t A_OUTFIT_DEC10 = 0x0C00;
constexpr uint16_t A_OUTFIT_DEC = 0x0D00;
constexpr uint16_t A_OUTFIT_INC = 0x0E00;
constexpr uint16_t A_OUTFIT_INC10 = 0x0F00;
constexpr uint16_t A_EVBTN = 0x1000;
constexpr uint16_t A_COMBAT = 0x1100;
constexpr uint16_t A_LOOT = 0x1200;
constexpr uint16_t A_LOOT_ALL = 0x1300;
constexpr uint16_t A_MENU = 0x1400;
constexpr uint16_t A_DIALOG = 0x1500;
constexpr uint16_t A_DROP = 0x1600;
constexpr uint16_t A_ENDING = 0x1700;

constexpr uint16_t S_FIRE = 1, S_GATHER = 2, S_TRAPS = 3, S_EMBARK = 4, S_HULL = 5, S_ENGINE = 6, S_LIFTOFF = 7,
                   S_TAKE_EVERYTHING = 8, S_OVERVIEW = 9, S_MENU = 10, S_RESTART = 11, S_PAGE_NEXT = 12,
                   S_PAGE_PREV = 13;
constexpr int M_RESUME = 0, M_LIGHTS = 1, M_HYPER = 2, M_TEMPO = 3, M_SLEEP = 4, M_TURN = 5, M_LIGHT = 6,
              M_RESTART = 7, kMenuItems = 8;

// The measures, from the glass's size and whether it is touched.
struct Metrics {
  int W, H;
  bool portrait, touch;
  int margin;
  int headerH;
  int btnH;      // a button
  int rowH;      // a line of a list
  int counterH;  // a worker's or a supply's row
  int footerH;
};
Metrics metrics(const App& app);

// Whether a cooldown shows anywhere on the current screen (a frame now and then moves its bar).
bool anyCooldown(const Game& g);
// Where the wanderer is drawn on the map (a tap is measured from there).
Point mapCentre(const App& app);
const char* tempoName(int t);
// The short name of a place, for a header too narrow for the long ones.
const char* shortName(const Game& g, Module m);
// The footer's line for a focused thing: its cost, or what it does.
void describeFocus(const Game& g, uint16_t id, char* out, int cap);
// "wood 100, fur 10" into out.
void costText(const Amount* a, int n, char* out, int cap);
// A count as the original shows it: floored, with no separators.
void count(double v, char* out, int cap);

}  // namespace ui
}  // namespace adr
