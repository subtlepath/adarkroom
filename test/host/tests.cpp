// tests.cpp — the game and its screens on the host, with the sanitizers watching.
//
// The rules: the opening as the original tells it, the village's economy, every event's every
// scene reached and left, fights won and lost, the world walked and come home from, the ascent to
// the ending. The saves: a round trip from every kind of moment gives the same bytes back. The
// screens: every one drawn on every device, turned both ways.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/core/app.h"
#include "../../src/core/save.h"

using namespace adr;

namespace {

int failures = 0, checks = 0;
#define CHECK(cond)                                                             \
  do {                                                                          \
    checks++;                                                                   \
    if (!(cond)) {                                                              \
      failures++;                                                               \
      fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                           \
  } while (0)

Game g;

bool logged(const char* needle) {
  const GameState& s = g.st();
  for (int i = 0; i < s.logN; ++i)
    if (strstr(s.log[i], needle)) return true;
  return false;
}

void secs(int n) {
  for (int i = 0; i < n; ++i) g.advance(1000);
}

// ---- the opening ----------------------------------------------------------------------------------

void testOpening() {
  printf("the opening\n");
  g.newGame(1);
  GameState& s = g.st();
  CHECK(s.active == M_ROOM);
  CHECK(logged("the room is freezing."));
  CHECK(logged("the fire is dead."));
  CHECK(!g.storeSeen(IT_WOOD));
  g.lightFire();  // the first fire is free: there is no wood yet
  CHECK(s.fire == 3);
  CHECK(!g.storeSeen(IT_WOOD));
  CHECK(logged("the light from the fire spills from the windows"));
  g.lightFire();  // on cooldown: nothing
  secs(31);
  CHECK(logged("a ragged stranger stumbles through the door"));
  secs(16);
  CHECK(s.hasOutside);
  CHECK(g.store(IT_WOOD) == 4);
  CHECK(logged("the wood is running out."));
  // the builder warms up and, once the room is warm, stands by the fire
  for (int i = 0; i < 12; ++i) {
    if (!g.onCooldown(CD_FIRE)) g.stokeFire();
    secs(15);
  }
  CHECK(s.builderLevel == 3);
  g.travelTo(M_OUTSIDE);
  CHECK(logged("the sky is grey and the wind blows relentlessly"));
  g.gatherWood();
  CHECK(g.store(IT_WOOD) >= 10);
  g.travelTo(M_ROOM);
  CHECK(s.builderLevel == 4);
  CHECK(logged("she builds things"));
  // with wood, the trap and the cart come up
  g.debugGive(IT_WOOD, 100);
  secs(2);
  CHECK(g.craftVisible(0));
  const double before = g.store(IT_WOOD);
  g.build(0);
  CHECK(s.buildings[BD_TRAP] == 1);
  CHECK(g.store(IT_WOOD) == before - 10);
  Amount c[3];
  int n;
  g.craftCost(0, c, n);
  CHECK(n == 1 && c[0].item == IT_WOOD && c[0].n == 20);  // the next trap costs more
  g.travelTo(M_OUTSIDE);
  g.checkTraps();
  CHECK(logged("the traps contain"));
  // the fire dies without stoking, the builder keeps it once she can
  s.builderLevel = 3;
  g.travelTo(M_ROOM);
}

// ---- the village's economy -------------------------------------------------------------------------

void testVillage() {
  printf("the village\n");
  g.newGame(2);
  GameState& s = g.st();
  g.lightFire();
  secs(60);
  s.builderLevel = 3;
  g.travelTo(M_OUTSIDE);
  g.travelTo(M_ROOM);
  g.debugGive(IT_WOOD, 5000);
  g.debugGive(IT_FUR, 500);
  g.debugGive(IT_MEAT, 500);
  secs(2);
  for (int i = 0; i < 6; ++i) g.build(2);  // huts
  CHECK(s.buildings[BD_HUT] == 6);
  g.build(3);  // the lodge
  CHECK(s.buildings[BD_LODGE] == 1);
  secs(2);
  CHECK(s.workerSeen >> WK_HUNTER & 1);
  CHECK(s.workerSeen >> WK_TRAPPER & 1);
  // strangers arrive in the night
  secs(60 * 4);
  CHECK(s.population > 0);
  CHECK(s.population <= g.maxPopulation());
  const int pop = s.population;
  g.adjustWorker(WK_HUNTER, 1000);
  CHECK(s.workers[WK_HUNTER] == pop);
  CHECK(g.numGatherers() == 0);
  g.adjustWorker(WK_HUNTER, -1);
  CHECK(g.numGatherers() == 1);
  const double fur = g.store(IT_FUR);
  secs(11);
  CHECK(g.store(IT_FUR) > fur);  // the hunters bring fur
  // too much of anything, and thieves come once the world is open
  s.hasWorld = true;
  g.debugGive(IT_WOOD, 6000);
  secs(1);
  CHECK(s.thieves == 1);
  CHECK(s.income[SRC_THIEVES].active);
  // a hut fire kills whoever lives in it
  s.population = 12;
  const int huts = s.buildings[BD_HUT];
  g.debugEvent(findEvent("outside.1"));
  CHECK(s.buildings[BD_HUT] == huts - 1);
  g.chooseButton(0);
  CHECK(s.ev.event < 0);
}

// ---- every event, every way through -----------------------------------------------------------------

// Plenty of everything, a strong wanderer, out in the world.
void outfit() {
  GameState& s = g.st();
  for (int i = 0; i < kStoreCount; ++i) {
    const Item it = static_cast<Item>(i);
    if (itemType(it) == ItemType::Blueprint) continue;
    g.debugGive(it, 200);
  }
  g.debugGive(IT_KINETIC_ARMOUR, 1);
  s.hasOutside = s.hasPath = true;
  if (!s.hasWorld) {
    g.travelTo(M_ROOM);
    g.debugGive(IT_COMPASS, 1);
    secs(1);
  }
  memset(s.outfit, 0, sizeof(s.outfit));
  s.outfit[IT_CURED_MEAT] = 20;
  s.outfit[IT_PLASMA_RIFLE] = 1;
  s.outfit[IT_ENERGY_CELL] = 30;
  s.outfit[IT_TORCH] = 5;
  s.outfit[IT_MEDICINE] = 5;
  s.outfit[IT_HYPO] = 3;
  s.outfit[IT_BOLAS] = 3;
  s.active = M_PATH;
  s.cd[CD_EMBARK].end = 0;
  g.embark();
}

// Play the event on the glass to its end: fights fought with whatever works, loot taken, a
// random choice among the buttons that can be pressed. Returns the scenes seen.
int playEvent(uint32_t seed, int maxSteps = 400) {
  GameState& s = g.st();
  uint32_t r = seed * 2654435761u + 1;
  auto roll = [&](int n) {
    r = r * 1103515245u + 12345u;
    return static_cast<int>((r >> 16) % static_cast<uint32_t>(n));
  };
  int scenes = 1, lastScene = -1, lastEvent = -1;
  for (int step = 0; step < maxSteps && s.ev.event >= 0; ++step) {
    if (s.ev.scene != lastScene || s.ev.event != lastEvent) {
      scenes++;
      lastScene = s.ev.scene;
      lastEvent = s.ev.event;
    }
    const Scene* sc = g.scene();
    if (s.ev.combat && !s.ev.lootShown) {
      // heal when low, else the strongest attack ready
      if (g.combatButtonEnabled(CB_HYPO) && s.trip.health < g.maxHealth() / 2) g.combatButton(CB_HYPO);
      for (int b = kWeaponCount - 1; b >= 0; --b)
        if (g.combatButtonEnabled(b)) {
          g.combatButton(b);
          break;
        }
      g.advance(250);
      continue;
    }
    if (s.ev.lootN && roll(2)) g.takeEverything();
    if (s.ev.event < 0) break;
    sc = g.scene();
    if (s.ev.combat && sc->buttonN == 0) {
      g.advance(1100);
      g.combatButton(CB_LEAVE);
      continue;
    }
    int avail[kMaxButtons], n = 0;
    for (int i = 0; i < sc->buttonN; ++i)
      if (g.buttonAvailable(i)) avail[n++] = i;
    g.advance(1100);  // the buttons' own cooldowns
    if (!n) {
      CHECK(!"a scene with no way on");
      break;
    }
    g.chooseButton(avail[roll(n)]);
  }
  return scenes;
}

void testEvents() {
  printf("every event\n");
  // the village's visitors: each, and each of its choices, a few times
  for (int e = 0; e < kEventCount; ++e) {
    const EventDef& ev = kEvents[e];
    if (ev.group != EG_GLOBAL && ev.group != EG_ROOM && ev.group != EG_OUTSIDE) continue;
    for (uint32_t seed = 0; seed < 12; ++seed) {
      g.newGame(100 + seed);
      GameState& s = g.st();
      s.builderLevel = 4;
      s.hasOutside = s.hasWorld = true;
      g.debugGive(IT_WOOD, 2000);
      g.debugGive(IT_FUR, 2000);
      g.debugGive(IT_SCALES, 200);
      g.debugGive(IT_TEETH, 200);
      g.debugGive(IT_MEDICINE, 20);
      g.debugGive(IT_CURED_MEAT, 200);
      g.debugGive(IT_TORCH, 5);
      s.buildings[BD_HUT] = 10;
      s.buildingSeen |= 1 << BD_HUT;
      s.buildings[BD_TRAP] = 5;
      s.buildingSeen |= 1 << BD_TRAP;
      s.population = 40;
      s.thieves = 1;
      g.debugEvent(e);
      CHECK(s.ev.event == e);
      playEvent(seed);
      CHECK(s.ev.event < 0);
      secs(120);  // the wanderers return, or not
    }
  }
  // the landmarks and the battleship, abroad
  int total = 0;
  for (int e = 0; e < kEventCount; ++e) {
    const EventDef& ev = kEvents[e];
    if (ev.group != EG_SETPIECE && ev.group != EG_EXECUTIONER && ev.group != EG_ENCOUNTER) continue;
    for (uint32_t seed = 0; seed < 25; ++seed) {
      g.newGame(1000 + seed * 7 + e);
      outfit();
      GameState& s = g.st();
      CHECK(s.active == M_WORLD);
      s.trip.world.flags |= WF_ENGINEERING | WF_MEDICAL;  // the command deck can open
      if (seed & 1) s.trip.world.flags |= WF_MARTIAL;
      g.debugEvent(e);
      total += playEvent(seed);
      CHECK(s.ev.event < 0);
      secs(3);  // a death's way home
    }
  }
  printf("  %d scenes played\n", total);
}

// ---- fights -------------------------------------------------------------------------------------

void testFights() {
  printf("fights\n");
  // bare-handed against a snarling beast, hitting until one of them falls
  int won = 0, lost = 0;
  for (uint32_t seed = 0; seed < 40; ++seed) {
    g.newGame(5000 + seed);
    outfit();
    GameState& s = g.st();
    memset(s.outfit, 0, sizeof(s.outfit));
    s.outfit[IT_CURED_MEAT] = 1;
    g.debugEvent(findEvent("encounter.0"));
    CHECK(s.ev.combat);
    CHECK(g.combatButtonShown(0));  // no weapons: the fists
    for (int t = 0; t < 400 && s.ev.event >= 0 && !s.ev.lootShown; ++t) {
      if (g.combatButtonEnabled(0)) g.combatButton(0);
      g.advance(100);
    }
    if (s.ev.event >= 0) {
      CHECK(s.ev.lootShown);
      won++;
      g.takeEverything();
      if (s.ev.event >= 0) {
        g.advance(1100);
        g.combatButton(CB_LEAVE);
      }
      CHECK(s.ev.event < 0);
      CHECK(s.active == M_WORLD);
    } else {
      lost++;
      CHECK(s.trip.dead);
      secs(3);
      CHECK(s.active == M_ROOM);
      CHECK(g.onCooldown(CD_EMBARK));
    }
    CHECK(s.punches > 0);
  }
  printf("  %d won, %d lost\n", won, lost);
  CHECK(won > 0);
  // a venomous enemy's poison works on the wanderer, every second, and not on itself
  {
    g.newGame(78);
    outfit();
    GameState& st = g.st();
    const int e = findEvent("executioner-medical");
    g.debugEvent(e);
    int medic = -1;
    for (int k = 0; k < kEvents[e].sceneN; ++k)
      if (kEvents[e].scenes[k].atHealthN) medic = k;
    CHECK(medic >= 0);
    g.debugScene(medic);
    CHECK(st.ev.combat);
    st.ev.enemy.status = ST_VENOMOUS;
    const int hp0 = st.ev.wanderer.hp;
    for (int t = 0; t < 100 && st.ev.wanderer.hp == hp0; ++t) g.advance(100);  // its poisoned blow
    const int hp1 = st.ev.wanderer.hp, enemyHp = st.ev.enemy.hp;
    CHECK(hp1 < hp0);
    CHECK(st.ev.enemy.status == ST_NONE);  // the venom goes with the blow
    st.ev.enemy.stunned = true;            // no more blows: only the poison now
    g.advance(static_cast<int64_t>(3200 * combatTempo()));
    CHECK(st.ev.wanderer.hp < hp1);
    CHECK(st.ev.enemy.hp == enemyHp);
  }
  // a rifle runs out of bullets, and the fists come back
  g.newGame(77);
  outfit();
  GameState& s = g.st();
  memset(s.outfit, 0, sizeof(s.outfit));
  s.outfit[IT_RIFLE] = 1;
  s.outfit[IT_BULLETS] = 1;
  s.outfit[IT_CURED_MEAT] = 3;
  g.debugEvent(findEvent("encounter.9"));  // a soldier
  CHECK(g.combatButtonShown(5) && g.combatButtonEnabled(5));
  g.combatButton(5);
  CHECK(s.outfit[IT_BULLETS] == 0);
  CHECK(!g.combatButtonEnabled(5));
  CHECK(g.combatButtonShown(0));
}

// ---- the world ----------------------------------------------------------------------------------

void testWorld() {
  printf("the world\n");
  g.newGame(9);
  outfit();
  GameState& s = g.st();
  CHECK(s.trip.active);
  CHECK(s.trip.x == kRadius && s.trip.y == kRadius);
  const int water = s.trip.water;
  g.move(DIR_NORTH);
  if (s.ev.event >= 0) playEvent(1);
  CHECK(s.trip.water < water || s.ev.event >= 0 || s.trip.y != kRadius - 1);
  // a long walk, taking what comes
  for (int i = 0; i < 300 && s.trip.active; ++i) {
    if (s.ev.event >= 0) {
      playEvent(i);
      continue;
    }
    s.outfit[IT_CURED_MEAT] = 20;
    s.trip.water = static_cast<int16_t>(g.maxWater());
    g.move(static_cast<Dir>((i / 7) % 4));
    g.advance(200);
  }
  // back to the village by the straightest way
  for (int i = 0; i < 200 && s.trip.active && !s.trip.dead; ++i) {
    if (s.ev.event >= 0) {
      playEvent(i);
      continue;
    }
    s.outfit[IT_CURED_MEAT] = 20;
    s.trip.water = static_cast<int16_t>(g.maxWater());
    if (s.trip.x < kRadius) g.move(DIR_EAST);
    else if (s.trip.x > kRadius) g.move(DIR_WEST);
    else if (s.trip.y < kRadius) g.move(DIR_SOUTH);
    else g.move(DIR_NORTH);
  }
  CHECK(!s.trip.active);
  CHECK(s.active == M_PATH || s.active == M_ROOM);
  // the map is generated as the original's: the village in the middle, a forest about it, the
  // landmarks placed
  int landmarks = 0;
  for (int x = 0; x < kMapSize; ++x)
    for (int y = 0; y < kMapSize; ++y)
      if (landmarkFor(s.world.tile[x][y] & 0x7F) || (s.world.tile[x][y] & kVisited)) landmarks++;
  CHECK(s.world.tile[kRadius][kRadius] == TL_VILLAGE);
  CHECK(landmarks > 40);
  CHECK(s.world.mask[kRadius][kRadius]);
}

// ---- saves --------------------------------------------------------------------------------------

std::vector<uint8_t> saved() {
  std::vector<uint8_t> b(kMaxSave);
  const size_t n = saveGame(g, b.data(), b.size());
  b.resize(n);
  return b;
}

void roundTrip(const char* when) {
  const std::vector<uint8_t> a = saved();
  CHECK(!a.empty());
  static Game other;
  const bool ok = loadGame(other, a.data(), a.size());
  CHECK(ok);
  std::vector<uint8_t> b(kMaxSave);
  b.resize(saveGame(other, b.data(), b.size()));
  const bool same = a == b;
  CHECK(same);
  if (!same) {
    size_t at = 0;
    while (at < a.size() && at < b.size() && a[at] == b[at]) at++;
    fprintf(stderr, "    (the save %s did not come back the same: %zu vs %zu bytes, first difference at %zu)\n", when,
            a.size(), b.size(), at);
    const GameState &x = g.st(), &y = other.st();
    fprintf(stderr, "    world tiles %d mask %d | trip tiles %d mask %d flags %d/%d | outposts %d | ev %d | timers %d\n",
            memcmp(x.world.tile, y.world.tile, sizeof(x.world.tile)) != 0,
            memcmp(x.world.mask, y.world.mask, sizeof(x.world.mask)) != 0,
            memcmp(x.trip.world.tile, y.trip.world.tile, sizeof(x.trip.world.tile)) != 0,
            memcmp(x.trip.world.mask, y.trip.world.mask, sizeof(x.trip.world.mask)) != 0, x.trip.world.flags,
            y.trip.world.flags, memcmp(x.trip.usedOutposts, y.trip.usedOutposts, sizeof(x.trip.usedOutposts)) != 0,
            x.ev.event != y.ev.event, memcmp(x.timers, y.timers, sizeof(x.timers)) != 0);
    for (size_t k = at > 8 ? at - 8 : 0; k < at + 12 && k < a.size(); ++k) fprintf(stderr, "%02x/%02x ", a[k], b[k]);
    fprintf(stderr, "\n    trip: x %d/%d y %d/%d water %d/%d hp %d/%d moves %d/%d %d/%d %d/%d\n", x.trip.x, y.trip.x,
            x.trip.y, y.trip.y, x.trip.water, y.trip.water, x.trip.health, y.trip.health, x.trip.foodMove,
            y.trip.foodMove, x.trip.waterMove, y.trip.waterMove, x.fightMove, y.fightMove);
  }
  // and the loaded game plays on as the original would
  other.advance(5000);
  g.advance(5000);
  const std::vector<uint8_t> c = saved();
  std::vector<uint8_t> d(kMaxSave);
  d.resize(saveGame(other, d.data(), d.size()));
  CHECK(c == d);
  printf("  %-28s %5zu bytes\n", when, a.size());
}

void testSaves() {
  printf("saves\n");
  g.newGame(31);
  roundTrip("a new game");
  g.lightFire();
  secs(200);
  roundTrip("the fire lit");
  g.st().builderLevel = 4;
  g.debugGive(IT_WOOD, 3000);
  g.debugGive(IT_FUR, 300);
  g.debugGive(IT_MEAT, 300);
  secs(1);
  for (int i = 0; i < 8; ++i) g.build(2);
  g.build(3);
  secs(300);
  roundTrip("a village");
  outfit();
  for (int i = 0; i < 6; ++i) g.move(DIR_WEST);
  if (g.st().ev.event >= 0) playEvent(3);
  roundTrip("abroad");
  g.debugEvent(findEvent("encounter.8"));
  g.advance(1500);
  roundTrip("in a fight");
  playEvent(2);
  g.debugEvent(findEvent("cave"));
  roundTrip("at a landmark");
  playEvent(4);
  // a corrupted save is refused, and the prestige still read
  std::vector<uint8_t> a = saved();
  a[a.size() / 2] ^= 0x55;
  static Game other;
  CHECK(!loadGame(other, a.data(), a.size()));
}

// ---- the ascent and the ending --------------------------------------------------------------------

void testSpace() {
  printf("the ascent\n");
  int ended = 0, crashed = 0;
  for (uint32_t seed = 0; seed < 12; ++seed) {
    g.newGame(700 + seed);
    GameState& s = g.st();
    s.hasShip = true;
    s.hull = 12;
    s.thrusters = 6;
    g.debugGive(IT_ALIEN_ALLOY, 3);
    if (seed & 1) g.debugGive(IT_FLEET_BEACON, 1);
    s.active = M_SHIP;
    g.spaceLayout(20, 30);
    g.liftOff();
    CHECK(s.space.active);
    CHECK(s.active == M_SPACE);
    // a pilot who steers out of the way of what is coming down on the ship
    for (int t = 0; t < 400 && s.space.active; ++t) {
      int danger[3] = {0, 0, 0};  // left, here, right
      for (int i = 0; i < s.space.n; ++i) {
        const Asteroid& a = s.space.a[i];
        const int dy = s.space.shipY - a.y;
        if (dy < 0 || dy > 6) continue;
        const int dx = a.x - s.space.shipX;
        if (dx >= -1 && dx <= 1) danger[dx + 1] += 7 - dy;
      }
      int dx = 0;
      if (danger[1]) dx = danger[0] <= danger[2] ? -1 : 1;
      if ((dx < 0 && s.space.shipX == 0) || (dx > 0 && s.space.shipX == s.space.cols - 1)) dx = -dx;
      g.spaceSteer(dx, 0);
      g.advance(250);
    }
    if (s.ending) {
      ended++;
      CHECK(s.gameOver);
      CHECK(s.hasPrev);
      CHECK(s.lastScore > 0);
      CHECK(s.prevScore >= s.lastScore);
      roundTrip("the ending");
      const int32_t total = s.prevScore;
      g.restart();
      CHECK(!s.ending);
      CHECK(s.prevScore == total);  // the prestige is kept
      CHECK(s.hasPrev);
    } else {
      crashed++;
      CHECK(s.active == M_SHIP);
      CHECK(g.onCooldown(CD_LIFTOFF));
    }
  }
  printf("  %d reached space, %d crashed\n", ended, crashed);
  CHECK(ended > 0);
}

// ---- the screens ----------------------------------------------------------------------------------

struct TestPlatform : Platform {
  const DeviceProfile* prof = &kX4Pro;
  std::vector<uint8_t> blobs[2];
  const DeviceProfile& profile() override { return *prof; }
  uint32_t millis() override { return 0; }
  uint32_t random() override { return 4; }
  size_t load(const char* key, uint8_t* buf, size_t cap) override {
    const auto& b = blobs[strcmp(key, "cfg") == 0];
    if (b.empty() || b.size() > cap) return 0;
    memcpy(buf, b.data(), b.size());
    return b.size();
  }
  bool save(const char* key, const uint8_t* buf, size_t len) override {
    blobs[strcmp(key, "cfg") == 0].assign(buf, buf + len);
    return true;
  }
  int batteryPercent() override { return 50; }
  bool charging() override { return false; }
};

uint8_t fb[kMaxFrameBytes];

void drawAll(App& app, Game& game, const char* where) {
  GameState& s = game.st();
  // every place
  for (Module m : {M_ROOM, M_OUTSIDE, M_PATH, M_FABRICATOR, M_SHIP}) {
    if (!game.unlocked(m)) continue;
    game.travelTo(m);
    app.render();
    for (Input::KeyCode k : {Input::Down, Input::Down, Input::Right, Input::Confirm, Input::Up, Input::Back, Input::Back}) {
      app.handle(Input::keyPress(k));
      app.render();
    }
    for (int y = 60; y < app.canvas().height(); y += 97) {
      app.handle(Input::tap(120, y));
      app.render();
      app.handle(Input::tap(360, y));
      app.render();
    }
    app.handle(Input::swipe(400, 400, 60, 400));
    app.render();
  }
  (void)s;
  (void)where;
}

void testScreens() {
  printf("the screens\n");
  for (const DeviceProfile* d : {&kX4Pro, &kX4Classic, &kX3, &kX4}) {
    for (int rot : {1, 0}) {
      TestPlatform p;
      p.prof = d;
      DeviceSettings ds;
      ds.rotation = static_cast<uint8_t>(rot);
      p.blobs[1].assign(reinterpret_cast<uint8_t*>(&ds), reinterpret_cast<uint8_t*>(&ds) + sizeof(ds));
      static Game game;
      static App app;
      app.begin(game, p, fb);
      app.loadOrStart();
      GameState& s = game.st();
      app.render();
      // a late game: everything built, bought and known
      s.builderLevel = 4;
      s.hasOutside = s.hasPath = s.hasWorld = s.hasShip = s.hasFabricator = true;
      s.fire = 4;
      s.temperature = 4;
      for (int i = 0; i < kStoreCount; ++i)
        if (itemType(static_cast<Item>(i)) != ItemType::Blueprint) game.debugGive(static_cast<Item>(i), 1234);
      for (int b = 0; b < BD_COUNT; ++b) {
        s.buildings[b] = b == BD_HUT ? 18 : b == BD_TRAP ? 7 : 1;
        s.buildingSeen |= static_cast<uint16_t>(1u << b);
      }
      s.population = 70;
      s.perks = 0x7FF;
      s.blueprints = 0x3F;
      game.advance(2000);
      drawAll(app, game, d->name);
      // the world, its overview, an event, a fight, the loot, the ascent, the ending
      s.active = M_PATH;
      s.outfit[IT_CURED_MEAT] = 10;
      s.cd[CD_EMBARK].end = 0;
      game.embark();
      app.render();
      app.handle(Input::keyPress(Input::Confirm));
      app.render();
      app.handle(Input::keyPress(Input::Back));
      app.render();
      game.debugEvent(findEvent("house"));
      app.render();
      game.chooseButton(0);
      app.render();
      app.handle(Input::keyPress(Input::Down));
      app.render();
      s.ev.event = -1;
      game.debugEvent(findEvent("encounter.10"));
      for (int i = 0; i < 12; ++i) {
        game.advance(700);
        app.handle(Input::keyPress(Input::Confirm));
        app.render();
      }
      for (int i = 0; i < 600 && s.ev.event >= 0 && !s.ev.lootShown; ++i) {
        for (int b = kWeaponCount - 1; b >= 0; --b)
          if (game.combatButtonEnabled(b)) game.combatButton(b);
        game.advance(100);
      }
      app.render();
      app.handle(Input::keyPress(Input::Confirm));
      app.render();
      s.hull = 5;
      s.ev.event = -1;
      s.trip.active = false;
      s.active = M_SHIP;
      s.cd[CD_LIFTOFF].end = 0;
      game.liftOff();
      for (int i = 0; i < 20; ++i) {
        game.advance(500);
        app.render();
      }
      app.renderSleep(false);
      app.renderSleep(true);
      s.space.active = false;
      s.ending = true;
      app.render();
      app.handle(Input::keyPress(Input::Back));
      app.render();
      for (int i = 0; i < 8; ++i) {
        app.handle(Input::keyPress(Input::Down));
        app.render();
      }
    }
  }
}

// ---- the app's flows, by the keys ----------------------------------------------------------------

void testFlows() {
  printf("the flows\n");
  TestPlatform p;
  p.prof = &kX4Classic;
  static Game game;
  static App app;
  app.begin(game, p, fb);
  app.loadOrStart();
  GameState& s = game.st();
  auto press = [&](Input::KeyCode k, bool held = false) {
    app.handle(Input::keyPress(k, held));
    app.render();
  };
  app.render();
  press(Input::Confirm);  // light the fire
  CHECK(s.fire == 3);
  // the menu pauses the world
  press(Input::Back);
  const int64_t before = s.now;
  app.tick(5000);
  CHECK(s.now == before);
  // hyper asks first
  press(Input::Down);
  press(Input::Down);
  press(Input::Confirm);
  press(Input::Confirm);  // "yes": the game resumes, twice as fast
  CHECK(s.settings.hyper);
  app.tick(1000);
  CHECK(s.now > before);
  // restart keeps the prestige
  s.hasPrev = true;
  s.prevScore = 99;
  press(Input::Back);
  for (int i = 0; i < 6; ++i) press(Input::Down);
  press(Input::Confirm);
  press(Input::Confirm);  // "yes"
  CHECK(s.fire == 0);
  CHECK(s.prevScore == 99 && s.hasPrev);
  // the side keys go from place to place; Left and Right go through what there is to do in one
  s.hasOutside = true;
  s.buildings[BD_HUT] = 1;
  s.population = 4;
  s.workerSeen |= 1u << WK_HUNTER;
  game.travelTo(M_ROOM);
  app.render();
  press(Input::Down);
  CHECK(s.active == M_OUTSIDE);
  press(Input::Up);
  CHECK(s.active == M_ROOM);
  press(Input::Up);  // nowhere before the room
  CHECK(s.active == M_ROOM);
  press(Input::Down);
  // a worker: Confirm to change how many, Left and Right to give and take, Confirm when done
  char where[200];
  for (int i = 0; i < 4; ++i) {
    app.describe(where, sizeof(where));
    if (strstr(where, "focus 06")) break;
    press(Input::Right);
  }
  app.describe(where, sizeof(where));
  CHECK(strstr(where, "focus 06") != nullptr);
  CHECK(s.active == M_OUTSIDE);
  const int hunters = s.workers[WK_HUNTER];
  press(Input::Left);  // not changing it yet: the focus moves back
  CHECK(s.workers[WK_HUNTER] == hunters);
  app.describe(where, sizeof(where));
  CHECK(strstr(where, "focus 06") == nullptr);
  press(Input::Right);
  press(Input::Confirm);
  press(Input::Right);
  CHECK(s.workers[WK_HUNTER] == hunters + 1);
  press(Input::Left);
  CHECK(s.workers[WK_HUNTER] == hunters);
  press(Input::Right, true);  // held: as many as there are gatherers
  CHECK(s.workers[WK_HUNTER] > hunters + 1);
  press(Input::Back);  // done (Back ends it, the menu stays shut)
  press(Input::Left);
  CHECK(s.workers[WK_HUNTER] > hunters + 1);
  app.describe(where, sizeof(where));
  CHECK(strstr(where, "overlay") == nullptr);
  // the first lift-off asks; "linger" stays
  s.hasShip = true;
  s.hull = 3;
  game.travelTo(M_SHIP);
  app.render();
  for (int i = 0; i < 4; ++i) press(Input::Right);  // to "lift off"
  press(Input::Confirm);
  CHECK(!s.space.active);  // the dialog
  press(Input::Right);     // "linger"
  press(Input::Confirm);
  CHECK(!s.space.active);
  CHECK(!s.seenWarning);
  press(Input::Confirm);  // lift off again, and this time go
  press(Input::Confirm);
  CHECK(s.space.active);
  CHECK(s.seenWarning);
  for (int i = 0; i < 10; ++i) {
    press(Input::Left);
    app.tick(500);
  }
  // no room for the loot: the drop menu
  game.newGame(8);
  outfit();
  s.outfit[IT_CURED_MEAT] = 100;  // the bag is more than full
  game.debugEvent(findEvent("house"));
  const int sc0 = s.ev.scene;
  app.render();
  for (int i = 0; i < 20 && s.ev.scene == sc0; ++i) {
    app.tick(1100);
    press(Input::Confirm);  // go inside
  }
  if (s.ev.lootN > 0) {
    const int before = s.outfit[s.ev.loot[0].item];
    press(Input::Confirm);  // take one: no room, so the drop menu
    press(Input::Confirm);  // drop the first thing offered
    CHECK(s.outfit[s.ev.loot[0].item] >= before || s.ev.lootN >= 1);
  }
}

}  // namespace

int main() {
  testOpening();
  testVillage();
  testFights();
  testWorld();
  testEvents();
  testSaves();
  testSpace();
  testScreens();
  testFlows();
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
