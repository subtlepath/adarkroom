// game_space.cpp — the ascent (space.js), stepped for e-paper.
//
// The original is a 30 fps dodging game: asteroids ('#', '$', '%', '&', 'H') fall across 700 px
// in 0.5 to 1.5 s, and the ship slides at (3 + thrusters) px a frame. An e-paper panel shows two
// or three frames a second, so here the sky is a grid that steps twice a second: an asteroid
// falls one or two rows a step, and the ship moves a cell each key press, or as many cells a step
// as its thrusters allow while a key is held. The rest is the original's: sixty seconds from the
// troposphere to space, the waves thickening at 10, 20 and 40, a hull that takes one hit per
// point, and a crash back to the ship when it runs out. On a sky twenty cells wide the waves come
// about 2.5 times less often than the original's, which keeps the chance an idle ship is hit (an
// asteroid covers a whole cell here, not a sliver of 700 px) about where it was.
#include <cmath>
#include <cstring>

#include "game.h"

namespace adr {

namespace {
constexpr int kStepMs = Game::kSpaceStepMs;
constexpr int kAscentMs = 60000;  // Space.FTB_SPEED
}  // namespace

const char* Game::spaceTitle() const { return altitudeName(); }

const char* Game::altitudeName() const {
  const int alt = altitude() / 10 * 10;  // (Space.setTitle runs only when the altitude is a multiple of ten)
  if (alt < 10) return "Troposphere";
  if (alt < 20) return "Stratosphere";
  if (alt < 30) return "Mesosphere";
  if (alt < 45) return "Thermosphere";
  if (alt < 60) return "Exosphere";
  return "Space";
}

void Game::spaceLayout(int cols, int rows) {
  if (cols < 5) cols = 5;
  if (rows < 8) rows = 8;
  if (s.space.cols == cols && s.space.rows == rows) return;
  if (s.space.cols > 0) {  // a turned glass: keep the ship in the sky
    s.space.shipX = static_cast<int16_t>(s.space.shipX * cols / s.space.cols);
    s.space.shipY = static_cast<int16_t>(s.space.shipY * rows / s.space.rows);
    for (int i = 0; i < s.space.n; ++i) {
      s.space.a[i].x = static_cast<int16_t>(s.space.a[i].x * cols / s.space.cols);
      s.space.a[i].y = static_cast<int16_t>(s.space.a[i].y * rows / s.space.rows);
    }
  }
  s.space.cols = static_cast<int16_t>(cols);
  s.space.rows = static_cast<int16_t>(rows);
}

void Game::spaceArrival() {
  auto& sp = s.space;
  const int16_t cols = sp.cols ? sp.cols : 20, rows = sp.rows ? sp.rows : 28;
  sp = {};
  sp.cols = cols;
  sp.rows = rows;
  sp.active = true;
  sp.hull = s.hull;
  sp.startedAt = s.now;
  sp.nextStep = s.now + kStepMs;
  sp.nextWave = s.now + 1000;
  sp.shipX = static_cast<int16_t>(cols / 2);
  sp.shipY = static_cast<int16_t>(rows * 3 / 4);
  steerX = steerY = 0;
  s.active = M_SPACE;
  changed = true;
}

// A key press: one cell, at once.
void Game::spaceNudge(int dx, int dy) {
  auto& sp = s.space;
  if (!sp.active) return;
  sp.shipX = static_cast<int16_t>(sp.shipX + dx);
  sp.shipY = static_cast<int16_t>(sp.shipY + dy);
  if (sp.shipX < 0) sp.shipX = 0;
  if (sp.shipX >= sp.cols) sp.shipX = static_cast<int16_t>(sp.cols - 1);
  if (sp.shipY < 1) sp.shipY = 1;
  if (sp.shipY >= sp.rows) sp.shipY = static_cast<int16_t>(sp.rows - 1);
  // flying into a rock
  for (int i = 0; i < sp.n; ++i)
    if (sp.a[i].x == sp.shipX && sp.a[i].y == sp.shipY) {
      sp.a[i] = sp.a[--sp.n];
      spaceHit();
      break;
    }
  changed = true;
}

int Game::spaceSpeed() const {
  // (3 + thrusters) px a frame in the original: one cell a step at the start, up to four
  const int v = static_cast<int>(std::lround((3 + s.thrusters) / 4.0));
  return v < 1 ? 1 : v > 4 ? 4 : v;
}

void Game::spaceHit() {
  auto& sp = s.space;
  sp.hull--;
  sp.hitFlash = 2;
  if (sp.hull <= 0) crash();
}

void Game::spaceWave() {
  auto& sp = s.space;
  const int alt = altitude();
  int count = 1;
  if (alt > 10) count += 1;
  if (alt > 20) count += 2;
  if (alt > 40) count += 2;
  for (int k = 0; k < count && sp.n < kMaxAsteroids; ++k) {
    const double rr = r.real();
    const uint8_t glyph = rr < 0.2 ? '#' : rr < 0.4 ? '$' : rr < 0.6 ? '%' : rr < 0.8 ? '&' : 'H';
    Asteroid& a = sp.a[sp.n++];
    a.x = static_cast<int16_t>(std::floor(r.real() * sp.cols));
    a.y = 0;
    a.glyph = glyph;
    // the original's durations run 525..1500 ms: about a third of them twice as fast
    a.speed = static_cast<uint8_t>(r.real() < 0.35 ? 2 : 1);
  }
  // Space.createAsteroid's delay, 1000 - altitude * 10 ms, drawn out to the grid's pace (tuned for a
  // sky twenty cells wide; a wider or narrower one gets its waves as much more or less often)
  sp.nextWave += static_cast<int64_t>((1000 - alt * 10) * 2.5 * 20.0 / (sp.cols > 0 ? sp.cols : 20));
}

void Game::spaceStep() {
  auto& sp = s.space;
  if (sp.hitFlash) sp.hitFlash--;
  // the held keys
  const int v = spaceSpeed();
  for (int k = 0; k < v && (steerX || steerY) && sp.active; ++k) spaceNudge(steerX, steerY);
  if (!sp.active) return;
  // the rocks fall
  for (int i = 0; i < sp.n;) {
    Asteroid& a = sp.a[i];
    bool hit = false;
    for (int k = 0; k < a.speed; ++k) {
      a.y++;
      if (a.x == sp.shipX && a.y == sp.shipY) {
        hit = true;
        break;
      }
    }
    if (hit || a.y >= sp.rows) {
      sp.a[i] = sp.a[--sp.n];
      if (hit) {
        spaceHit();
        if (!sp.active) return;
      }
      continue;
    }
    ++i;
  }
  while (sp.nextWave <= s.now && sp.active) spaceWave();
  if (s.now - sp.startedAt >= kAscentMs) endGame();
}

void Game::crash() {
  auto& sp = s.space;
  if (!sp.active) return;
  sp.active = false;
  sp.n = 0;
  s.active = M_SHIP;
  startCooldown(CD_LIFTOFF, 120);
  changed = true;
}

void Game::endGame() {
  auto& sp = s.space;
  sp.active = false;
  sp.won = true;
  sp.n = 0;
  for (Timer& t : s.timers) t.kind = T_NONE;
  s.lastScore = score();
  // Prestige.save: a share of the stores for the next wanderer, and the running score
  int32_t prev[24];
  prestigeStores(prev, true);
  memcpy(s.prevStores, prev, sizeof(prev));
  s.hasPrev = true;
  s.prevScore += s.lastScore;
  s.ending = true;
  s.gameOver = true;
  changed = true;
}

}  // namespace adr
