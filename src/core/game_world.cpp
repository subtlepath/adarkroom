// game_world.cpp — the barren world (world.js): the map, the wanderer's moves, water and food,
// the landmarks, and the way home.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "game.h"

namespace adr {

const Landmark kLandmarks[] = {
    {TL_OUTPOST, 0, 0, 0, "outpost", "An Outpost"},
    {TL_IRON_MINE, 1, 5, 5, "ironmine", "Iron Mine"},
    {TL_COAL_MINE, 1, 10, 10, "coalmine", "Coal Mine"},
    {TL_SULPHUR_MINE, 1, 20, 20, "sulphurmine", "Sulphur Mine"},
    {TL_HOUSE, 10, 0, 45, "house", "An Old House"},
    {TL_CAVE, 5, 3, 10, "cave", "A Damp Cave"},
    {TL_TOWN, 10, 10, 20, "town", "An Abandoned Town"},
    {TL_CITY, 20, 20, 45, "city", "A Ruined City"},
    {TL_SHIP, 1, 28, 28, "ship", "A Crashed Starship"},
    {TL_BOREHOLE, 10, 15, 45, "borehole", "A Borehole"},
    {TL_BATTLEFIELD, 5, 18, 45, "battlefield", "A Battlefield"},
    {TL_SWAMP, 1, 15, 45, "swamp", "A Murky Swamp"},
    {TL_EXECUTIONER, 1, 28, 28, "executioner", "A Ravaged Battleship"},
    {TL_CACHE, 1, 10, 45, "cache", "A Destroyed Village"},  // only with prestige
};
const int kLandmarkCount = sizeof(kLandmarks) / sizeof(kLandmarks[0]);

const Landmark* landmarkFor(uint8_t tile) {
  for (const Landmark& l : kLandmarks)
    if (l.tile == tile) return &l;
  return nullptr;
}

static bool isTerrain(uint8_t t) { return t == TL_FOREST || t == TL_FIELD || t == TL_BARRENS; }

// ---- the map ------------------------------------------------------------------------------------

int Game::chooseTile(int x, int y, const WorldMap& m) {
  const uint8_t adjacent[4] = {
      static_cast<uint8_t>(y > 0 ? m.tile[x][y - 1] : 0),
      static_cast<uint8_t>(y < kRadius * 2 ? m.tile[x][y + 1] : 0),
      static_cast<uint8_t>(x < kRadius * 2 ? m.tile[x + 1][y] : 0),
      static_cast<uint8_t>(x > 0 ? m.tile[x - 1][y] : 0),
  };
  // chances, in the order the original's object first sees each tile
  uint8_t keys[8];
  double chance[8];
  int n = 0;
  double nonSticky = 1;
  auto slot = [&](uint8_t t) {
    for (int i = 0; i < n; ++i)
      if (keys[i] == t) return i;
    keys[n] = t;
    chance[n] = 0;
    return n++;
  };
  for (uint8_t a : adjacent) {
    if (a == TL_VILLAGE) return TL_FOREST;  // the village stands in a forest
    if (a) {
      chance[slot(a)] += 0.5;
      nonSticky -= 0.5;
    }
  }
  // World.TILE order: FOREST, FIELD, BARRENS
  static const uint8_t kTerrain[3] = {TL_FOREST, TL_FIELD, TL_BARRENS};
  static const double kProb[3] = {0.15, 0.35, 0.5};
  for (int t = 0; t < 3; ++t) chance[slot(kTerrain[t])] += kProb[t] * nonSticky;
  // sorted by chance, highest first (a stable sort, as V8's)
  int order[8];
  for (int i = 0; i < n; ++i) order[i] = i;
  for (int i = 1; i < n; ++i)
    for (int j = i; j > 0 && chance[order[j]] > chance[order[j - 1]]; --j) {
      const int tmp = order[j];
      order[j] = order[j - 1];
      order[j - 1] = tmp;
    }
  double c = 0;
  const double roll = r.real();
  for (int i = 0; i < n; ++i) {
    c += chance[order[i]];
    if (roll < c) return keys[order[i]];
  }
  return TL_BARRENS;
}

void Game::placeLandmark(int minR, int maxR, uint8_t tile, WorldMap& m) {
  int x = kRadius, y = kRadius;
  while (!isTerrain(m.tile[x][y])) {
    const int radius = static_cast<int>(std::floor(r.real() * (maxR - minR))) + minR;
    int xDist = static_cast<int>(std::floor(r.real() * radius));
    int yDist = radius - xDist;
    if (r.real() < 0.5) xDist = -xDist;
    if (r.real() < 0.5) yDist = -yDist;
    x = kRadius + xDist;
    if (x < 0) x = 0;
    if (x > kRadius * 2) x = kRadius * 2;
    y = kRadius + yDist;
    if (y < 0) y = 0;
    if (y > kRadius * 2) y = kRadius * 2;
  }
  m.tile[x][y] = tile;
}

void Game::generateMap(uint32_t seed, WorldMap& m) {
  // The map comes from its own seed, so a save need only keep the seed and what changed since.
  const Rng saved = r;
  r.seed(seed);
  memset(m.tile, 0, sizeof(m.tile));
  m.tile[kRadius][kRadius] = TL_VILLAGE;
  for (int rad = 1; rad <= kRadius; ++rad) {
    for (int t = 0; t < rad * 8; ++t) {
      int x, y;
      if (t < 2 * rad) {
        x = kRadius - rad + t;
        y = kRadius - rad;
      } else if (t < 4 * rad) {
        x = kRadius + rad;
        y = kRadius - (3 * rad) + t;
      } else if (t < 6 * rad) {
        x = kRadius + (5 * rad) - t;
        y = kRadius + rad;
      } else {
        x = kRadius - rad;
        y = kRadius + (7 * rad) - t;
      }
      m.tile[x][y] = static_cast<uint8_t>(chooseTile(x, y, m));
    }
  }
  for (const Landmark& l : kLandmarks) {
    if (l.tile == TL_CACHE && !s.hasPrev) continue;
    for (int k = 0; k < l.num; ++k) placeLandmark(l.minRadius, l.maxRadius, l.tile, m);
  }
  r = saved;
}

void Game::uncoverMap(int x, int y, int rad, WorldMap& m) {
  m.mask[x][y] = 1;
  for (int i = -rad; i <= rad; ++i)
    for (int j = -rad + std::abs(i); j <= rad - std::abs(i); ++j)
      if (y + j >= 0 && y + j <= kRadius * 2 && x + i <= kRadius * 2 && x + i >= 0) m.mask[x + i][y + j] = 1;
}

void Game::lightMap(int x, int y, WorldMap& m) { uncoverMap(x, y, 2 * (hasPerk(PK_SCOUT) ? 2 : 1), m); }

void Game::testMap() {
  if (s.seenAll) return;
  for (int i = 0; i < kMapSize; ++i)
    for (int j = 0; j < kMapSize; ++j)
      if (!s.world.mask[i][j]) return;
  s.seenAll = true;
}

void Game::applyMap() {
  if (!s.seenAll) {
    int x, y;
    do {
      x = static_cast<int>(std::floor(r.real() * kMapSize));
      y = static_cast<int>(std::floor(r.real() * kMapSize));
    } while (s.world.mask[x][y]);
    uncoverMap(x, y, 5, s.world);
  }
  testMap();
  changed = true;
}

const char* Game::compassDir() const {
  int sx = 0, sy = 0;
  for (int i = 0; i < kMapSize; ++i)
    for (int j = 0; j < kMapSize; ++j)
      if ((s.world.tile[i][j] & 0x7F) == TL_SHIP) {
        sx = i - kRadius;
        sy = j - kRadius;
        i = kMapSize;
        break;
      }
  static char dir[24];
  const char* horz = sx < 0 ? "west" : "east";
  const char* vert = sy < 0 ? "north" : "south";
  if (std::abs(sx) / 2.0 > std::abs(sy)) snprintf(dir, sizeof(dir), "%s", horz);
  else if (std::abs(sy) / 2.0 > std::abs(sx)) snprintf(dir, sizeof(dir), "%s", vert);
  else snprintf(dir, sizeof(dir), "%s%s", vert, horz);
  return dir;
}

void Game::initWorld() {
  if (!s.hasWorld) {
    s.hasWorld = true;
    s.executionerPlaced = true;
    s.mapSeed = r.next();
    generateMap(s.mapSeed, s.world);
    memset(s.world.mask, 0, sizeof(s.world.mask));
    lightMap(kRadius, kRadius, s.world);
    s.world.flags = 0;
  }
  testMap();
}

int Game::distance(int x, int y) const { return std::abs(x - kRadius) + std::abs(y - kRadius); }

// ---- the expedition -----------------------------------------------------------------------------

void Game::worldArrival() {
  Trip& t = s.trip;
  memset(static_cast<void*>(&t), 0, sizeof(t));
  t.active = true;
  memcpy(&t.world, &s.world, sizeof(t.world));
  t.x = kRadius;
  t.y = kRadius;
  cancel(T_DEATH_RETURN);
  s.cd[CD_EMBARK].end = 0;
  setWater(maxWater());
  setHp(maxHealth());
  changed = true;
}

void Game::setWater(int w) {
  s.trip.water = static_cast<int16_t>(w > maxWater() ? maxWater() : w);
  changed = true;
}

void Game::setHp(int hp) {
  s.trip.health = static_cast<int16_t>(hp > maxHealth() ? maxHealth() : hp);
  changed = true;
}

void Game::move(Dir d) {
  Trip& t = s.trip;
  if (!t.active || t.dead || s.active != M_WORLD || s.ev.event >= 0) return;
  int dx = 0, dy = 0;
  switch (d) {
    case DIR_NORTH: if (t.y > 0) dy = -1; break;
    case DIR_SOUTH: if (t.y < kRadius * 2) dy = 1; break;
    case DIR_WEST: if (t.x > 0) dx = -1; break;
    case DIR_EAST: if (t.x < kRadius * 2) dx = 1; break;
  }
  if (!dx && !dy) return;
  const uint8_t oldTile = t.world.tile[t.x][t.y];
  t.x = static_cast<int8_t>(t.x + dx);
  t.y = static_cast<int8_t>(t.y + dy);
  narrateMove(oldTile, t.world.tile[t.x][t.y]);
  lightMap(t.x, t.y, t.world);
  doSpace();
  if (s.active == M_WORLD && checkDanger()) {
    if (s.danger) notify(M_WORLD, "dangerous to be this far from the village without proper protection");
    else notify(M_WORLD, "safer here");
  }
  changed = true;
  refresh();
}

void Game::narrateMove(uint8_t from, uint8_t to) {
  const char* msg = nullptr;
  if (from == TL_FOREST) {
    if (to == TL_FIELD) msg = "the trees yield to dry grass. the yellowed brush rustles in the wind.";
    else if (to == TL_BARRENS) msg = "the trees are gone. parched earth and blowing dust are poor replacements.";
  } else if (from == TL_FIELD) {
    if (to == TL_FOREST)
      msg = "trees loom on the horizon. grasses gradually yield to a forest floor of dry branches and fallen leaves.";
    else if (to == TL_BARRENS) msg = "the grasses thin. soon, only dust remains.";
  } else if (from == TL_BARRENS) {
    if (to == TL_FIELD) msg = "the barrens break at a sea of dying grass, swaying in the arid breeze.";
    else if (to == TL_FOREST)
      msg = "a wall of gnarled trees rises from the dust. their branches twist into a skeletal canopy overhead.";
  }
  if (msg) notify(M_WORLD, msg);
}

bool Game::checkDanger() {
  if (!s.danger) {
    if (store(IT_I_ARMOUR) == 0 && distance() >= 8) {
      s.danger = true;
      return true;
    }
    if (store(IT_S_ARMOUR) == 0 && distance() >= 18) {
      s.danger = true;
      return true;
    }
  } else {
    if (distance() < 8) {
      s.danger = false;
      return true;
    }
    // (the original compares the function World.getDistance itself here, never true)
  }
  return false;
}

bool Game::useSupplies() {
  Trip& t = s.trip;
  t.foodMove++;
  t.waterMove++;
  const int movesPerFood = 2 * (hasPerk(PK_SLOW_METABOLISM) ? 2 : 1);
  if (t.foodMove >= movesPerFood) {
    t.foodMove = 0;
    int num = s.outfit[IT_CURED_MEAT];
    num--;
    if (num == 0) {
      notify(M_WORLD, "the meat has run out");
    } else if (num < 0) {
      num = 0;
      if (!t.starvation) {
        notify(M_WORLD, "starvation sets in");
        t.starvation = true;
      } else {
        s.starved++;
        if (s.starved >= 10 && !hasPerk(PK_SLOW_METABOLISM)) addPerk(PK_SLOW_METABOLISM);
        die();
        return false;
      }
    } else {
      t.starvation = false;
      setHp(t.health + meatHeal());
    }
    s.outfit[IT_CURED_MEAT] = num;
  }
  const int movesPerWater = 1 * (hasPerk(PK_DESERT_RAT) ? 2 : 1);
  if (t.waterMove >= movesPerWater) {
    t.waterMove = 0;
    int water = t.water;
    water--;
    if (water == 0) {
      notify(M_WORLD, "there is no more water");
    } else if (water < 0) {
      water = 0;
      if (!t.thirst) {
        notify(M_WORLD, "the thirst becomes unbearable");
        t.thirst = true;
      } else {
        s.dehydrated++;
        if (s.dehydrated >= 10 && !hasPerk(PK_DESERT_RAT)) addPerk(PK_DESERT_RAT);
        die();
        return false;
      }
    } else {
      t.thirst = false;
    }
    setWater(water);
  }
  return true;
}

void Game::checkFight() {
  s.fightMove++;
  if (s.fightMove > 3) {
    const double chance = 0.2 * (hasPerk(PK_STEALTHY) ? 0.5 : 1);
    if (r.real() < chance) {
      s.fightMove = 0;
      triggerFight();
    }
  }
}

void Game::doSpace() {
  Trip& t = s.trip;
  const uint8_t cur = t.world.tile[t.x][t.y];
  if (cur == TL_VILLAGE) {
    goHome();
  } else if (cur == TL_EXECUTIONER) {
    const int e = findEvent((t.world.flags & WF_EXECUTIONER) ? "executioner-antechamber" : "executioner-intro");
    if (e >= 0) startEvent(e);
  } else if (landmarkFor(cur)) {
    if (cur != TL_OUTPOST || !t.outpostUsed(t.x, t.y)) {
      const int e = findEvent(landmarkFor(cur)->scene);
      if (e >= 0) startEvent(e);
    }
  } else {
    if (useSupplies()) checkFight();
  }
}

void Game::useOutpost() {
  notify(M_NONE, "water replenished");
  setWater(maxWater());
  s.trip.useOutpost(s.trip.x, s.trip.y);
}

void Game::clearDungeon() {
  s.trip.world.tile[s.trip.x][s.trip.y] = TL_OUTPOST;
  drawRoad();
}

void Game::drawRoad() {
  Trip& t = s.trip;
  auto& map = t.world.tile;
  const int sx = t.x, sy = t.y;
  // the closest road, spiralling out along lines of equal manhattan distance
  int rx = kRadius, ry = kRadius;
  {
    int x = 0, y = 0, dx = 1, dy = -1;
    const int d = distance(sx, sy) + 2;
    bool found = false;
    for (int i = 0; i < d * d && !found; ++i) {
      const int qx = sx + x, qy = sy + y;
      if (0 < qx && qx < kRadius * 2 && 0 < qy && qy < kRadius * 2) {
        const uint8_t tile = map[qx][qy];
        if (tile == TL_ROAD || (tile == TL_OUTPOST && !(x == 0 && y == 0)) || tile == TL_VILLAGE) {
          rx = qx;
          ry = qy;
          found = true;
          break;
        }
      }
      if (x == 0 || y == 0) {
        const int tmp = dx;
        dx = -dy;
        dy = tmp;
      }
      if (x == 0 && y <= 0) {
        x++;
      } else {
        x += dx;
        y += dy;
      }
    }
  }
  const int xDist = sx - rx, yDist = sy - ry;
  const int xDir = xDist > 0 ? 1 : xDist < 0 ? -1 : 0;
  const int yDir = yDist > 0 ? 1 : yDist < 0 ? -1 : 0;
  int xIntersect, yIntersect;
  if (std::abs(xDist) > std::abs(yDist)) {
    xIntersect = rx;
    yIntersect = ry + yDist;
  } else {
    xIntersect = rx + xDist;
    yIntersect = ry;
  }
  for (int x = 0; x < std::abs(xDist); ++x)
    if (isTerrain(map[rx + xDir * x][yIntersect])) map[rx + xDir * x][yIntersect] = TL_ROAD;
  for (int y = 0; y < std::abs(yDist); ++y)
    if (isTerrain(map[xIntersect][ry + yDir * y])) map[xIntersect][ry + yDir * y] = TL_ROAD;
  changed = true;
}

void Game::goHome() {
  Trip& t = s.trip;
  memcpy(&s.world, &t.world, sizeof(s.world));
  testMap();
  if ((s.world.flags & WF_SULPHURMINE) && s.buildings[BD_SULPHUR_MINE] == 0) setBuilding(BD_SULPHUR_MINE, 1);
  if ((s.world.flags & WF_IRONMINE) && s.buildings[BD_IRON_MINE] == 0) setBuilding(BD_IRON_MINE, 1);
  if ((s.world.flags & WF_COALMINE) && s.buildings[BD_COAL_MINE] == 0) setBuilding(BD_COAL_MINE, 1);
  if ((s.world.flags & WF_SHIP) && !s.hasShip) initShip();
  if ((s.world.flags & WF_EXECUTIONER) && !s.hasFabricator) {
    initFabricator();
    notify(M_NONE,
           "builder knows the strange device when she sees it. takes it for herself real quick. doesn\xE2\x80\x99t "
           "ask where it came from.");
  }
  redeemBlueprints();
  t.active = false;
  returnOutfit();
  s.active = M_PATH;
  refresh();
  changed = true;
}

void Game::redeemBlueprints() {
  static const struct {
    Item bp;
    uint8_t bit;
  } kBp[] = {{IT_HYPO_BLUEPRINT, 1}, {IT_KINETIC_ARMOUR_BLUEPRINT, 2}, {IT_DISRUPTOR_BLUEPRINT, 4},
             {IT_PLASMA_RIFLE_BLUEPRINT, 8}, {IT_STIM_BLUEPRINT, 16}, {IT_GLOWSTONE_BLUEPRINT, 32}};
  bool redeemed = false;
  for (const auto& b : kBp) {
    if (s.outfit[b.bp]) {
      s.blueprints |= b.bit;
      s.outfit[b.bp] = 0;
      redeemed = true;
    }
  }
  if (redeemed) notify(M_NONE, "blueprints feed into the fabricator data port. possibilities grow.");
}

void Game::returnOutfit() {
  for (int i = 0; i < kStoreCount; ++i) {
    if (!s.outfit[i]) continue;
    const Item it = static_cast<Item>(i);
    addStore(it, s.outfit[i]);
    // World.leaveItAtHome: supplies and weapons stay packed for the next trip
    bool keep = it == IT_CURED_MEAT || it == IT_BULLETS || it == IT_ENERGY_CELL || it == IT_CHARM ||
                it == IT_MEDICINE || it == IT_STIM || it == IT_HYPO || weaponFor(it) != nullptr;
    for (int c = 0; c < kCraftCount; ++c)
      if (kCrafts[c].key && kCrafts[c].item == it) keep = true;
    if (!keep) s.outfit[i] = 0;
  }
}

void Game::die() {
  Trip& t = s.trip;
  if (t.dead) return;
  t.dead = true;
  notify(M_WORLD, "the world fades");
  t.active = false;
  memset(s.outfit, 0, sizeof(s.outfit));
  // the world fades (600 ms), then two seconds of nothing, then the room
  schedule(T_DEATH_RETURN, 2600, 0, true);
  changed = true;
}

}  // namespace adr
