#include "save.h"

#include <cstring>

#include "game.h"

namespace adr {

namespace {

uint32_t crc32(const uint8_t* p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

class Writer {
 public:
  Writer(uint8_t* out, size_t cap) : _p(out), _cap(cap) {}
  void byte(uint8_t b) {
    if (_n < _cap) _p[_n] = b;
    _n++;
  }
  void u(uint64_t v) {
    while (v >= 0x80) {
      byte(static_cast<uint8_t>(v | 0x80));
      v >>= 7;
    }
    byte(static_cast<uint8_t>(v));
  }
  void i(int64_t v) { u((static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63)); }
  void b(bool v) { byte(v ? 1 : 0); }
  void half(double v) { i(static_cast<int64_t>(v * 2 + (v >= 0 ? 0.25 : -0.25))); }  // stores: halves
  void str(const char* s) {
    const size_t n = strlen(s);
    u(n);
    for (size_t k = 0; k < n; ++k) byte(static_cast<uint8_t>(s[k]));
  }
  size_t size() const { return _n; }
  bool ok() const { return _n <= _cap; }

 private:
  uint8_t* _p;
  size_t _cap, _n = 0;
};

class Reader {
 public:
  Reader(const uint8_t* in, size_t n) : _p(in), _n(n) {}
  uint8_t byte() {
    if (_at >= _n) {
      _bad = true;
      return 0;
    }
    return _p[_at++];
  }
  uint64_t u() {
    uint64_t v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
      const uint8_t c = byte();
      v |= static_cast<uint64_t>(c & 0x7F) << shift;
      if (!(c & 0x80)) return v;
    }
    _bad = true;
    return 0;
  }
  int64_t i() {
    const uint64_t z = u();
    return static_cast<int64_t>(z >> 1) ^ -static_cast<int64_t>(z & 1);
  }
  bool b() { return byte() != 0; }
  double half() { return i() / 2.0; }
  void str(char* out, size_t cap) {
    const size_t n = u();
    for (size_t k = 0; k < n; ++k) {
      const char c = static_cast<char>(byte());
      if (k + 1 < cap) out[k] = c;
    }
    out[n < cap ? n : cap - 1] = 0;
  }
  // a bounded index
  int idx(int limit) {
    const uint64_t v = u();
    if (v >= static_cast<uint64_t>(limit)) {
      _bad = true;
      return 0;
    }
    return static_cast<int>(v);
  }
  bool bad() const { return _bad; }

 private:
  const uint8_t* _p;
  size_t _n, _at = 0;
  bool _bad = false;
};

constexpr uint8_t kMagic[4] = {'A', 'D', 'R', 'X'};
constexpr int kCells = kMapSize * kMapSize;
WorldMap gBase;  // the map the seed grows, for either way

// the tiles and the mask of `m` where they differ from `base`
void writeMapDiff(Writer& w, const WorldMap& m, const WorldMap& base) {
  int n = 0;
  for (int k = 0; k < kCells; ++k)
    if ((&m.tile[0][0])[k] != (&base.tile[0][0])[k]) n++;
  w.u(n);
  int last = 0;
  for (int k = 0; k < kCells; ++k)
    if ((&m.tile[0][0])[k] != (&base.tile[0][0])[k]) {
      w.u(k - last);
      w.byte((&m.tile[0][0])[k]);
      last = k;
    }
  // the mask as runs, alternating from unseen
  int run = 0;
  uint8_t cur = 0;
  int runs = 0;
  for (int k = 0; k < kCells; ++k) {
    const uint8_t v = (&m.mask[0][0])[k] ? 1 : 0;
    if (v != cur) {
      runs++;
      cur = v;
    }
  }
  w.u(runs);
  cur = 0;
  for (int k = 0; k < kCells; ++k) {
    const uint8_t v = (&m.mask[0][0])[k] ? 1 : 0;
    if (v != cur) {
      w.u(run);
      run = 0;
      cur = v;
    }
    run++;
  }
  w.byte(m.flags);
}

void readMapDiff(Reader& r, WorldMap& m, const WorldMap& base) {
  memcpy(m.tile, base.tile, sizeof(m.tile));
  const int n = r.idx(kCells + 1);
  int at = 0;
  for (int k = 0; k < n && !r.bad(); ++k) {
    at += r.idx(kCells);
    const uint8_t t = r.byte();
    if (at < kCells) (&m.tile[0][0])[at] = t;
  }
  const int runs = r.idx(kCells + 1);
  memset(m.mask, 0, sizeof(m.mask));
  int pos = 0;
  for (int k = 0; k < runs && !r.bad(); ++k) {
    pos += r.idx(kCells + 1);
    if (pos < kCells) (&m.mask[0][0])[pos] |= 2;  // a change point
  }
  // sweep: every change point flips seen/unseen
  uint8_t cur = 0;
  for (int k = 0; k < kCells; ++k) {
    uint8_t& v = (&m.mask[0][0])[k];
    if (v & 2) cur ^= 1;
    v = cur;
  }
  m.flags = r.byte();
}

void writeFighter(Writer& w, const Fighter& f) {
  w.i(f.hp);
  w.i(f.maxHp);
  w.byte(f.status);
  w.b(f.stunned);
}
void readFighter(Reader& r, Fighter& f) {
  f.hp = static_cast<int16_t>(r.i());
  f.maxHp = static_cast<int16_t>(r.i());
  f.status = r.byte();
  f.stunned = r.b();
  f.floatText[0] = 0;
}

void writePrestige(Writer& w, const GameState& s) {
  w.b(s.hasPrev);
  for (int32_t v : s.prevStores) w.i(v);
  w.i(s.prevScore);
}
void readPrestige(Reader& r, GameState& s) {
  s.hasPrev = r.b();
  for (int32_t& v : s.prevStores) v = static_cast<int32_t>(r.i());
  s.prevScore = static_cast<int32_t>(r.i());
}

}  // namespace

size_t saveGame(Game& g, uint8_t* out, size_t cap) {
  GameState& s = g.st();
  Writer w(out, cap);
  for (uint8_t c : kMagic) w.byte(c);
  w.byte(kSaveVersion);
  w.u(static_cast<uint64_t>(kEventCount));
  writePrestige(w, s);
  w.b(s.settings.lightsOff);
  w.b(s.settings.hyper);
  // stores
  w.u(s.storeSeen);
  for (int k = 0; k < kStoreCount; ++k)
    if (s.storeSeen >> k & 1) w.half(s.stores[k]);
  w.u(s.perks);
  w.u(s.blueprints);
  w.i(s.punches);
  w.i(s.starved);
  w.i(s.dehydrated);
  w.i(s.fire);
  w.i(s.temperature);
  w.i(s.builderLevel);
  w.u((s.hasOutside ? 1 : 0) | (s.hasPath ? 2 : 0) | (s.hasWorld ? 4 : 0) | (s.hasShip ? 8 : 0) |
      (s.hasFabricator ? 16 : 0) | (s.executionerPlaced ? 32 : 0) | (s.seenForest ? 64 : 0) | (s.cityCleared ? 128 : 0) |
      (s.seenAll ? 256 : 0) | (s.seenShip ? 512 : 0) | (s.seenWarning ? 1024 : 0) | (s.fabricatorSeen ? 2048 : 0) |
      (s.pathDiscovery ? 4096 : 0) | (s.gameOver ? 8192 : 0) | (s.ending ? 16384 : 0) | (s.roomChanged ? 32768 : 0));
  w.u(s.buildingSeen);
  for (int k = 0; k < BD_COUNT; ++k)
    if (s.buildingSeen >> k & 1) w.i(s.buildings[k]);
  w.u(s.workerSeen);
  for (int k = 0; k < WK_COUNT; ++k)
    if (s.workerSeen >> k & 1) w.i(s.workers[k]);
  w.i(s.population);
  w.i(s.thieves);
  for (int k = 0; k < kStoreCount; ++k) w.half(s.stolen[k]);
  // income, in its order
  w.u(s.incomeN);
  for (int k = 0; k < s.incomeN; ++k) {
    const uint8_t src = s.incomeOrder[k];
    const IncomeRec& inc = s.income[src];
    w.u(src);
    w.u(inc.delay);
    w.i(inc.timeLeft);
    w.u(inc.n);
    for (int j = 0; j < inc.n; ++j) {
      w.u(inc.item[j]);
      w.half(inc.amount[j]);
    }
  }
  // the outfit
  for (int k = 0; k < kStoreCount; ++k) w.i(s.outfit[k]);
  // the world
  if (s.hasWorld) {
    w.u(s.mapSeed);
    g.baseMap(gBase);
    writeMapDiff(w, s.world, gBase);
  }
  w.i(s.hull);
  w.i(s.thrusters);
  w.u(s.craftUnlocked);
  w.u(s.craftAtMax);
  w.u(s.tradeUnlocked);
  w.i(s.lastScore);
  w.u(s.fightMove);
  w.b(s.danger);
  w.u(s.workerOrderN);
  for (int k = 0; k < s.workerOrderN; ++k) w.u(s.workerOrder[k]);
  // the clock, its timers and the cooldowns
  w.i(s.now);
  int nt = 0;
  for (const Timer& t : s.timers)
    if (t.kind != T_NONE) nt++;
  w.u(nt);
  for (const Timer& t : s.timers) {
    if (t.kind == T_NONE) continue;
    w.u(t.kind);
    w.i(t.arg);
    w.i(t.due - s.now);
    w.i(t.period);
  }
  for (const Cooldown& c : s.cd) {
    w.i(c.end > s.now ? c.end - s.now : 0);
    w.i(c.total);
  }
  w.u(s.active);
  // the notices kept for a place not in view, and the log's last lines
  w.u(s.queuedN);
  for (int k = 0; k < s.queuedN; ++k) {
    w.u(s.queuedFor[k]);
    w.str(s.queued[k]);
  }
  const int keep = s.logN < 12 ? s.logN : 12;
  w.u(keep);
  for (int k = s.logN - keep; k < s.logN; ++k) w.str(s.log[k]);
  w.u(s.logSerial);
  // the expedition
  const Trip& t = s.trip;
  w.b(t.active);
  if (t.active) {
    w.u(static_cast<uint64_t>(t.x));
    w.u(static_cast<uint64_t>(t.y));
    w.i(t.water);
    w.i(t.health);
    w.u(t.foodMove);
    w.u(t.waterMove);
    w.u((t.starvation ? 1 : 0) | (t.thirst ? 2 : 0));
    int used = 0;
    for (int k = 0; k < kCells; ++k)
      if (t.outpostUsed(k / kMapSize, k % kMapSize)) used++;
    w.u(used);
    for (int k = 0; k < kCells; ++k)
      if (t.outpostUsed(k / kMapSize, k % kMapSize)) w.u(k);
    writeMapDiff(w, t.world, s.world);
  }
  // the event under way
  const ActiveEvent& ev = s.ev;
  w.b(ev.event >= 0);
  if (ev.event >= 0) {
    w.str(kEvents[ev.event].key);
    w.u(ev.scene);
    for (int k = 0; k < kMaxButtons; ++k) {
      w.i(ev.buttonCd[k] > s.now ? ev.buttonCd[k] - s.now : 0);
      w.i(ev.buttonCdTotal[k]);
    }
    w.u((ev.combat ? 1 : 0) | (ev.fought ? 2 : 0) | (ev.won ? 4 : 0) | (ev.lootShown ? 8 : 0) | (ev.exploding ? 16 : 0) |
        (ev.fistsAdded ? 32 : 0) | (ev.blink ? 64 : 0) | (ev.lootFits ? 128 : 0) | (ev.takeLeave ? 256 : 0));
    writeFighter(w, ev.wanderer);
    writeFighter(w, ev.enemy);
    w.i(ev.meditateDmg);
    w.u(ev.lastSpecial);
    w.i(ev.dotDmg);
    w.i(ev.enemyAttackMs);
    for (int k = 0; k < kWeaponCount + 6; ++k) {
      w.i(ev.combatCd[k] > s.now ? ev.combatCd[k] - s.now : 0);
      w.i(ev.combatCdTotal[k]);
    }
    w.u(ev.weaponDisabled);
    w.u(ev.lootN);
    for (int k = 0; k < ev.lootN; ++k) {
      w.u(ev.loot[k].item);
      w.i(ev.loot[k].numLeft);
    }
  }
  // the ascent
  const auto& sp = s.space;
  w.b(sp.active);
  if (sp.active) {
    w.i(sp.hull);
    w.i(s.now - sp.startedAt);
    w.i(sp.nextStep - s.now);
    w.i(sp.nextWave - s.now);
    w.i(sp.shipX);
    w.i(sp.shipY);
    w.i(sp.cols);
    w.i(sp.rows);
    w.u(sp.n);
    for (int k = 0; k < sp.n; ++k) {
      w.i(sp.a[k].x);
      w.i(sp.a[k].y);
      w.byte(sp.a[k].glyph);
      w.byte(sp.a[k].speed);
    }
  }
  for (uint32_t v : g.rng().s) w.u(v);
  if (!w.ok()) return 0;
  const size_t n = w.size();
  if (n + 4 > cap) return 0;
  const uint32_t crc = crc32(out, n);
  out[n] = static_cast<uint8_t>(crc);
  out[n + 1] = static_cast<uint8_t>(crc >> 8);
  out[n + 2] = static_cast<uint8_t>(crc >> 16);
  out[n + 3] = static_cast<uint8_t>(crc >> 24);
  return n + 4;
}

static bool header(const uint8_t* in, size_t n) {
  if (n < 9 || memcmp(in, kMagic, 4) != 0) return false;
  const uint32_t crc = in[n - 4] | in[n - 3] << 8 | in[n - 2] << 16 | static_cast<uint32_t>(in[n - 1]) << 24;
  return crc32(in, n - 4) == crc;
}

bool loadPrestige(Game& g, const uint8_t* in, size_t n) {
  if (!header(in, n)) return false;
  Reader r(in + 4, n - 8);
  r.byte();
  r.u();
  readPrestige(r, g.st());
  return !r.bad();
}

bool loadGame(Game& g, const uint8_t* in, size_t n) {
  if (!header(in, n)) return false;
  Reader r(in + 4, n - 8);
  if (r.byte() != kSaveVersion) return false;
  if (r.u() != static_cast<uint64_t>(kEventCount)) {
    // (an event table this firmware did not write: the save's indices may point elsewhere)
  }
  // (a save that fails to read leaves a state the caller replaces with a new game)
  GameState& s = g.st();
  memset(static_cast<void*>(&s), 0, sizeof(s));
  s.ev = ActiveEvent();
  readPrestige(r, s);
  s.settings.lightsOff = r.b();
  s.settings.hyper = r.b();
  s.storeSeen = r.u();
  for (int k = 0; k < kStoreCount; ++k)
    if (s.storeSeen >> k & 1) s.stores[k] = r.half();
  s.perks = static_cast<uint16_t>(r.u());
  s.blueprints = static_cast<uint8_t>(r.u());
  s.punches = static_cast<int32_t>(r.i());
  s.starved = static_cast<int32_t>(r.i());
  s.dehydrated = static_cast<int32_t>(r.i());
  s.fire = static_cast<int8_t>(r.i());
  s.temperature = static_cast<int8_t>(r.i());
  s.builderLevel = static_cast<int8_t>(r.i());
  const uint64_t f = r.u();
  s.hasOutside = f & 1;
  s.hasPath = f & 2;
  s.hasWorld = f & 4;
  s.hasShip = f & 8;
  s.hasFabricator = f & 16;
  s.executionerPlaced = f & 32;
  s.seenForest = f & 64;
  s.cityCleared = f & 128;
  s.seenAll = f & 256;
  s.seenShip = f & 512;
  s.seenWarning = f & 1024;
  s.fabricatorSeen = f & 2048;
  s.pathDiscovery = f & 4096;
  s.gameOver = f & 8192;
  s.ending = f & 16384;
  s.roomChanged = f & 32768;
  s.buildingSeen = static_cast<uint16_t>(r.u());
  for (int k = 0; k < BD_COUNT; ++k)
    if (s.buildingSeen >> k & 1) s.buildings[k] = static_cast<int16_t>(r.i());
  s.workerSeen = static_cast<uint16_t>(r.u());
  for (int k = 0; k < WK_COUNT; ++k)
    if (s.workerSeen >> k & 1) s.workers[k] = static_cast<int32_t>(r.i());
  s.population = static_cast<int32_t>(r.i());
  s.thieves = static_cast<int8_t>(r.i());
  for (int k = 0; k < kStoreCount; ++k) s.stolen[k] = r.half();
  s.incomeN = static_cast<uint8_t>(r.idx(SRC_COUNT + 1));
  for (int k = 0; k < s.incomeN && !r.bad(); ++k) {
    const int src = r.idx(SRC_COUNT);
    s.incomeOrder[k] = static_cast<uint8_t>(src);
    IncomeRec& inc = s.income[src];
    inc.active = true;
    inc.delay = static_cast<uint8_t>(r.u());
    inc.timeLeft = static_cast<int32_t>(r.i());
    inc.n = static_cast<uint8_t>(r.idx(4));
    for (int j = 0; j < inc.n; ++j) {
      inc.item[j] = static_cast<Item>(r.idx(kStoreCount));
      inc.amount[j] = r.half();
    }
  }
  for (int k = 0; k < kStoreCount; ++k) s.outfit[k] = static_cast<int32_t>(r.i());
  if (s.hasWorld) {
    s.mapSeed = static_cast<uint32_t>(r.u());
    g.baseMap(gBase);
    readMapDiff(r, s.world, gBase);
  }
  s.hull = static_cast<int16_t>(r.i());
  s.thrusters = static_cast<int16_t>(r.i());
  s.craftUnlocked = r.u();
  s.craftAtMax = r.u();
  s.tradeUnlocked = r.u();
  s.lastScore = static_cast<int32_t>(r.i());
  s.fightMove = static_cast<uint8_t>(r.u());
  s.danger = r.b();
  s.workerOrderN = static_cast<uint8_t>(r.idx(WK_COUNT + 1));
  for (int k = 0; k < s.workerOrderN; ++k) s.workerOrder[k] = static_cast<uint8_t>(r.idx(WK_COUNT));
  s.now = r.i();
  const int nt = r.idx(kMaxTimers + 1);
  for (int k = 0; k < nt && !r.bad(); ++k) {
    Timer& t = s.timers[k];
    t.kind = static_cast<uint8_t>(r.idx(T_COUNT));
    t.arg = static_cast<int16_t>(r.i());
    t.due = s.now + r.i();
    t.period = r.i();
  }
  for (Cooldown& c : s.cd) {
    const int64_t left = r.i();
    c.end = left > 0 ? s.now + left : 0;
    c.total = static_cast<int32_t>(r.i());
  }
  s.active = static_cast<uint8_t>(r.idx(M_COUNT));
  s.queuedN = static_cast<uint8_t>(r.idx(kQueued + 1));
  for (int k = 0; k < s.queuedN && !r.bad(); ++k) {
    s.queuedFor[k] = static_cast<uint8_t>(r.idx(M_COUNT));
    r.str(s.queued[k], kLogChars);
  }
  s.logN = static_cast<uint8_t>(r.idx(kLogLines + 1));
  for (int k = 0; k < s.logN && !r.bad(); ++k) r.str(s.log[k], kLogChars);
  s.logSerial = static_cast<uint32_t>(r.u());
  Trip& t = s.trip;
  t.active = r.b();
  if (t.active) {
    t.x = static_cast<int8_t>(r.idx(kMapSize));
    t.y = static_cast<int8_t>(r.idx(kMapSize));
    t.water = static_cast<int16_t>(r.i());
    t.health = static_cast<int16_t>(r.i());
    t.foodMove = static_cast<uint8_t>(r.u());
    t.waterMove = static_cast<uint8_t>(r.u());
    const uint64_t tf = r.u();
    t.starvation = tf & 1;
    t.thirst = tf & 2;
    const int used = r.idx(kCells + 1);
    for (int k = 0; k < used && !r.bad(); ++k) {
      const int c = r.idx(kCells);
      t.useOutpost(c / kMapSize, c % kMapSize);
    }
    readMapDiff(r, t.world, s.world);
  }
  ActiveEvent& ev = s.ev;
  if (r.b()) {
    char key[48];
    r.str(key, sizeof(key));
    ev.event = static_cast<int16_t>(findEvent(key));
    ev.scene = static_cast<uint8_t>(r.u());
    for (int k = 0; k < kMaxButtons; ++k) {
      const int64_t left = r.i();
      ev.buttonCd[k] = left > 0 ? s.now + left : 0;
      ev.buttonCdTotal[k] = static_cast<int32_t>(r.i());
    }
    const uint64_t ef = r.u();
    ev.combat = ef & 1;
    ev.fought = ef & 2;
    ev.won = ef & 4;
    ev.lootShown = ef & 8;
    ev.exploding = ef & 16;
    ev.fistsAdded = ef & 32;
    ev.blink = ef & 64;
    ev.lootFits = ef & 128;
    ev.takeLeave = ef & 256;
    readFighter(r, ev.wanderer);
    readFighter(r, ev.enemy);
    ev.meditateDmg = static_cast<int32_t>(r.i());
    ev.lastSpecial = static_cast<uint8_t>(r.u());
    ev.dotDmg = static_cast<int16_t>(r.i());
    ev.enemyAttackMs = static_cast<int16_t>(r.i());
    for (int k = 0; k < kWeaponCount + 6; ++k) {
      const int64_t left = r.i();
      ev.combatCd[k] = left > 0 ? s.now + left : 0;
      ev.combatCdTotal[k] = static_cast<int32_t>(r.i());
    }
    ev.weaponDisabled = static_cast<uint16_t>(r.u());
    ev.lootN = static_cast<uint8_t>(r.idx(kMaxLoot + 1));
    for (int k = 0; k < ev.lootN && !r.bad(); ++k) {
      ev.loot[k].item = static_cast<Item>(r.idx(kStoreCount));
      ev.loot[k].numLeft = static_cast<int16_t>(r.i());
    }
    if (ev.event < 0 || ev.scene >= kEvents[ev.event].sceneN) ev = ActiveEvent();  // an event this firmware lacks
  }
  auto& sp = s.space;
  sp.active = r.b();
  if (sp.active) {
    sp.hull = static_cast<int16_t>(r.i());
    sp.startedAt = s.now - r.i();
    sp.nextStep = s.now + r.i();
    sp.nextWave = s.now + r.i();
    sp.shipX = static_cast<int16_t>(r.i());
    sp.shipY = static_cast<int16_t>(r.i());
    sp.cols = static_cast<int16_t>(r.i());
    sp.rows = static_cast<int16_t>(r.i());
    sp.n = static_cast<uint8_t>(r.idx(kMaxAsteroids + 1));
    for (int k = 0; k < sp.n && !r.bad(); ++k) {
      sp.a[k].x = static_cast<int16_t>(r.i());
      sp.a[k].y = static_cast<int16_t>(r.i());
      sp.a[k].glyph = r.byte();
      sp.a[k].speed = r.byte();
    }
  }
  for (uint32_t& v : g.rng().s) v = static_cast<uint32_t>(r.u());
  return !r.bad();
}

}  // namespace adr
