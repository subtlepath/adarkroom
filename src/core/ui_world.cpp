// ui_world.cpp — the barren world: the map around the wanderer, the supplies, what the landmarks
// in sight are, and the whole map at a glance.
//
// The browser shows all 61 x 61 tiles at 10 px; e-paper cannot at any size worth reading, so the
// map here is a window on the wanderer (who stays in its middle), set in a monospace as the
// browser's is: the land in grey, the landmarks in bold, the way home a road of '#'. The whole
// map, a pixel-drawn overview, is a key away.
#include <cstdio>
#include <cstring>

#include "app.h"
#include "ui.h"

namespace adr {

using namespace ui;

namespace {

constexpr int kCellW = 18, kCellH = 22;

struct MapFrame {
  Rect map;           // where the cells are
  int cols, rows;
  Rect supplies, legend, log;
};

MapFrame mapFrame(const Metrics& m) {
  MapFrame f;
  if (m.portrait) {
    const int top = m.headerH + 72;
    const int bottomBand = 30 + 96 + m.footerH;
    f.cols = (m.W - 2 * m.margin) / kCellW;
    if (!(f.cols & 1)) f.cols--;
    f.rows = (m.H - top - bottomBand - 8) / kCellH;
    if (!(f.rows & 1)) f.rows--;
    const int w = f.cols * kCellW, h = f.rows * kCellH;
    f.map = {(m.W - w) / 2, top, w, h};
    f.supplies = {m.margin, m.headerH + 2, m.W - 2 * m.margin, 64};
    f.legend = {m.margin, f.map.bottom() + 6, m.W - 2 * m.margin, 30};
    f.log = {m.margin, f.legend.bottom(), m.W - 2 * m.margin, 94};
  } else {
    const int left = 258;
    f.cols = (m.W - left - m.margin) / kCellW;
    if (!(f.cols & 1)) f.cols--;
    f.rows = (m.H - m.headerH - 12 - 34) / kCellH;
    if (!(f.rows & 1)) f.rows--;
    const int w = f.cols * kCellW, h = f.rows * kCellH;
    f.map = {left + (m.W - left - m.margin - w) / 2, m.headerH + 8, w, h};
    f.supplies = {m.margin, 16, 222, 130};
    f.legend = {left, f.map.bottom() + 4, m.W - left - m.margin, 30};
    f.log = {m.margin, 150, 222, m.H - 150 - 12};
  }
  return f;
}

}  // namespace

Point ui::mapCentre(const App& app) {
  const MapFrame f = mapFrame(metrics(app));
  return {f.map.x + (f.cols / 2) * kCellW + kCellW / 2, f.map.y + (f.rows / 2) * kCellH + kCellH / 2};
}

// ---- the supplies, as the world panel lists them -------------------------------------------------

static void supplyLine(const Game& g, char* out, int cap) {
  const GameState& s = g.st();
  int len = 0;
  out[0] = 0;
  // cured meat first, then the rest in the stores' order
  if (s.outfit[IT_CURED_MEAT] > 0) len += snprintf(out, cap, "cured meat %d", static_cast<int>(s.outfit[IT_CURED_MEAT]));
  for (int i = 0; i < kStoreCount && len < cap; ++i) {
    if (i == IT_CURED_MEAT || s.outfit[i] <= 0) continue;
    len += snprintf(out + len, cap - len, "%s%s %d", len ? " \xC2\xB7 " : "", itemLabel(static_cast<Item>(i)), static_cast<int>(s.outfit[i]));
  }
}

void App::drawWorld() {
  const Game& g = *_game;
  const GameState& s = g.st();
  const Trip& t = s.trip;
  const Metrics m = metrics(*this);
  const MapFrame f = mapFrame(m);
  // the header: where, and the wanderer's state
  const int hx = m.portrait ? m.margin : 258;
  char buf[240];
  snprintf(buf, sizeof(buf), "hp %d/%d", t.health, g.maxHealth());
  int right = _canvas.textRight(F_B25, m.W - m.margin, 32, buf) - Font(F_B25).width(buf);
  snprintf(buf, sizeof(buf), "water %d/%d", t.water, g.maxWater());
  right = _canvas.textRight(F_R25, right - 16, 32, buf) - Font(F_R25).width(buf);
  // the place's name, if the wanderer's state leaves it room
  const char* title = right - 16 - hx >= Font(F_R25).width("A Barren World") ? "A Barren World" : "the world";
  _canvas.text(F_R25, hx, 32, title);
  _canvas.hline(hx, m.headerH - 2, m.W - m.margin - hx);
  if (m.touch) {
    const char* label = "whole map";
    const int w = Font(F_R25).width(label) + 16;
    const Rect r = {right - 16 - w, 6, w, 36};
    if (r.x > hx + Font(F_R25).width(title) + 10) {
      _canvas.frame(r, 1);
      _canvas.textCentered(F_R25, r.x + r.w / 2, 32, label);
      addHot(r, A_SINGLE | S_OVERVIEW, true, false);
    }
  }
  // the supplies
  supplyLine(g, buf, sizeof(buf));
  {
    const char* bagName = g.store(IT_RUCKSACK) > 0 ? "rucksack" : "pockets";
    char bag[48];
    snprintf(bag, sizeof(bag), "%s: free %d/%d", bagName, static_cast<int>(g.freeSpace()), g.capacity());
    int y = f.supplies.y + 24;
    const int len = static_cast<int>(strlen(buf));
    int at = 0, lines = 0;
    const int maxLines = m.portrait ? 1 : 3;
    while (at < len && lines < maxLines) {
      int next;
      const int n = wrapLine(F_R25, buf + at, len - at, f.supplies.w, next);
      if (lines == maxLines - 1 && at + next < len) _canvas.textFit(F_R25, f.supplies.x, y, buf + at, f.supplies.w);
      else _canvas.text(F_R25, f.supplies.x, y, buf + at, n, kInk, false);
      at += next;
      y += 29;
      lines++;
    }
    if (!lines) {
      _canvas.text(F_I25, f.supplies.x, y, "nothing carried", 10);
      y += 29;
    }
    _canvas.text(F_I25, f.supplies.x, y, bag, 11);
  }
  // the map
  const int cx = f.cols / 2, cy = f.rows / 2;
  _canvas.frame({f.map.x - 3, f.map.y - 3, f.map.w + 6, f.map.h + 6}, 1);
  bool seenLandmark[32] = {};
  bool seenVillage = false;
  for (int row = 0; row < f.rows; ++row)
    for (int col = 0; col < f.cols; ++col) {
      const int mx = t.x + col - cx, my = t.y + row - cy;
      const int px = f.map.x + col * kCellW, py = f.map.y + row * kCellH;
      if (mx < 0 || my < 0 || mx >= kMapSize || my >= kMapSize) {
        _canvas.shade({px, py, kCellW, kCellH}, 2);  // beyond the edge of the world
        continue;
      }
      const int base = py + 17;
      if (mx == t.x && my == t.y) {
        _canvas.fill({px, py, kCellW, kCellH}, true);
        _canvas.textCentered(F_MB22, px + kCellW / 2, base, "@", kInk, true);
        continue;
      }
      if (!t.world.mask[mx][my]) continue;
      const uint8_t tile = t.world.tile[mx][my];
      const char c[2] = {static_cast<char>(tile & 0x7F), 0};
      const Landmark* lm = (tile & kVisited) ? nullptr : landmarkFor(tile);
      if (lm && tile == TL_OUTPOST && t.outpostUsed(mx, my)) lm = nullptr;
      if (tile == TL_VILLAGE) {
        _canvas.textCentered(F_MB22, px + kCellW / 2, base, c);
        seenVillage = true;
      } else if (lm) {
        _canvas.textCentered(F_MB22, px + kCellW / 2, base, c);
        seenLandmark[lm - kLandmarks] = true;
      } else {
        _canvas.textCentered(F_M22, px + kCellW / 2, base, c, tile == TL_ROAD ? kInk : 10);
      }
    }
  // what the landmarks in sight are (the browser's tooltips)
  {
    int x = f.legend.x;
    const int base = f.legend.y + 22;
    auto entry = [&](char c, const char* label) {
      char key[2] = {c, 0};
      const int w = Font(F_MB22).width(key) + 6 + Font(F_R25).width(label) + 16;
      if (x + w > f.legend.right()) return;
      x = _canvas.text(F_MB22, x, base, key) + 6;
      x = _canvas.text(F_R25, x, base, label, 12) + 16;
    };
    if (seenVillage) entry('A', "The Village");
    for (int i = 0; i < kLandmarkCount; ++i)
      if (seenLandmark[i]) entry(static_cast<char>(kLandmarks[i].tile), kLandmarks[i].label);
  }
  drawLog(f.log, m.portrait ? 3 : 14, true);
  if (keyMode()) snprintf(_detail, sizeof(_detail), "the keys walk the wanderer; confirm: the whole map");
  if (m.portrait) drawFooter();
}

// ---- the whole map ------------------------------------------------------------------------------

void App::drawOverview() {
  const Game& g = *_game;
  const GameState& s = g.st();
  const Trip& t = s.trip;
  const Metrics m = metrics(*this);
  const int cell = m.portrait ? (m.W - 2 * m.margin) / kMapSize : (m.H - m.headerH - 30) / kMapSize;
  const int size = cell * kMapSize;
  const Rect map = {(m.W - size) / 2, m.portrait ? 70 : 60, size, size};
  _canvas.text(F_R25, m.margin, 32, m.portrait ? "the world, as far as known" : "the world, as far as it is known");
  _canvas.hline(m.margin, m.headerH - 2, m.W - 2 * m.margin);
  _canvas.frame({map.x - 3, map.y - 3, map.w + 6, map.h + 6}, 1);
  for (int x = 0; x < kMapSize; ++x)
    for (int y = 0; y < kMapSize; ++y) {
      const int px = map.x + x * cell, py = map.y + y * cell;
      if (x == t.x && y == t.y) {
        _canvas.fill({px - 1, py - 1, cell + 2, cell + 2}, true);
        _canvas.fill({px + cell / 2 - 1, py + cell / 2 - 1, 2, 2}, false);
        continue;
      }
      if (!t.world.mask[x][y]) continue;
      const uint8_t tile = t.world.tile[x][y];
      const Landmark* lm = (tile & kVisited) ? nullptr : landmarkFor(tile);
      if (lm && tile == TL_OUTPOST && t.outpostUsed(x, y)) lm = nullptr;
      const int c = cell / 2;
      switch (tile & 0x7F) {
        case TL_FOREST:
          _canvas.pixel(px + c, py + c - 1);
          _canvas.pixel(px + c - 1, py + c + 1);
          _canvas.pixel(px + c + 1, py + c + 1);
          break;
        case TL_FIELD: _canvas.pixel(px + c, py + c); break;
        case TL_BARRENS: break;
        case TL_ROAD: _canvas.fill({px + c - 1, py + c - 1, 3, 3}, true); break;
        case TL_VILLAGE:
          _canvas.frame({px, py, cell, cell}, 1);
          _canvas.fill({px + c - 1, py + c - 1, 2, 2}, true);
          break;
        default:
          if (lm) _canvas.fill({px + 1, py + 1, cell - 2, cell - 2}, true);
          else _canvas.frame({px + 1, py + 1, cell - 2, cell - 2}, 1);
          break;
      }
    }
  // the key, beside the map when the glass is wide, under it when tall
  const int keyX = m.portrait ? m.margin + 10 : map.right() + 24;
  int y = m.portrait ? map.bottom() + 36 : map.y + 24;
  int x = keyX;
  auto key = [&](int kind, const char* label) {
    const int s2 = 11;
    const int w = s2 + 8 + Font(F_R25).width(label);
    if (x > keyX && x + w > m.W - m.margin) {
      x = keyX;
      y += 32;
    }
    const Rect r = {x, y - 13, s2, s2};
    if (kind == 0) _canvas.fill(r, true);
    else if (kind == 1) _canvas.frame(r, 1);
    else if (kind == 2) _canvas.fill({x + 4, y - 9, 3, 3}, true);
    else {
      _canvas.fill(r, true);
      _canvas.fill({x + 4, y - 9, 3, 3}, false);
    }
    x = _canvas.text(F_R25, x + s2 + 8, y, label) + 22;
  };
  key(3, "the wanderer");
  key(0, "unexplored");
  key(1, "explored");
  key(2, "road");
  y += 40;
  char buf[64];
  snprintf(buf, sizeof(buf), "%d from the village", g.distance());
  _canvas.text(F_I25, keyX, y, buf);
  if (keyMode()) snprintf(_detail, sizeof(_detail), "the keys still walk; confirm or back: the map around");
  if (m.touch) addHot(map, A_SINGLE | S_OVERVIEW, true, false);
  drawFooter();
}

}  // namespace adr
