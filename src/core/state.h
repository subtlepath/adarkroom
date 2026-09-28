// state.h — the game's whole state, as typed fields where the original keeps a JSON tree
// (script/state_manager.js). Every field that tree held is here, plus what the original let
// slip on a reload (its timers, the expedition under way, the event on the glass): the device
// sleeps often, and a sleep must pause the world, not reset it.
#pragma once

#include <cstdint>
#include <cstring>

#include "items.h"

namespace adr {

constexpr int kRadius = 30;
constexpr int kMapSize = kRadius * 2 + 1;  // 61

// Map tiles: the original's characters (world.js TILE). A visited landmark carries the high bit
// (the original appends '!').
enum Tile : uint8_t {
  TL_VILLAGE = 'A', TL_IRON_MINE = 'I', TL_COAL_MINE = 'C', TL_SULPHUR_MINE = 'S',
  TL_FOREST = ';', TL_FIELD = ',', TL_BARRENS = '.', TL_ROAD = '#',
  TL_HOUSE = 'H', TL_CAVE = 'V', TL_TOWN = 'O', TL_CITY = 'Y', TL_OUTPOST = 'P',
  TL_SHIP = 'W', TL_BOREHOLE = 'B', TL_BATTLEFIELD = 'F', TL_SWAMP = 'M', TL_CACHE = 'U',
  TL_EXECUTIONER = 'X',
};
constexpr uint8_t kVisited = 0x80;

enum Module : uint8_t { M_ROOM, M_OUTSIDE, M_PATH, M_FABRICATOR, M_SHIP, M_WORLD, M_SPACE, M_COUNT, M_NONE = 0xFF };

// The world's own flags (game.world, and the expedition's World.state).
enum WorldFlag : uint8_t {
  WF_SULPHURMINE = 1, WF_IRONMINE = 2, WF_COALMINE = 4, WF_SHIP = 8, WF_EXECUTIONER = 16,
  WF_ENGINEERING = 32, WF_MEDICAL = 64, WF_MARTIAL = 128,
};

struct WorldMap {
  uint8_t tile[kMapSize][kMapSize];  // [x][y], as the original indexes it
  uint8_t mask[kMapSize][kMapSize];  // 1: seen
  uint8_t flags;
};

struct IncomeRec {
  bool active;
  uint8_t n;
  uint8_t delay;
  int32_t timeLeft;
  Item item[3];
  double amount[3];
};

// A named timer: what the original's setTimeout chains were. Kept in the save, so a sleep only
// pauses them.
enum TimerKind : uint8_t {
  T_NONE,
  T_FIRE_COOL, T_TEMP, T_BUILDER, T_UNLOCK_FOREST, T_INCOME, T_POP, T_EVENT,
  T_WANDERER,      // arg: 0 wood 300, 1 wood 1500, 2 fur 300, 3 fur 1500
  T_DEATH_RETURN,  // back in the room after dying
  // combat (arg as noted)
  T_ENEMY_ATTACK, T_SPECIAL,  // arg: special index
  T_DOT, T_STUN_END, T_STATUS_END,  // arg: 0 wanderer, 1 enemy; status end restores 'none'
  T_ENRAGE_END, T_EXPLODE, T_WIN_FADE, T_WIN_SHOW,
  T_COUNT
};

struct Timer {
  uint8_t kind;
  int16_t arg;
  int64_t due;  // game ms
  int64_t period;  // > 0: an interval
};
constexpr int kMaxTimers = 32;

// A button's cooldown, the main screens' (the original keeps these in state 'cooldown').
enum CooldownId : uint8_t { CD_FIRE, CD_GATHER, CD_TRAPS, CD_EMBARK, CD_LIFTOFF, CD_COUNT };
struct Cooldown {
  int64_t end;    // game ms; 0: none
  int32_t total;  // ms, as it was started
};

struct Settings {  // config.*
  bool lightsOff = false;  // white on black
  bool hyper = false;
};

// ---- the expedition (World.state and friends) -----------------------------------------------

struct Trip {
  bool active = false;
  WorldMap world;       // a copy of game.world, committed when home safe
  int8_t x = kRadius, y = kRadius;
  int16_t water = 0, health = 0;
  uint8_t foodMove = 0, waterMove = 0;
  bool starvation = false, thirst = false, dead = false;
  uint8_t usedOutposts[(kMapSize * kMapSize + 7) / 8];
  bool outpostUsed(int px, int py) const {
    const int i = px * kMapSize + py;
    return usedOutposts[i >> 3] & (1 << (i & 7));
  }
  void useOutpost(int px, int py) {
    const int i = px * kMapSize + py;
    usedOutposts[i >> 3] |= static_cast<uint8_t>(1 << (i & 7));
  }
};

// ---- the event on the glass -------------------------------------------------------------------

enum Status : uint8_t { ST_NONE, ST_SHIELD, ST_ENRAGED, ST_MEDITATION, ST_BOOST, ST_ENERGISED, ST_VENOMOUS };

struct Fighter {
  int16_t hp = 0, maxHp = 0;
  uint8_t status = ST_NONE;
  bool stunned = false;
  char floatText[12] = {};  // the last hit on it ("-3", "miss", "+8", "stunned")
  int64_t floatAt = 0;
};

constexpr int kMaxLoot = 12;
struct LootRow {
  Item item;
  int16_t numLeft;
};

constexpr int kMaxButtons = 12;
struct ActiveEvent {
  int16_t event = -1;  // index into kEvents; -1: none
  uint8_t scene = 0;
  int64_t buttonCd[kMaxButtons] = {};  // scene buttons' cooldowns: end, game ms
  int32_t buttonCdTotal[kMaxButtons] = {};
  // A fight.
  bool combat = false;
  bool fought = false, won = false, lootShown = false, exploding = false;
  Fighter wanderer, enemy;
  int32_t meditateDmg = 0;
  uint8_t lastSpecial = ST_NONE;
  int16_t dotDmg = 0;
  int16_t enemyAttackMs = 0;  // the enemy's attack interval now (enraged: 500)
  // Combat buttons: weapons (kWeapons index), then eat, meds, hypo, stim, shield, leave.
  int64_t combatCd[kWeaponCount + 6] = {};
  int32_t combatCdTotal[kWeaponCount + 6] = {};
  uint16_t weaponDisabled = 0;  // weapons out of ammunition (bit per kWeapons index)
  bool fistsAdded = false;
  // Loot.
  uint8_t lootN = 0;
  LootRow loot[kMaxLoot];
  // Events.canLeave, when the loot was drawn or the wanderer last healed: whether it all fits
  // ("take everything", else "take all you can"), and whether taking it also leaves
  bool lootFits = false, takeLeave = false;
  bool blink = false;
};

// ---- the ascent -------------------------------------------------------------------------------

constexpr int kMaxAsteroids = 64;
struct Asteroid {
  int16_t x, y;   // grid cell; y is the top row it occupies
  uint8_t glyph;  // '#', '$', '%', '&', 'H'
  uint8_t speed;  // rows per step
};

// ---- the log ----------------------------------------------------------------------------------

constexpr int kLogLines = 24;
constexpr int kLogChars = 200;
constexpr int kQueued = 16;

// ---- all of it --------------------------------------------------------------------------------

struct GameState {
  // stores
  double stores[kStoreCount];
  uint64_t storeSeen;  // a store key exists (the original shows a row for it, even at 0)
  // character
  uint16_t perks;
  uint8_t blueprints;  // bit per fabricator blueprint item (items.h order from IT_HYPO_BLUEPRINT)
  int32_t punches, starved, dehydrated;
  // the room
  int8_t fire, temperature, builderLevel;
  // features
  bool hasOutside, hasPath, hasWorld, hasShip, hasFabricator, executionerPlaced;
  // the village
  int16_t buildings[BD_COUNT];
  uint16_t buildingSeen;
  int32_t workers[WK_COUNT];
  uint16_t workerSeen;
  int32_t population;
  bool seenForest;
  int8_t thieves;  // 0: none yet, 1: stealing, 2: dealt with
  double stolen[kStoreCount];
  bool cityCleared;
  // income, in the order its sources first appeared
  IncomeRec income[SRC_COUNT];
  uint8_t incomeOrder[SRC_COUNT];
  uint8_t incomeN;
  // the path
  int32_t outfit[kStoreCount];
  // the world: generated from its seed, then changed by the wanderer's roads and cleared places
  uint32_t mapSeed;
  WorldMap world;
  bool seenAll;
  // the ship
  int16_t hull, thrusters;
  bool seenShip, seenWarning;
  bool fabricatorSeen;
  // what the build buttons have shown (Room.buttons): unlocked and, for the maximum notices,
  // whether it was at its maximum last time it was looked at
  uint64_t craftUnlocked, craftAtMax, tradeUnlocked;
  // prestige (kept across a restart)
  int32_t prevStores[24];
  bool hasPrev;
  int32_t prevScore;
  int32_t lastScore;
  // the engine
  int64_t now;  // game ms
  Timer timers[kMaxTimers];
  Cooldown cd[CD_COUNT];
  Settings settings;
  uint8_t active;  // Module
  bool pathDiscovery;
  bool gameOver;
  // the queue of notices for a location not in view
  char queued[kQueued][kLogChars];
  uint8_t queuedFor[kQueued];
  uint8_t queuedN;
  // the log, newest last
  char log[kLogLines][kLogChars];
  uint8_t logN;
  uint32_t logSerial;  // counts every line ever printed
  // the trip and the event under way
  Trip trip;
  ActiveEvent ev;
  bool roomChanged;  // Room.changed: the fire or the room changed while away
  // World.fightMove and World.danger: kept from one trip to the next, as the original keeps them
  uint8_t fightMove;
  bool danger;
  // the workers, in the order they first appeared (a death takes from them in this order)
  uint8_t workerOrder[WK_COUNT];
  uint8_t workerOrderN;
  // the ascent (a grid: space.js's asteroids, stepped for e-paper)
  struct {
    bool active;
    bool won;
    int16_t hull;
    int64_t startedAt;  // game ms at lift-off
    int64_t nextStep;   // game ms of the next step
    int64_t nextWave;   // game ms of the next wave of asteroids
    int16_t shipX, shipY;
    int16_t cols, rows;
    uint8_t n;
    Asteroid a[kMaxAsteroids];
    uint8_t hitFlash;  // steps the last hit still shows
  } space;
  // the ending
  bool ending;
};

}  // namespace adr
