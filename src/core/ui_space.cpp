// ui_space.cpp — the ascent: the sky darkening from the troposphere to space, the rocks, the ship.
#include <cstdio>

#include "app.h"
#include "ui.h"

namespace adr {

using namespace ui;

void App::drawSpace() {
  Game& g = *_game;
  const GameState& s = g.st();
  const Metrics m = metrics(*this);
  const int cell = 28;
  const int top = 48;
  const int cols = (m.W - 2 * 8) / cell;
  const int rows = (m.H - top - 8) / cell;
  g.spaceLayout(cols, rows);
  const int ox = (m.W - cols * cell) / 2;
  // the sky: white to black over the minute, as the browser's background fades
  const int alt = g.altitude();
  int dark = alt * kInk / 55;
  if (dark > kInk) dark = kInk;
  const bool night = dark > kInk / 2;
  const Rect sky = {ox, top, cols * cell, rows * cell};
  _canvas.shade(sky, dark);
  // the stars, drifting down as the ship climbs
  const int drift = static_cast<int>((s.now - s.space.startedAt) / 250);
  for (int i = 0; i < 90; ++i) {
    uint32_t h = static_cast<uint32_t>(i) * 2654435761u;
    const int x = sky.x + static_cast<int>(h % static_cast<uint32_t>(sky.w));
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    const int y = sky.y + static_cast<int>((h % static_cast<uint32_t>(sky.h) + drift * (1 + i % 3)) % static_cast<uint32_t>(sky.h));
    _canvas.pixel(x, y, !night);
    if (i % 4 == 0) _canvas.pixel(x + 1, y, !night);
  }
  // the rocks
  for (int i = 0; i < s.space.n; ++i) {
    const Asteroid& a = s.space.a[i];
    const Rect r = {ox + a.x * cell, top + a.y * cell, cell, cell};
    _canvas.fill(r, night);
    const char c[2] = {static_cast<char>(a.glyph), 0};
    _canvas.textCentered(F_MB30, r.x + cell / 2, r.y + 22, c, kInk, night);
  }
  // the ship
  {
    const Rect r = {ox + s.space.shipX * cell, top + s.space.shipY * cell, cell, cell};
    _shipRect = r;
    _canvas.fill(r.inset(-2), !night);
    _canvas.textCentered(F_MB30, r.x + cell / 2, r.y + 22, "@", kInk, !night);
  }
  // the header
  _canvas.fill({0, 0, m.W, top - 4}, false);
  _canvas.text(F_R25, m.margin, 32, g.altitudeName());
  char buf[32];
  snprintf(buf, sizeof(buf), "hull: %d/%d", s.space.hull, s.hull);
  _canvas.textRight(F_R25, m.W - m.margin, 32, buf);
  _canvas.hline(0, top - 4, m.W, 1);
  if (s.space.hitFlash) _canvas.frame({0, top - 4, m.W, m.H - top + 4}, 6);
}

}  // namespace adr
