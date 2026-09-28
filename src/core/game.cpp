// game.cpp — the engine (engine.js, state_manager.js, notifications.js), the room (room.js),
// the village (outside.js), the path (path.js), the fabricator, the ship and the prestige.
#include "game.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace adr {

// ---- randomness ---------------------------------------------------------------------------------

namespace {
uint32_t rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }
uint64_t splitmix(uint64_t& z) {
  uint64_t r = (z += 0x9E3779B97F4A7C15ull);
  r = (r ^ (r >> 30)) * 0xBF58476D1CE4E5B9ull;
  r = (r ^ (r >> 27)) * 0x94D049BB133111EBull;
  return r ^ (r >> 31);
}
}  // namespace

void Rng::seed(uint64_t v) {
  const uint64_t a = splitmix(v), b = splitmix(v);
  s[0] = static_cast<uint32_t>(a);
  s[1] = static_cast<uint32_t>(a >> 32);
  s[2] = static_cast<uint32_t>(b);
  s[3] = static_cast<uint32_t>(b >> 32) | 1;
}

uint32_t Rng::next() {
  const uint32_t result = rotl(s[1] * 5, 7) * 9;
  const uint32_t t = s[1] << 9;
  s[2] ^= s[0];
  s[3] ^= s[1];
  s[1] ^= s[2];
  s[0] ^= s[3];
  s[2] ^= t;
  s[3] = rotl(s[3], 11);
  return result;
}

// ---- the tables ---------------------------------------------------------------------------------

#define NO_COST {IT_NONE, 0, 0}
const Craftable kCrafts[kCraftCount] = {
    {"trap", BD_TRAP, IT_NONE, 10,
     "builder says she can make traps to catch any creatures might still be alive out there",
     "more traps to catch more creatures", "more traps won't help now", {{IT_WOOD, 10, 10}, NO_COST, NO_COST}},
    {"cart", BD_CART, IT_NONE, 1, "builder says she can make a cart for carrying wood",
     "the rickety cart will carry more wood from the forest", nullptr, {{IT_WOOD, 30, 0}, NO_COST, NO_COST}},
    {"hut", BD_HUT, IT_NONE, 20, "builder says there are more wanderers. says they'll work, too.",
     "builder puts up a hut, out in the forest. says word will get around.", "no more room for huts.",
     {{IT_WOOD, 100, 50}, NO_COST, NO_COST}},
    {"lodge", BD_LODGE, IT_NONE, 1, "villagers could help hunt, given the means",
     "the hunting lodge stands in the forest, a ways out of town", nullptr,
     {{IT_WOOD, 200, 0}, {IT_FUR, 10, 0}, {IT_MEAT, 5, 0}}},
    {"trading post", BD_TRADING_POST, IT_NONE, 1, "a trading post would make commerce easier",
     "now the nomads have a place to set up shop, they might stick around a while", nullptr,
     {{IT_WOOD, 400, 0}, {IT_FUR, 100, 0}, NO_COST}},
    {"tannery", BD_TANNERY, IT_NONE, 1, "builder says leather could be useful. says the villagers could make it.",
     "tannery goes up quick, on the edge of the village", nullptr, {{IT_WOOD, 500, 0}, {IT_FUR, 50, 0}, NO_COST}},
    {"smokehouse", BD_SMOKEHOUSE, IT_NONE, 1,
     "should cure the meat, or it'll spoil. builder says she can fix something up.",
     "builder finishes the smokehouse. she looks hungry.", nullptr, {{IT_WOOD, 600, 0}, {IT_MEAT, 50, 0}, NO_COST}},
    {"workshop", BD_WORKSHOP, IT_NONE, 1, "builder says she could make finer things, if she had the tools",
     "workshop's finally ready. builder's excited to get to it", nullptr,
     {{IT_WOOD, 800, 0}, {IT_LEATHER, 100, 0}, {IT_SCALES, 10, 0}}},
    {"steelworks", BD_STEELWORKS, IT_NONE, 1, "builder says the villagers could make steel, given the tools",
     "a haze falls over the village as the steelworks fires up", nullptr,
     {{IT_WOOD, 1500, 0}, {IT_IRON, 100, 0}, {IT_COAL, 100, 0}}},
    {"armoury", BD_ARMOURY, IT_NONE, 1, "builder says it'd be useful to have a steady source of bullets",
     "armoury's done, welcoming back the weapons of the past.", nullptr,
     {{IT_WOOD, 3000, 0}, {IT_STEEL, 100, 0}, {IT_SULPHUR, 50, 0}}},
    {"torch", BD_COUNT, IT_TORCH, 0, nullptr, "a torch to keep the dark away", nullptr,
     {{IT_WOOD, 1, 0}, {IT_CLOTH, 1, 0}, NO_COST}},
    {"waterskin", BD_COUNT, IT_WATERSKIN, 1, nullptr, "this waterskin'll hold a bit of water, at least", nullptr,
     {{IT_LEATHER, 50, 0}, NO_COST, NO_COST}},
    {"cask", BD_COUNT, IT_CASK, 1, nullptr, "the cask holds enough water for longer expeditions", nullptr,
     {{IT_LEATHER, 100, 0}, {IT_IRON, 20, 0}, NO_COST}},
    {"water tank", BD_COUNT, IT_WATER_TANK, 1, nullptr, "never go thirsty again", nullptr,
     {{IT_IRON, 100, 0}, {IT_STEEL, 50, 0}, NO_COST}},
    {"bone spear", BD_COUNT, IT_BONE_SPEAR, 0, nullptr, "this spear's not elegant, but it's pretty good at stabbing",
     nullptr, {{IT_WOOD, 100, 0}, {IT_TEETH, 5, 0}, NO_COST}},
    {"rucksack", BD_COUNT, IT_RUCKSACK, 1, nullptr, "carrying more means longer expeditions to the wilds", nullptr,
     {{IT_LEATHER, 200, 0}, NO_COST, NO_COST}},
    {"wagon", BD_COUNT, IT_WAGON, 1, nullptr, "the wagon can carry a lot of supplies", nullptr,
     {{IT_WOOD, 500, 0}, {IT_IRON, 100, 0}, NO_COST}},
    {"convoy", BD_COUNT, IT_CONVOY, 1, nullptr, "the convoy can haul mostly everything", nullptr,
     {{IT_WOOD, 1000, 0}, {IT_IRON, 200, 0}, {IT_STEEL, 100, 0}}},
    {"l armour", BD_COUNT, IT_L_ARMOUR, 1, nullptr, "leather's not strong. better than rags, though.", nullptr,
     {{IT_LEATHER, 200, 0}, {IT_SCALES, 20, 0}, NO_COST}},
    {"i armour", BD_COUNT, IT_I_ARMOUR, 1, nullptr, "iron's stronger than leather", nullptr,
     {{IT_LEATHER, 200, 0}, {IT_IRON, 100, 0}, NO_COST}},
    {"s armour", BD_COUNT, IT_S_ARMOUR, 1, nullptr, "steel's stronger than iron", nullptr,
     {{IT_LEATHER, 200, 0}, {IT_STEEL, 100, 0}, NO_COST}},
    {"iron sword", BD_COUNT, IT_IRON_SWORD, 0, nullptr, "sword is sharp. good protection out in the wilds.", nullptr,
     {{IT_WOOD, 200, 0}, {IT_LEATHER, 50, 0}, {IT_IRON, 20, 0}}},
    {"steel sword", BD_COUNT, IT_STEEL_SWORD, 0, nullptr, "the steel is strong, and the blade true.", nullptr,
     {{IT_WOOD, 500, 0}, {IT_LEATHER, 100, 0}, {IT_STEEL, 20, 0}}},
    {"rifle", BD_COUNT, IT_RIFLE, 0, nullptr, "black powder and bullets, like the old days.", nullptr,
     {{IT_WOOD, 200, 0}, {IT_STEEL, 50, 0}, {IT_SULPHUR, 50, 0}}},
};
#undef NO_COST

#define NC {IT_NONE, 0}
const TradeGood kTrades[kTradeCount] = {
    {IT_SCALES, 0, {{IT_FUR, 150}, NC, NC}},
    {IT_TEETH, 0, {{IT_FUR, 300}, NC, NC}},
    {IT_IRON, 0, {{IT_FUR, 150}, {IT_SCALES, 50}, NC}},
    {IT_COAL, 0, {{IT_FUR, 200}, {IT_TEETH, 50}, NC}},
    {IT_STEEL, 0, {{IT_FUR, 300}, {IT_SCALES, 50}, {IT_TEETH, 50}}},
    {IT_MEDICINE, 0, {{IT_SCALES, 50}, {IT_TEETH, 30}, NC}},
    {IT_BULLETS, 0, {{IT_SCALES, 10}, NC, NC}},
    {IT_ENERGY_CELL, 0, {{IT_SCALES, 10}, {IT_TEETH, 10}, NC}},
    {IT_BOLAS, 0, {{IT_TEETH, 10}, NC, NC}},
    {IT_GRENADE, 0, {{IT_SCALES, 100}, {IT_TEETH, 50}, NC}},
    {IT_BAYONET, 0, {{IT_SCALES, 500}, {IT_TEETH, 250}, NC}},
    {IT_ALIEN_ALLOY, 0, {{IT_FUR, 1500}, {IT_SCALES, 750}, {IT_TEETH, 300}}},
    {IT_COMPASS, 1, {{IT_FUR, 400}, {IT_SCALES, 20}, {IT_TEETH, 10}}},
};
#undef NC

const Fabricable kFabs[kFabCount] = {
    {IT_ENERGY_BLADE, 0, false, 1, 1, "the blade hums, charged particles sparking and fizzing."},
    {IT_FLUID_RECYCLER, 1, false, 1, 2, "water out, water in. waste not, want not."},
    {IT_CARGO_DRONE, 1, false, 1, 2, "the workhorse of the wanderer fleet."},
    {IT_KINETIC_ARMOUR, 1, true, 1, 2, "wanderer soldiers succeed by subverting the enemy's rage."},
    {IT_DISRUPTOR, 0, true, 1, 1, "somtimes it is best not to fight."},
    {IT_HYPO, 0, true, 5, 1, "a handful of hypos. life in a vial."},
    {IT_STIM, 0, true, 1, 1, "sometimes it is best to fight without restraint."},
    {IT_PLASMA_RIFLE, 0, true, 1, 1, "the peak of wanderer weapons technology, sleek and deadly."},
    {IT_GLOWSTONE, 0, true, 1, 1, "a smooth, perfect sphere. its light is inextinguishable."},
};

// ---- the clock and its timers -----------------------------------------------------------------

void Game::schedule(TimerKind k, int64_t ms, int16_t arg, bool skipDouble, int64_t period) {
  if (!skipDouble) {
    ms = hyperMs(ms);
    if (period) period = hyperMs(period);
  }
  for (Timer& t : s.timers) {
    if (t.kind == T_NONE) {
      t.kind = k;
      t.arg = arg;
      t.due = s.now + ms;
      t.period = period;
      return;
    }
  }
}

void Game::cancel(TimerKind k) {
  for (Timer& t : s.timers)
    if (t.kind == k) t.kind = T_NONE;
}

bool Game::scheduled(TimerKind k) const {
  for (const Timer& t : s.timers)
    if (t.kind == k) return true;
  return false;
}

int64_t Game::nextDue() const {
  int64_t best = INT64_MAX;
  for (const Timer& t : s.timers)
    if (t.kind != T_NONE && t.due < best) best = t.due;
  return best == INT64_MAX ? best : best - s.now;
}

void Game::advance(int64_t ms) {
  if (s.gameOver) return;
  const int64_t until = s.now + ms;
  for (;;) {
    int best = -1;
    for (int i = 0; i < kMaxTimers; ++i) {
      const Timer& t = s.timers[i];
      if (t.kind != T_NONE && t.due <= until && (best < 0 || t.due < s.timers[best].due)) best = i;
    }
    // the ascent steps on its own beat
    if (s.space.active && s.space.nextStep <= until && (best < 0 || s.space.nextStep <= s.timers[best].due)) {
      s.now = s.space.nextStep;
      s.space.nextStep += kSpaceStepMs;
      spaceStep();
      changed = true;
      if (s.gameOver) return;
      continue;
    }
    if (best < 0) break;
    const Timer t = s.timers[best];
    s.now = t.due;
    if (t.period > 0) s.timers[best].due += t.period;
    else s.timers[best].kind = T_NONE;
    fire(t);
    refresh();
    changed = true;
    if (s.gameOver) return;
  }
  s.now = until;
}

bool Game::takeChanged() {
  const bool c = changed;
  changed = false;
  return c;
}

void Game::startCooldown(CooldownId id, int seconds) {
  const int64_t ms = hyperMs(seconds * 1000LL);
  s.cd[id].end = s.now + ms;
  s.cd[id].total = static_cast<int32_t>(ms);
}

float Game::cooldownLeft(CooldownId id) const {
  const Cooldown& c = s.cd[id];
  if (c.end <= s.now || c.total <= 0) return 0;
  return static_cast<float>(c.end - s.now) / c.total;
}

void Game::fire(const Timer& t) {
  switch (t.kind) {
    case T_FIRE_COOL: coolFire(); break;
    case T_TEMP: adjustTemp(); break;
    case T_BUILDER: updateBuilderState(); break;
    case T_UNLOCK_FOREST: unlockForest(); break;
    case T_INCOME: collectIncome(); break;
    case T_POP: increasePopulation(); break;
    case T_EVENT: triggerEvent(); break;
    case T_WANDERER: {
      const bool wood = t.arg < 2;
      addStore(wood ? IT_WOOD : IT_FUR, (t.arg & 1) ? 1500 : 300);
      notify(M_ROOM, wood ? "the mysterious wanderer returns, cart piled high with wood."
                          : "the mysterious wanderer returns, cart piled high with furs.");
      break;
    }
    case T_DEATH_RETURN:
      s.active = M_ROOM;
      roomArrival();
      startCooldown(CD_EMBARK, 120);
      break;
    case T_ENEMY_ATTACK: enemyAttack(); break;
    case T_SPECIAL: {
      const Scene* sc = scene();
      if (!sc || t.arg >= sc->specialN) break;
      runHook(sc->specials[t.arg].action);
      break;
    }
    case T_DOT: dotDamage(t.arg != 0, s.ev.dotDmg); break;
    case T_STUN_END: s.ev.enemy.stunned = false; break;
    case T_STATUS_END: (t.arg ? s.ev.enemy : s.ev.wanderer).status = ST_NONE; break;
    case T_ENRAGE_END: {
      s.ev.enemy.status = ST_NONE;
      cancel(T_ENEMY_ATTACK);
      const Scene* sc = scene();
      if (sc) scheduleCombat(T_ENEMY_ATTACK, combatMs(sc->attackDelay), 0, true);
      break;
    }
    case T_EXPLODE: {
      s.ev.exploding = false;
      const Scene* sc = scene();
      if (!sc) break;
      damage(false, sc->explosion, WT_RANGED);
      if (!checkPlayerDeath()) winFight();
      break;
    }
    case T_WIN_FADE:
      if (s.ev.fought) break;
      s.ev.fought = true;
      cancelCombat();
      schedule(T_WIN_SHOW, 1300, 0, true);
      break;
    case T_WIN_SHOW: showLoot(); break;
    default: break;
  }
}

// ---- stores ($SM) ------------------------------------------------------------------------------

void Game::setStore(Item i, double v) {
  if (i >= kStoreCount) return;
  if (v > 99999999999999.0) v = 99999999999999.0;
  if (v < 0) v = 0;  // stores can not be negative
  s.stores[i] = v;
  s.storeSeen |= 1ull << i;
  changed = true;
}

void Game::setBuilding(Building b, int v) {
  s.buildings[b] = static_cast<int16_t>(v);
  s.buildingSeen |= static_cast<uint16_t>(1u << b);
  changed = true;
}

void Game::setWorker(Worker w, int v) {
  if (!(s.workerSeen >> w & 1) && s.workerOrderN < WK_COUNT) s.workerOrder[s.workerOrderN++] = w;
  s.workers[w] = v;
  s.workerSeen |= static_cast<uint16_t>(1u << w);
  changed = true;
}

void Game::addPerk(Perk p) {
  s.perks |= static_cast<uint16_t>(1u << p);
  notify(M_NONE, kPerks[p].notify);
}

bool Game::hasBlueprint(Item forItem) const {
  switch (forItem) {
    case IT_HYPO: return s.blueprints & 1;
    case IT_KINETIC_ARMOUR: return s.blueprints & 2;
    case IT_DISRUPTOR: return s.blueprints & 4;
    case IT_PLASMA_RIFLE: return s.blueprints & 8;
    case IT_STIM: return s.blueprints & 16;
    case IT_GLOWSTONE: return s.blueprints & 32;
    default: return false;
  }
}

void Game::setIncome(uint8_t src, int delay, const Item* items, const double* amounts, int n) {
  IncomeRec& r = s.income[src];
  if (!r.active) {
    r.timeLeft = 0;
    s.incomeOrder[s.incomeN++] = src;
  }
  r.active = true;
  r.delay = static_cast<uint8_t>(delay);
  r.n = static_cast<uint8_t>(n);
  for (int k = 0; k < n; ++k) {
    r.item[k] = items[k];
    r.amount[k] = amounts[k];
  }
  changed = true;
}

void Game::removeIncome(uint8_t src) {
  if (!s.income[src].active) return;
  s.income[src].active = false;
  int j = 0;
  for (int i = 0; i < s.incomeN; ++i)
    if (s.incomeOrder[i] != src) s.incomeOrder[j++] = s.incomeOrder[i];
  s.incomeN = static_cast<uint8_t>(j);
}

void Game::collectIncome() {
  if (s.active != M_SPACE) {
    for (int oi = 0; oi < s.incomeN; ++oi) {
      const uint8_t src = s.incomeOrder[oi];
      IncomeRec& inc = s.income[src];
      inc.timeLeft--;
      if (inc.timeLeft > 0) continue;
      if (src == SRC_THIEVES) addStolen(inc.item, inc.amount, inc.n);
      bool ok = true;
      if (src != SRC_THIEVES) {
        for (int k = 0; k < inc.n; ++k)
          if (store(inc.item[k]) + inc.amount[k] < 0) {
            ok = false;
            break;
          }
      }
      if (ok)
        for (int k = 0; k < inc.n; ++k) addStore(inc.item[k], inc.amount[k]);
      inc.timeLeft = inc.delay;
    }
  }
  schedule(T_INCOME, 1000);
}

void Game::startThieves() {
  s.thieves = 1;
  const Item items[3] = {IT_WOOD, IT_FUR, IT_MEAT};
  const double amounts[3] = {-10, -5, -5};
  setIncome(SRC_THIEVES, 10, items, amounts, 3);
}

void Game::addStolen(const Item* items, const double* amounts, int n) {
  for (int k = 0; k < n; ++k) {
    const double old = store(items[k]);
    const double shortBy = old + amounts[k];
    s.stolen[items[k]] += shortBy < 0 ? -amounts[k] + shortBy : -amounts[k];
  }
}

// ---- notifications ------------------------------------------------------------------------------

void Game::notify(Module m, const char* text, bool noQueue) {
  if (!text || !*text) return;
  char buf[kLogChars];
  size_t n = strlen(text);
  if (n > kLogChars - 2) n = kLogChars - 2;
  memcpy(buf, text, n);
  if (buf[n - 1] != '.') buf[n++] = '.';
  buf[n] = 0;
  if (m != M_NONE && s.active != m) {
    if (!noQueue && s.queuedN < kQueued) {
      memcpy(s.queued[s.queuedN], buf, n + 1);
      s.queuedFor[s.queuedN++] = m;
    }
  } else {
    printMessage(buf);
  }
}

void Game::printMessage(const char* text) {
  if (s.logN == kLogLines) {
    memmove(s.log[0], s.log[1], sizeof(s.log[0]) * (kLogLines - 1));
    s.logN--;
  }
  strncpy(s.log[s.logN], text, kLogChars - 1);
  s.log[s.logN][kLogChars - 1] = 0;
  s.logN++;
  s.logSerial++;
  changed = true;
}

// ---- the derived updates ------------------------------------------------------------------------

void Game::refresh() {
  if (refreshing) return;
  refreshing = true;
  // Room.updateStoresView: thieves, and the compass opening the path.
  if (s.thieves == 0 && s.hasWorld) {
    for (int i = 0; i < kStoreCount; ++i)
      if (storeSeen(static_cast<Item>(i)) && s.stores[i] > 5000) {
        startThieves();
        break;
      }
  }
  if (store(IT_COMPASS) > 0 && !s.pathDiscovery) {
    s.pathDiscovery = true;
    openPath();
  }
  // Room.updateBuildButtons.
  for (int i = 0; i < kCraftCount; ++i) {
    if (!kCrafts[i].key) continue;
    const bool max = kCrafts[i].maximum > 0 && craftCount(i) + 1 > kCrafts[i].maximum;
    if (!craftVisible(i)) {
      craftUnlockCheck(i);
    } else if (max && !(s.craftAtMax >> i & 1)) {
      notify(M_ROOM, kCrafts[i].maxMsg);
    }
    if (craftVisible(i)) {
      if (max) s.craftAtMax |= 1ull << i;
      else s.craftAtMax &= ~(1ull << i);
    }
  }
  if (s.buildings[BD_TRADING_POST] > 0) {
    for (int i = 0; i < kTradeCount; ++i)
      if (!tradeVisible(i) && (kTrades[i].item == IT_COMPASS || storeSeen(kTrades[i].item)))
        s.tradeUnlocked |= 1ull << i;
  }
  // Outside.updateVillage: the jobs the buildings open, and the first wanderers.
  if (s.hasOutside) {
    for (int b = 0; b < BD_COUNT; ++b)
      if (s.buildingSeen >> b & 1) checkWorker(static_cast<Building>(b));
    if (s.buildings[BD_HUT] > 0 && !scheduled(T_POP)) schedulePopIncrease();
    updateVillageIncome();
  }
  refreshing = false;
}

// ---- travel -------------------------------------------------------------------------------------

bool Game::unlocked(Module m) const {
  switch (m) {
    case M_ROOM: return true;
    case M_OUTSIDE: return s.hasOutside;
    case M_PATH: return s.hasPath;
    case M_FABRICATOR: return s.hasFabricator;
    case M_SHIP: return s.hasShip;
    default: return false;
  }
}

void Game::travelTo(Module m) {
  if (s.active == m || !unlocked(m)) return;
  if (s.ev.event >= 0) return;
  s.active = m;
  switch (m) {
    case M_ROOM: roomArrival(); break;
    case M_OUTSIDE: outsideArrival(); break;
    case M_PATH: break;
    case M_FABRICATOR:
      if (!s.fabricatorSeen) {
        notify(M_FABRICATOR, "the familiar hum of wanderer machinery coming to life. finally, real tools.");
        s.fabricatorSeen = true;
      }
      break;
    case M_SHIP:
      if (!s.seenShip) {
        notify(M_SHIP, "somewhere above the debris cloud, the wanderer fleet hovers. been on this rock too long.");
        s.seenShip = true;
      }
      break;
    default: break;
  }
  // Notifications.printQueue
  int j = 0;
  for (int i = 0; i < s.queuedN; ++i) {
    if (s.queuedFor[i] == m) printMessage(s.queued[i]);
    else {
      if (i != j) {
        memcpy(s.queued[j], s.queued[i], kLogChars);
        s.queuedFor[j] = s.queuedFor[i];
      }
      j++;
    }
  }
  s.queuedN = static_cast<uint8_t>(j);
  refresh();
  changed = true;
}

const char* Game::locationTitle(Module m) const {
  switch (m) {
    case M_ROOM: return s.fire < 2 ? "A Dark Room" : "A Firelit Room";
    case M_OUTSIDE: {
      const int huts = s.buildings[BD_HUT];
      if (huts == 0) return "A Silent Forest";
      if (huts == 1) return "A Lonely Hut";
      if (huts <= 4) return "A Tiny Village";
      if (huts <= 8) return "A Modest Village";
      if (huts <= 14) return "A Large Village";
      return "A Raucous Village";
    }
    case M_PATH: return "A Dusty Path";
    case M_FABRICATOR: return "A Whirring Fabricator";
    case M_SHIP: return "An Old Starship";
    case M_WORLD: return "A Barren World";
    case M_SPACE: return spaceTitle();
    default: return "";
  }
}

// ---- a new game ---------------------------------------------------------------------------------

void Game::newGame(uint64_t seed) {
  // Keep the prestige (Engine.deleteSave keeps Prestige.get()).
  int32_t prev[24];
  memcpy(prev, s.prevStores, sizeof(prev));
  const bool hasPrev = s.hasPrev;
  const int32_t prevScore = s.prevScore;
  const Settings settings = s.settings;
  memset(static_cast<void*>(&s), 0, sizeof(s));
  s.ev = ActiveEvent();
  s.trip = Trip();
  memcpy(s.prevStores, prev, sizeof(prev));
  s.hasPrev = hasPrev;
  s.prevScore = prevScore;
  s.settings = settings;
  r.seed(seed);
  s.active = M_NONE;
  s.builderLevel = -1;
  s.fire = 0;
  s.temperature = 0;
  s.thrusters = 1;
  // Events.init
  scheduleNextEvent();
  initRoom();
  travelTo(M_ROOM);
  changed = true;
}

void Game::resume(uint64_t seed) {
  // A little fresh randomness, so a reload does not replay the same rolls.
  uint64_t mix = seed;
  for (uint32_t v : r.s) mix = mix * 31 + v;
  r.seed(mix);
  refresh();
  changed = true;
}

void Game::setHyper(bool on) { s.settings.hyper = on; }

// ---- the room -----------------------------------------------------------------------------------

const char* Game::fireText(int v) {
  static const char* const k[] = {"dead", "smoldering", "flickering", "burning", "roaring"};
  return v >= 0 && v <= 4 ? k[v] : "";
}
const char* Game::tempText(int v) {
  static const char* const k[] = {"freezing", "cold", "mild", "warm", "hot"};
  return v >= 0 && v <= 4 ? k[v] : "";
}

void Game::initRoom() {
  schedule(T_FIRE_COOL, 5 * 60 * 1000);
  schedule(T_TEMP, 30 * 1000);
  schedule(T_INCOME, 1000);
  char buf[64];
  snprintf(buf, sizeof(buf), "the room is %s", tempText(s.temperature));
  notify(M_ROOM, buf);
  snprintf(buf, sizeof(buf), "the fire is %s", fireText(s.fire));
  notify(M_ROOM, buf);
}

void Game::roomArrival() {
  char buf[64];
  if (s.roomChanged) {
    snprintf(buf, sizeof(buf), "the fire is %s", fireText(s.fire));
    notify(M_ROOM, buf);
    snprintf(buf, sizeof(buf), "the room is %s", tempText(s.temperature));
    notify(M_ROOM, buf);
    s.roomChanged = false;
  }
  if (s.builderLevel == 3) {
    s.builderLevel = 4;
    const Item it = IT_WOOD;
    const double two = 2;
    setIncome(SRC_BUILDER, 10, &it, &two, 1);
    notify(M_ROOM, "the stranger is standing by the fire. she says she can help. says she builds things.");
  }
}

void Game::lightFire() {
  if (onCooldown(CD_FIRE)) return;
  startCooldown(CD_FIRE, 10);
  const bool seen = storeSeen(IT_WOOD);
  const double wood = store(IT_WOOD);
  if (seen && wood < 5) {
    notify(M_ROOM, "not enough wood to get the fire going");
    s.cd[CD_FIRE].end = 0;
    return;
  }
  if (seen) setStore(IT_WOOD, wood - 5);
  s.fire = 3;
  onFireChange();
  refresh();
}

void Game::stokeFire() {
  if (onCooldown(CD_FIRE)) return;
  startCooldown(CD_FIRE, 10);
  const bool seen = storeSeen(IT_WOOD);
  const double wood = store(IT_WOOD);
  if (seen && wood == 0) {
    notify(M_ROOM, "the wood has run out");
    s.cd[CD_FIRE].end = 0;
    return;
  }
  if (seen && wood > 0) setStore(IT_WOOD, wood - 1);
  if (s.fire < 4) s.fire++;
  onFireChange();
  refresh();
}

void Game::onFireChange() {
  if (s.active != M_ROOM) s.roomChanged = true;
  char buf[64];
  snprintf(buf, sizeof(buf), "the fire is %s", fireText(s.fire));
  notify(M_ROOM, buf, true);
  if (s.fire > 1 && s.builderLevel < 0) {
    s.builderLevel = 0;
    notify(M_ROOM, "the light from the fire spills from the windows, out into the dark");
    schedule(T_BUILDER, 30 * 1000);
  }
  cancel(T_FIRE_COOL);
  schedule(T_FIRE_COOL, 5 * 60 * 1000);
  changed = true;
}

void Game::coolFire() {
  const double wood = store(IT_WOOD);
  if (s.fire <= 2 && s.builderLevel > 3 && wood > 0) {
    notify(M_ROOM, "builder stokes the fire", true);
    setStore(IT_WOOD, wood - 1);
    s.fire++;
  }
  if (s.fire > 0) {
    s.fire--;
    onFireChange();  // which sets the next cooling
  }
}

void Game::adjustTemp() {
  const int old = s.temperature;
  char buf[64];
  if (s.temperature > 0 && s.temperature > s.fire) {
    s.temperature--;
    snprintf(buf, sizeof(buf), "the room is %s", tempText(s.temperature));
    notify(M_ROOM, buf, true);
  }
  if (s.temperature < 4 && s.temperature < s.fire) {
    s.temperature++;
    snprintf(buf, sizeof(buf), "the room is %s", tempText(s.temperature));
    notify(M_ROOM, buf, true);
  }
  if (s.temperature != old) s.roomChanged = true;
  schedule(T_TEMP, 30 * 1000);
}

void Game::unlockForest() {
  setStore(IT_WOOD, 4);
  initOutside();
  notify(M_ROOM, "the wind howls outside");
  notify(M_ROOM, "the wood is running out");
}

void Game::updateBuilderState() {
  int level = s.builderLevel;
  if (level == 0) {
    notify(M_ROOM, "a ragged stranger stumbles through the door and collapses in the corner");
    level = s.builderLevel = 1;
    schedule(T_UNLOCK_FOREST, 15 * 1000);
  } else if (level < 3 && s.temperature >= 3) {
    if (level == 1) notify(M_ROOM, "the stranger shivers, and mumbles quietly. her words are unintelligible.");
    else if (level == 2) notify(M_ROOM, "the stranger in the corner stops shivering. her breathing calms.");
    level = ++s.builderLevel;
  }
  if (level < 3) schedule(T_BUILDER, 30 * 1000);
}

int Game::craftCount(int i) const {
  const Craftable& c = kCrafts[i];
  return c.building != BD_COUNT ? s.buildings[c.building] : static_cast<int>(store(c.item));
}

bool Game::craftAtMax(int i) const { return kCrafts[i].maximum > 0 && craftCount(i) + 1 > kCrafts[i].maximum; }

void Game::craftCost(int i, Amount out[3], int& n) const {
  const Craftable& c = kCrafts[i];
  n = 0;
  const int built = c.building != BD_COUNT ? s.buildings[c.building] : 0;
  for (const auto& k : c.cost)
    if (k.item != IT_NONE) out[n++] = {k.item, k.base + k.perEach * built};
}

bool Game::craftUnlockCheck(int i) {
  if (craftVisible(i)) return true;
  if (s.builderLevel < 4) return false;
  const Craftable& c = kCrafts[i];
  const bool needsWorkshop = c.item != IT_NONE;
  if (needsWorkshop && s.buildings[BD_WORKSHOP] == 0) return false;
  Amount cost[3];
  int n;
  craftCost(i, cost, n);
  if (c.building != BD_COUNT && s.buildings[c.building] > 0) {
    s.craftUnlocked |= 1ull << i;
    return true;
  }
  for (int k = 0; k < n; ++k)
    if (cost[k].item == IT_WOOD && store(IT_WOOD) < cost[k].n * 0.5) return false;
  for (int k = 0; k < n; ++k)
    if (!(store(cost[k].item) != 0)) return false;
  s.craftUnlocked |= 1ull << i;
  if (!(c.building != BD_COUNT && s.buildings[c.building])) notify(M_ROOM, c.availableMsg);
  return true;
}

bool Game::tradeAtMax(int i) const {
  return kTrades[i].maximum > 0 && store(kTrades[i].item) + 1 > kTrades[i].maximum;
}

void Game::build(int i) {
  if (i < 0 || i >= kCraftCount || !kCrafts[i].key) return;
  if (s.temperature <= 1) {
    notify(M_ROOM, "builder just shivers");
    return;
  }
  const Craftable& c = kCrafts[i];
  if (c.maximum > 0 && c.maximum <= craftCount(i)) return;
  Amount cost[3];
  int n;
  craftCost(i, cost, n);
  for (int k = 0; k < n; ++k) {
    if (store(cost[k].item) < cost[k].n) {
      char buf[64];
      snprintf(buf, sizeof(buf), "not enough %s", itemName(cost[k].item));
      notify(M_ROOM, buf);
      return;
    }
  }
  for (int k = 0; k < n; ++k) setStore(cost[k].item, store(cost[k].item) - cost[k].n);
  notify(M_ROOM, c.buildMsg);
  if (c.building != BD_COUNT) setBuilding(c.building, s.buildings[c.building] + 1);
  else addStore(c.item, 1);
  refresh();
}

void Game::buy(int i) {
  if (i < 0 || i >= kTradeCount) return;
  const TradeGood& g = kTrades[i];
  if (g.maximum > 0 && g.maximum <= store(g.item)) return;
  for (const auto& k : g.cost) {
    if (k.item == IT_NONE) continue;
    if (store(k.item) < k.n) {
      char buf[64];
      snprintf(buf, sizeof(buf), "not enough %s", itemName(k.item));
      notify(M_ROOM, buf);
      return;
    }
  }
  for (const auto& k : g.cost)
    if (k.item != IT_NONE) setStore(k.item, store(k.item) - k.n);
  addStore(g.item, 1);
  refresh();
}

// ---- the village --------------------------------------------------------------------------------

void Game::initOutside() {
  if (!s.hasOutside) s.hasOutside = true;
  refresh();
}

void Game::outsideArrival() {
  if (!s.seenForest) {
    notify(M_OUTSIDE, "the sky is grey and the wind blows relentlessly");
    s.seenForest = true;
  }
}

int Game::numGatherers() const {
  int n = s.population;
  for (int w = 1; w < WK_COUNT; ++w)
    if (s.workerSeen >> w & 1) n -= s.workers[w];
  return n;
}

void Game::schedulePopIncrease() {
  const double minutes = std::floor(r.real() * (3 - 0.5)) + 0.5;
  schedule(T_POP, static_cast<int64_t>(minutes * 60 * 1000));
}

void Game::increasePopulation() {
  const int space = maxPopulation() - s.population;
  if (space > 0) {
    int num = static_cast<int>(std::floor(r.real() * (space / 2.0) + space / 2.0));
    if (num == 0) num = 1;
    if (num == 1) notify(M_NONE, "a stranger arrives in the night");
    else if (num < 5) notify(M_NONE, "a weathered family takes up in one of the huts.");
    else if (num < 10) notify(M_NONE, "a small group arrives, all dust and bones.");
    else if (num < 30) notify(M_NONE, "a convoy lurches in, equal parts worry and hope.");
    else notify(M_NONE, "the town's booming. word does get around.");
    s.population += num;
    changed = true;
  }
  schedulePopIncrease();
}

void Game::killVillagers(int num) {
  s.population -= num;
  if (s.population < 0) s.population = 0;
  int remaining = numGatherers();
  if (remaining < 0) {
    int gap = -remaining;
    for (int k = 0; k < s.workerOrderN; ++k) {
      const int w = s.workerOrder[k];
      if (s.workers[w] < gap) {
        gap -= s.workers[w];
        s.workers[w] = 0;
      } else {
        s.workers[w] -= gap;
        break;
      }
    }
  }
  changed = true;
}

int Game::destroyHuts(int num, bool allowEmpty) {
  int dead = 0;
  for (int i = 0; i < num; ++i) {
    const int population = s.population;
    const double rate = population / 4.0;
    const int full = static_cast<int>(std::floor(rate));
    const int huts = allowEmpty ? s.buildings[BD_HUT] : static_cast<int>(std::ceil(rate));
    if (!huts) break;
    const int target = static_cast<int>(std::floor(r.real() * huts)) + 1;
    int inhabitants = 0;
    if (target <= full) inhabitants = 4;
    else if (target == full + 1) inhabitants = population % 4;
    setBuilding(BD_HUT, s.buildings[BD_HUT] - 1);
    if (inhabitants) {
      killVillagers(inhabitants);
      dead += inhabitants;
    }
  }
  return dead;
}

bool Game::checkWorker(Building b) {
  bool added = false;
  for (int w = 1; w < WK_COUNT; ++w) {
    if (kWorkers[w].building != b) continue;
    if (!(s.workerSeen >> w & 1)) {
      setWorker(static_cast<Worker>(w), 0);
      added = true;
    }
  }
  return added;
}

void Game::updateVillageIncome() {
  for (int w = 0; w < WK_COUNT; ++w) {
    int num;
    if (w == WK_GATHERER) num = numGatherers();
    else if (s.workerSeen >> w & 1) num = s.workers[w];
    else continue;
    if (num < 0) num = 0;
    const WorkerInfo& info = kWorkers[w];
    Item items[3];
    double amounts[3];
    int n = 0;
    bool needsUpdate = !s.income[w].active;
    for (const auto& k : info.stores) {
      if (k.item == IT_NONE) continue;
      items[n] = k.item;
      amounts[n] = k.n2 / 2.0 * num;
      if (s.income[w].active && (n >= s.income[w].n || s.income[w].amount[n] != amounts[n])) needsUpdate = true;
      n++;
    }
    if (needsUpdate) setIncome(static_cast<uint8_t>(w), info.delay, items, amounts, n);
  }
}

void Game::adjustWorker(Worker w, int delta) {
  if (w == WK_GATHERER || !(s.workerSeen >> w & 1)) return;
  if (delta > 0) {
    const int free = numGatherers();
    if (free > 0) s.workers[w] += delta < free ? delta : free;
  } else if (s.workers[w] > 0) {
    const int d = -delta < s.workers[w] ? -delta : s.workers[w];
    s.workers[w] -= d;
  }
  changed = true;
  refresh();
}

void Game::gatherWood() {
  if (onCooldown(CD_GATHER)) return;
  startCooldown(CD_GATHER, 60);
  notify(M_OUTSIDE, "dry brush and dead branches litter the forest floor");
  addStore(IT_WOOD, s.buildings[BD_CART] > 0 ? 50 : 10);
  refresh();
}

void Game::checkTraps() {
  if (onCooldown(CD_TRAPS) || s.buildings[BD_TRAP] <= 0) return;
  startCooldown(CD_TRAPS, 90);
  struct Drop {
    double under;
    Item item;
    const char* message;
  };
  static const Drop kDrops[] = {
      {0.5, IT_FUR, "scraps of fur"},       {0.75, IT_MEAT, "bits of meat"},
      {0.85, IT_SCALES, "strange scales"},  {0.93, IT_TEETH, "scattered teeth"},
      {0.995, IT_CLOTH, "tattered cloth"},  {1.0, IT_CHARM, "a crudely made charm"},
  };
  int drops[6] = {};
  int order[6], orderN = 0;
  const int numTraps = s.buildings[BD_TRAP];
  const int numBait = static_cast<int>(store(IT_BAIT));
  const int numDrops = numTraps + (numBait < numTraps ? numBait : numTraps);
  for (int i = 0; i < numDrops; ++i) {
    const double roll = r.real();
    for (int j = 0; j < 6; ++j) {
      if (roll < kDrops[j].under) {
        if (drops[j] == 0) order[orderN++] = j;
        drops[j]++;
        break;
      }
    }
  }
  char msg[kLogChars] = "the traps contain ";
  for (int l = 0; l < orderN; ++l) {
    if (orderN > 1 && l > 0 && l < orderN - 1) strncat(msg, ", ", sizeof(msg) - strlen(msg) - 1);
    else if (orderN > 1 && l == orderN - 1) strncat(msg, " and ", sizeof(msg) - strlen(msg) - 1);
    strncat(msg, kDrops[order[l]].message, sizeof(msg) - strlen(msg) - 1);
  }
  const int baitUsed = numBait < numTraps ? numBait : numTraps;
  notify(M_OUTSIDE, msg);
  for (int j = 0; j < 6; ++j)
    if (drops[j]) addStore(kDrops[j].item, drops[j]);
  addStore(IT_BAIT, -baitUsed);
  refresh();
}

// ---- the path -----------------------------------------------------------------------------------

void Game::openPath() {
  initWorld();
  s.hasPath = true;
  char buf[64];
  snprintf(buf, sizeof(buf), "the compass points %s", compassDir());
  notify(M_ROOM, buf);
}

int Game::capacity() const {
  if (store(IT_CARGO_DRONE) > 0) return 10 + 100;
  if (store(IT_CONVOY) > 0) return 10 + 60;
  if (store(IT_WAGON) > 0) return 10 + 30;
  if (store(IT_RUCKSACK) > 0) return 10 + 10;
  return 10;
}

double Game::freeSpace() const {
  double used = 0;
  for (int i = 0; i < kStoreCount; ++i)
    if (s.outfit[i] > 0) used += s.outfit[i] * weight(static_cast<Item>(i));
  return capacity() - used;
}

bool Game::carryable(Item i) {
  switch (i) {
    case IT_CURED_MEAT: case IT_BULLETS: case IT_GRENADE: case IT_BOLAS: case IT_LASER_RIFLE:
    case IT_ENERGY_CELL: case IT_BAYONET: case IT_CHARM: case IT_ALIEN_ALLOY: case IT_MEDICINE:
    case IT_TORCH: case IT_BONE_SPEAR: case IT_IRON_SWORD: case IT_STEEL_SWORD: case IT_RIFLE:
    case IT_ENERGY_BLADE: case IT_DISRUPTOR: case IT_HYPO: case IT_STIM: case IT_PLASMA_RIFLE:
    case IT_GLOWSTONE:
      return true;
    default: return false;
  }
}

int Game::maxWater() const {
  if (store(IT_FLUID_RECYCLER) > 0) return 10 + 100;
  if (store(IT_WATER_TANK) > 0) return 10 + 50;
  if (store(IT_CASK) > 0) return 10 + 20;
  if (store(IT_WATERSKIN) > 0) return 10 + 10;
  return 10;
}

int Game::maxHealth() const {
  if (store(IT_KINETIC_ARMOUR) > 0) return 10 + 75;
  if (store(IT_S_ARMOUR) > 0) return 10 + 35;
  if (store(IT_I_ARMOUR) > 0) return 10 + 15;
  if (store(IT_L_ARMOUR) > 0) return 10 + 5;
  return 10;
}

const char* Game::armourName() const {
  if (store(IT_KINETIC_ARMOUR) > 0) return "kinetic";
  if (store(IT_S_ARMOUR) > 0) return "steel";
  if (store(IT_I_ARMOUR) > 0) return "iron";
  if (store(IT_L_ARMOUR) > 0) return "leather";
  return "none";
}

void Game::adjustOutfit(Item i, int delta) {
  if (i >= kStoreCount || !carryable(i)) return;
  const int cur = s.outfit[i];
  const int have = static_cast<int>(store(i));
  if (delta > 0) {
    const double w = weight(i);
    if (freeSpace() >= w && cur < have) {
      const int byWeight = static_cast<int>(std::floor(freeSpace() / w + 1e-9));
      const int byStore = have - cur;
      int add = delta;
      if (byWeight < add) add = byWeight;
      if (byStore < add) add = byStore;
      s.outfit[i] = cur + add;
    }
  } else if (cur > 0) {
    s.outfit[i] = cur + delta < 0 ? 0 : cur + delta;
  }
  changed = true;
}

void Game::embark() {
  if (!canEmbark() || onCooldown(CD_EMBARK) || s.active != M_PATH) return;
  // the outfit never holds more than the stores do
  for (int i = 0; i < kStoreCount; ++i)
    if (s.outfit[i] > store(static_cast<Item>(i))) s.outfit[i] = static_cast<int32_t>(store(static_cast<Item>(i)));
  for (int i = 0; i < kStoreCount; ++i)
    if (s.outfit[i]) addStore(static_cast<Item>(i), -s.outfit[i]);
  s.active = M_WORLD;
  worldArrival();
  refresh();
}

// ---- the fabricator and the ship ----------------------------------------------------------------

void Game::initFabricator() {
  s.hasFabricator = true;
  changed = true;
}

bool Game::fabVisible(int i) const { return !kFabs[i].blueprint || hasBlueprint(kFabs[i].item); }
bool Game::fabAtMax(int i) const { return kFabs[i].maximum > 0 && store(kFabs[i].item) + 1 > kFabs[i].maximum; }

void Game::fabricate(int i) {
  if (i < 0 || i >= kFabCount || !fabVisible(i) || fabAtMax(i)) return;
  const Fabricable& f = kFabs[i];
  if (store(IT_ALIEN_ALLOY) < f.alloy) {
    notify(M_FABRICATOR, "not enough alien alloy");
    return;
  }
  setStore(IT_ALIEN_ALLOY, store(IT_ALIEN_ALLOY) - f.alloy);
  addStore(f.item, f.quantity);
  notify(M_FABRICATOR, f.buildMsg);
  refresh();
}

void Game::initShip() {
  if (!s.hasShip) {
    s.hasShip = true;
    s.hull = 0;
    s.thrusters = 1;
  }
  changed = true;
}

void Game::reinforceHull() {
  if (store(IT_ALIEN_ALLOY) < 1) {
    notify(M_SHIP, "not enough alien alloy");
    return;
  }
  addStore(IT_ALIEN_ALLOY, -1);
  s.hull++;
  refresh();
}

void Game::upgradeEngine() {
  if (store(IT_ALIEN_ALLOY) < 1) {
    notify(M_SHIP, "not enough alien alloy");
    return;
  }
  addStore(IT_ALIEN_ALLOY, -1);
  s.thrusters++;
  refresh();
}

void Game::liftOff() {
  if (s.hull <= 0 || onCooldown(CD_LIFTOFF)) return;
  s.seenWarning = true;
  startCooldown(CD_LIFTOFF, 120);
  spaceArrival();
}

// ---- prestige and score -------------------------------------------------------------------------

namespace {
struct PrestigeSlot {
  Item item;
  char type;
};
const PrestigeSlot kPrestige[24] = {
    {IT_WOOD, 'g'}, {IT_FUR, 'g'}, {IT_MEAT, 'g'}, {IT_IRON, 'g'}, {IT_COAL, 'g'}, {IT_SULPHUR, 'g'},
    {IT_STEEL, 'g'}, {IT_CURED_MEAT, 'g'}, {IT_SCALES, 'g'}, {IT_TEETH, 'g'}, {IT_LEATHER, 'g'},
    {IT_BAIT, 'g'}, {IT_TORCH, 'g'}, {IT_CLOTH, 'g'}, {IT_BONE_SPEAR, 'w'}, {IT_IRON_SWORD, 'w'},
    {IT_STEEL_SWORD, 'w'}, {IT_BAYONET, 'w'}, {IT_RIFLE, 'w'}, {IT_LASER_RIFLE, 'w'}, {IT_BULLETS, 'a'},
    {IT_ENERGY_CELL, 'a'}, {IT_GRENADE, 'a'}, {IT_BOLAS, 'a'},
};
}  // namespace

void Game::prestigeStores(int32_t out[24], bool reduce) {
  for (int i = 0; i < 24; ++i) {
    int div = 1;
    if (reduce) {
      int amount;
      switch (kPrestige[i].type) {
        case 'g': amount = static_cast<int>(std::floor(r.real() * 10)); break;
        case 'w': amount = static_cast<int>(std::floor(std::floor(r.real() * 10) / 2)); break;
        default: amount = static_cast<int>(std::ceil(r.real() * 10 * std::ceil(r.real() * 10))); break;
      }
      div = amount ? amount : 1;
    }
    out[i] = static_cast<int32_t>(std::floor(store(kPrestige[i].item) / div));
  }
}

int32_t Game::score() const {
  static const double kFactor[24] = {1, 1.5, 1, 2, 2, 3, 3, 2, 2, 2, 2, 1.5, 1, 1, 10, 30, 50, 100, 150, 150, 3, 3, 5, 4};
  double full = 0;
  for (int i = 0; i < 24; ++i) full += std::floor(store(kPrestige[i].item)) * kFactor[i];
  full += store(IT_ALIEN_ALLOY) * 10;
  full += store(IT_FLEET_BEACON) * 500;
  full += s.hull * 50;
  return static_cast<int32_t>(std::floor(full));
}

void Game::collectPrestige() {
  if (!s.hasPrev) return;
  for (int i = 0; i < 24; ++i) addStore(kPrestige[i].item, s.prevStores[i]);
  memset(s.prevStores, 0, sizeof(s.prevStores));
  refresh();
}

void Game::restart() {
  newGame(static_cast<uint64_t>(r.next()) << 32 | r.next());
}

}  // namespace adr
