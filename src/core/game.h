// game.h — A Dark Room's rules, ported from script/*.js to run on the device.
//
// Game owns the state and the game clock. Time only passes when advance() is called, so the
// firmware pauses the world by not calling it (asleep, or in the menu) and the host tests run
// hours of it in a moment. Everything the original did on a setTimeout is a named timer in the
// state (state.h TimerKind), so a save taken at any moment resumes exactly.
//
// The screens (ui/) read the state and call the actions below; nothing here draws.
#pragma once

#include <cstdint>

#include "events.h"
#include "state.h"

namespace adr {

// Math.random(), and anything else the game rolls: xoshiro128**.
class Rng {
 public:
  void seed(uint64_t s);
  uint32_t next();
  double real() { return (next() >> 8) * (1.0 / 16777216.0); }  // [0, 1)
  uint32_t s[4] = {1, 2, 3, 4};
};

// Room.Craftables: the builder's buildings, then the workshop's crafts.
struct Craftable {
  const char* key;
  Building building;  // BD_COUNT when it is an item
  Item item;          // IT_NONE when it is a building
  int16_t maximum;    // 0: none
  const char* availableMsg;
  const char* buildMsg;
  const char* maxMsg;
  struct { Item item; int32_t base, perEach; } cost[3];
};
constexpr int kCraftCount = 24;
extern const Craftable kCrafts[kCraftCount];

// Room.TradeGoods.
struct TradeGood {
  Item item;
  int16_t maximum;
  struct { Item item; int32_t n; } cost[3];
};
constexpr int kTradeCount = 13;
extern const TradeGood kTrades[kTradeCount];

// Fabricator.Craftables.
struct Fabricable {
  Item item;
  int16_t maximum;
  bool blueprint;
  int16_t quantity;
  int16_t alloy;
  const char* buildMsg;
};
constexpr int kFabCount = 9;
extern const Fabricable kFabs[kFabCount];

// The landmarks (world.js LANDMARKS).
struct Landmark {
  Tile tile;
  uint8_t num;
  uint8_t minRadius, maxRadius;
  const char* scene;  // a kEvents key
  const char* label;
};
extern const Landmark kLandmarks[];
extern const int kLandmarkCount;
const Landmark* landmarkFor(uint8_t tile);  // the tile as it is on the map (visited: none)

enum Dir : uint8_t { DIR_NORTH, DIR_SOUTH, DIR_WEST, DIR_EAST };

// The combat screen's other buttons, after the weapons.
enum CombatButton : uint8_t {
  CB_EAT = kWeaponCount, CB_MEDS, CB_HYPO, CB_STIM, CB_SHIELD, CB_LEAVE, CB_COUNT
};

class Game {
 public:
  // A new game (keeping any prestige carried in `prestige`, a finished game's state).
  void newGame(uint64_t seed);
  // After loading a save: rebuild what is derived, reschedule nothing (the timers came with it).
  void resume(uint64_t seed);

  // The clock. `ms` of real time; hyper mode is applied where the original applied it.
  void advance(int64_t ms);
  // True once something the screens show has changed since the last call.
  bool takeChanged();
  // How far, in game ms, the next timer is (for a firmware that wants to sleep until then).
  int64_t nextDue() const;

  GameState& st() { return s; }
  const GameState& st() const { return s; }
  Rng& rng() { return r; }

  // ---- queries --------------------------------------------------------------------------------
  double store(Item i) const { return i < kStoreCount ? s.stores[i] : 0; }
  bool storeSeen(Item i) const { return i < kStoreCount && (s.storeSeen >> i & 1); }
  int building(Building b) const { return s.buildings[b]; }
  bool hasPerk(Perk p) const { return s.perks >> p & 1; }
  bool hasBlueprint(Item bpFor) const;
  int numGatherers() const;
  int maxPopulation() const { return s.buildings[BD_HUT] * 4; }
  bool unlocked(Module m) const;
  Module activeModule() const { return static_cast<Module>(s.active); }
  const char* locationTitle(Module m) const;
  const char* spaceTitle() const;
  // The room's fire and warmth, as the original words them.
  static const char* fireText(int v);
  static const char* tempText(int v);

  // A main button's cooldown: 0 when ready, else the fraction left (0..1].
  float cooldownLeft(CooldownId id) const;
  bool onCooldown(CooldownId id) const { return cooldownLeft(id) > 0; }

  // The room's build buttons.
  bool craftVisible(int i) const { return s.craftUnlocked >> i & 1; }
  int craftCount(int i) const;
  bool craftAtMax(int i) const;
  void craftCost(int i, Amount out[3], int& n) const;
  bool tradeVisible(int i) const { return s.tradeUnlocked >> i & 1; }
  bool tradeAtMax(int i) const;
  bool fabVisible(int i) const;
  bool fabAtMax(int i) const;

  // The path.
  int capacity() const;
  double freeSpace() const;  // capacity less what the outfit weighs
  static double weight(Item i) { return kItems[i].weight10 / 10.0; }
  static bool carryable(Item i);
  int maxWater() const;
  int maxHealth() const;
  const char* armourName() const;
  bool canEmbark() const { return s.outfit[IT_CURED_MEAT] > 0; }
  int meatHeal() const { return 8 * (hasPerk(PK_GASTRONOME) ? 2 : 1); }

  // The world.
  int distance(int x, int y) const;
  int distance() const { return distance(s.trip.x, s.trip.y); }
  uint8_t terrain() const { return s.trip.world.tile[s.trip.x][s.trip.y]; }
  const char* compassDir() const;

  // The event on the glass.
  const EventDef* event() const { return s.ev.event >= 0 ? &kEvents[s.ev.event] : nullptr; }
  const Scene* scene() const { return event() ? &event()->scenes[s.ev.scene] : nullptr; }
  bool buttonAvailable(int i) const;       // the scene's button i: available and affordable
  float buttonCooldownLeft(int i) const;   // 0 when ready
  bool combatButtonShown(int b) const;
  bool combatButtonEnabled(int b) const;
  float combatCooldownLeft(int b) const;
  const char* combatButtonText(int b) const;
  int quantity(Item i) const;  // Events.getQuantity: the outfit abroad, the stores at home
  bool canTakeAnything() const;
  bool canTakeEverything() const;
  // The one scene button that leaves, when the scene has just one (Events.drawButtons' return).
  int soleButton() const;

  // The ascent.
  const char* altitudeName() const;
  int altitude() const { return static_cast<int>((s.now - s.space.startedAt) / 1000); }

  // Score (scoring.js).
  int32_t score() const;

  // ---- actions (the original's click handlers) ------------------------------------------------
  void travelTo(Module m);
  void lightFire();
  void stokeFire();
  void gatherWood();
  void checkTraps();
  void build(int craft);
  void buy(int trade);
  void fabricate(int fab);
  void adjustWorker(Worker w, int delta);
  void adjustOutfit(Item i, int delta);
  void embark();
  void move(Dir d);
  void reinforceHull();
  void upgradeEngine();
  void liftOff();  // asks first, the first time
  void setHyper(bool on);

  // Events.
  void chooseButton(int i);
  void combatButton(int b);
  void takeLoot(int row, bool all);
  // The drop menu: what could be left behind to make room for loot row `row`.
  struct DropOption { Item item; int n; };
  int dropOptions(int row, DropOption out[kStoreCount]) const;
  void dropAndTake(int row, Item drop, int n);
  void takeEverything();

  // The ascent: a press moves the ship a cell at once; a held key (-1, 0 or +1 on each axis)
  // moves it on every step, as far as its thrusters allow.
  static constexpr int kSpaceStepMs = 500;
  void spaceNudge(int dx, int dy);
  void spaceSteer(int dx, int dy) { steerX = dx; steerY = dy; }
  void spaceLayout(int cols, int rows);
  int spaceSpeed() const;

  // The ending.
  void restart();  // keeps the prestige

  // The map the world's seed grows (a save keeps only what differs from it).
  void baseMap(WorldMap& m) { generateMap(s.mapSeed, m); }

  // Debugging and the host tests.
  void notify(Module m, const char* text, bool noQueue = false);
  void debugGive(Item i, double n) { addStore(i, n); }
  void debugEvent(int index) { startEvent(index); }
  void debugScene(int scene) { loadScene(scene); }

 private:
  friend struct Hooks;
  GameState s;
  Rng r;
  bool changed = true;
  int steerX = 0, steerY = 0;

  // stores and friends ($SM)
  void setStore(Item i, double v);
  void addStore(Item i, double v) { setStore(i, (storeSeen(i) ? s.stores[i] : 0) + v); }
  void setBuilding(Building b, int v);
  void setWorker(Worker w, int v);
  void addPerk(Perk p);
  void setIncome(uint8_t src, int delay, const Item* items, const double* amounts, int n);
  void removeIncome(uint8_t src);

  // timers
  void schedule(TimerKind k, int64_t ms, int16_t arg = 0, bool skipDouble = false, int64_t period = 0);
  void scheduleCombat(TimerKind k, int64_t ms, int16_t arg = 0, bool interval = false);
  void cancel(TimerKind k);
  void cancelCombat();
  bool scheduled(TimerKind k) const;
  void fire(const Timer& t);
  void startCooldown(CooldownId id, int seconds);
  int64_t hyperMs(int64_t ms) const { return s.settings.hyper ? ms / 2 : ms; }
  int64_t combatMs(double seconds) const;

  // the derived updates the original ran on every state change
  void refresh();
  bool refreshing = false;

  void printMessage(const char* text);

  // room.js
  void initRoom();
  void roomArrival();
  void onFireChange();
  void coolFire();
  void adjustTemp();
  void updateBuilderState();
  void unlockForest();
  bool craftUnlockCheck(int i);
  // outside.js
  void initOutside();
  void outsideArrival();
  void increasePopulation();
  void schedulePopIncrease();
  void killVillagers(int n);
  int destroyHuts(int n, bool allowEmpty = false);
  void updateVillageIncome();
  bool checkWorker(Building b);
  // state_manager.js
  void collectIncome();
  void startThieves();
  void addStolen(const Item* items, const double* amounts, int n);
  // path.js / world.js
  void openPath();
  void initWorld();
  void generateMap(uint32_t seed, WorldMap& m);
  int chooseTile(int x, int y, const WorldMap& m);
  void placeLandmark(int minR, int maxR, uint8_t tile, WorldMap& m);
  void lightMap(int x, int y, WorldMap& m);
  void uncoverMap(int x, int y, int radius, WorldMap& m);
  void testMap();
  void applyMap();
  void worldArrival();
  void narrateMove(uint8_t from, uint8_t to);
  bool checkDanger();
  bool useSupplies();
  void checkFight();
  void doSpace();
  void goHome();
  void die();
  void setWater(int w);
  void setHp(int hp);
  void clearDungeon();
  void drawRoad();
  void markVisited() { s.trip.world.tile[s.trip.x][s.trip.y] |= kVisited; }
  void useOutpost();
  void returnOutfit();
  void redeemBlueprints();
  void initShip();
  void initFabricator();
  // events.js
  void scheduleNextEvent(double scale = 1);
  void triggerEvent();
  void triggerFight();
  void startEvent(int index);
  void switchEvent(int index);
  void loadScene(int scene);
  void endEvent();
  bool isAvailable(Hook h) const;
  void runHook(Hook h);
  void startCombat();
  void enemyAttack();
  void useWeapon(int w);
  void doHeal(Item what, int amount, int button);
  void damage(bool byWanderer, int dmg, WeaponType type);
  void dotDamage(bool onWanderer, int dmg);
  bool checkPlayerDeath();
  void setStatus(bool wanderer, uint8_t status);
  void winFight();
  void showLoot();
  void drawLoot(const LootEntry* loot, int n);
  void canLeave();  // Events.canLeave
  void floatText(bool onWanderer, const char* text);
  // space.js
  void spaceArrival();
  void spaceStep();
  void spaceWave();
  void spaceHit();
  void crash();
  void endGame();
  // prestige.js
  void prestigeStores(int32_t out[24], bool reduce);
  void collectPrestige();
};

}  // namespace adr
