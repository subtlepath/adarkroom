// ui_common.cpp — what every screen is made of: the header of places, the log, buttons, the
// footer, the menu and its dialogs, the drop menu, and the screens between (death, the ending,
// sleep).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "app.h"
#include "ui.h"

namespace adr {

namespace ui {

Metrics metrics(const App& app) {
  Metrics m;
  App& a = const_cast<App&>(app);
  m.W = a.canvas().width();
  m.H = a.canvas().height();
  m.portrait = m.H > m.W;
  m.touch = app.profile().touch;
  m.margin = 14;
  m.headerH = 48;
  m.btnH = m.touch ? 52 : 42;
  m.rowH = 31;
  m.counterH = m.touch ? 48 : 36;
  m.footerH = 38;
  return m;
}

bool anyCooldown(const Game& g) {
  const GameState& s = g.st();
  if (s.ev.event >= 0) {
    for (int i = 0; i < kMaxButtons; ++i)
      if (g.buttonCooldownLeft(i) > 0) return true;
    for (int b = 0; b < CB_COUNT; ++b)
      if (g.combatCooldownLeft(b) > 0) return true;
    return false;
  }
  switch (s.active) {
    case M_ROOM: return g.onCooldown(CD_FIRE);
    case M_OUTSIDE: return g.onCooldown(CD_GATHER) || g.onCooldown(CD_TRAPS);
    case M_PATH: return g.onCooldown(CD_EMBARK);
    case M_SHIP: return g.onCooldown(CD_LIFTOFF);
    default: return false;
  }
}

const char* shortName(const Game& g, Module m) {
  switch (m) {
    case M_ROOM: return "room";
    case M_OUTSIDE: return g.st().buildings[BD_HUT] ? "village" : "forest";
    case M_PATH: return "path";
    case M_FABRICATOR: return "fabricator";
    case M_SHIP: return "ship";
    default: return "";
  }
}

void count(double v, char* out, int cap) { snprintf(out, cap, "%.0f", std::floor(v + 1e-9)); }

void costText(const Amount* a, int n, char* out, int cap) {
  int len = 0;
  out[0] = 0;
  for (int i = 0; i < n && len < cap; ++i)
    len += snprintf(out + len, cap - len, "%s%s %d", i ? ", " : "", itemName(a[i].item), static_cast<int>(a[i].n));
}

}  // namespace ui

using namespace ui;

// ---- the header: the places, as tabs ------------------------------------------------------------

void App::drawHeader(int y) {
  const Game& g = *_game;
  const Metrics m = metrics(*this);
  static const Module kOrder[] = {M_ROOM, M_OUTSIDE, M_PATH, M_FABRICATOR, M_SHIP};
  const int left = m.portrait ? m.margin : 238, right = m.W - m.margin;
  const Module cur = g.activeModule();
  // the long names if they fit, else the short ones beside the one here, else only the short ones
  const FontId f = F_R25;
  const Font font(f);
  const int sepW = 21;
  int shortNames = 0;  // 1: the others short; 2: all of them
  for (; shortNames < 2; ++shortNames) {
    int w = 0, n = 0;
    for (Module mod : kOrder) {
      if (!g.unlocked(mod)) continue;
      const char* t = (shortNames == 2 || (shortNames && mod != cur)) ? shortName(g, mod) : g.locationTitle(mod);
      w += font.width(t) + (n++ ? sepW : 0);
    }
    if (w <= right - left) break;
  }
  int x = left;
  bool first = true;
  const int base = y + 32;
  for (Module mod : kOrder) {
    if (!g.unlocked(mod)) continue;
    if (!first) {
      _canvas.vline(x + sepW / 2, base - 19, 24);
      x += sepW;
    }
    first = false;
    const char* t = (shortNames == 2 || (shortNames && mod != cur)) ? shortName(g, mod) : g.locationTitle(mod);
    const int x0 = x;
    x = _canvas.text(f, x, base, t, mod == cur ? kInk : 11);
    if (mod == cur) _canvas.hline(x0, base + 4, x - x0, 1);
    addHot({x0 - 4, y + 4, x - x0 + 8, m.headerH - 8}, static_cast<uint16_t>(A_TAB | static_cast<uint16_t>(mod)), true, false);
  }
  _canvas.hline(left, y + m.headerH - 2, right - left);
}

// ---- the log: newest first, fading as the browser's does ----------------------------------------

int App::drawLog(Rect area, int maxLines, bool fromTop) {
  const GameState& s = _game->st();
  const FontId f = F_R25;
  const int lineH = 29, gap = 6;
  int y = area.y + 22, lines = 0;
  (void)fromTop;
  for (int i = s.logN - 1, age = 0; i >= 0 && lines < maxLines; --i, ++age) {
    static const int kFade[] = {kInk, kInk, 12, 10, 8, 7, 6, 5};
    const int level = kFade[age < 7 ? age : 7];
    const char* msg = s.log[i];
    const int len = static_cast<int>(strlen(msg));
    int at = 0;
    while (at < len && lines < maxLines) {
      int next;
      const int n = wrapLine(f, msg + at, len - at, area.w, next);
      if (y + 6 > area.bottom()) return lines;
      _canvas.text(f, area.x, y, msg + at, n, level, false);
      y += lineH;
      lines++;
      at += next;
    }
    y += gap;
  }
  return lines;
}

// ---- a button, as the original draws it ---------------------------------------------------------

void App::drawButton(Rect r, const char* label, bool enabled, float cooldown, uint16_t id, int cdSteps) {
  const bool focus = keyMode() && _focus == id && _overlay == Overlay::None;
  const bool menuFocus = keyMode() && _focus == id;
  const bool isFocused = focus || (menuFocus && _overlay != Overlay::None && (id & 0xFF00) >= A_MENU);
  const Metrics m = metrics(*this);
  const FontId f = F_R25;
  (void)m;
  // The focus: black, the label white. A focused button that cannot be pressed (not merely cooling
  // down) is a thick frame instead, its label grey as the others'.
  const bool black = isFocused && (enabled || cooldown > 0);
  if (black) _canvas.fill(r, true);
  else if (isFocused) _canvas.frame(r, 3);
  else if (enabled && cooldown <= 0) _canvas.frame(r, 1);
  else _canvas.dottedFrame(r);
  // the cooldown: a grey bar shrinking to the left, in steps an e-paper panel can keep up with;
  // on the focused (black) button, a white strip along its foot instead, so the label stays clear
  if (cooldown > 0) {
    const int steps = cdSteps > 0 ? cdSteps : 8;
    const float q = std::ceil(cooldown * steps) / steps;
    const int w = static_cast<int>((r.w - 2) * q);
    if (black) {
      _canvas.fill({r.x + 3, r.bottom() - 7, w - 4 > 0 ? w - 4 : 0, 4}, false);
    } else {
      const Rect bar = {r.x + 1, r.y + 1, w, r.h - 2};
      _canvas.shade(bar, 4);
    }
  }
  const int base = r.y + (r.h + Font(f).ascent() - Font(f).descent()) / 2;
  const int level = (black || (enabled && cooldown <= 0)) ? kInk : 10;
  if (Font(f).width(label) > r.w - 8) _canvas.textFit(f, r.x + 4, base, label, r.w - 8, level, black);
  else _canvas.textCentered(f, r.x + r.w / 2, base, label, level, black);
  addHot(r, id, enabled && cooldown <= 0);
}

// ---- the footer: the focused thing's cost or use, and the way to the next place -----------------

void App::drawFooter() {
  const Metrics m = metrics(*this);
  const int y = m.H - m.footerH;
  const int left = m.portrait ? m.margin : 238;
  _canvas.hline(left, y, m.W - m.margin - left);
  const int base = y + 27;
  int right = m.W - m.margin;
  if (_pages > 1) {
    char p[24];
    snprintf(p, sizeof(p), "%d/%d", _page + 1, _pages);
    right = _canvas.textRight(F_R25, right, base, p) - Font(F_R25).width(p) - 12;
    if (m.touch) {
      addHot({m.W - m.margin - 60, y, 60, m.footerH}, A_SINGLE | S_PAGE_NEXT, _page + 1 < _pages, false);
    }
  }
  if (_detail[0]) caption(left, base, right - left - 8, _detail);
}

// A caption: in the body's size when it fits, a longer one smaller (and cut if even that is short).
int App::caption(int x, int base, int w, const char* s, int level, bool centred) {
  const FontId f = Font(F_I25).width(s) <= w ? F_I25 : F_I20;
  if (centred && Font(f).width(s) <= w) return _canvas.textCentered(f, x + w / 2, base, s, level);
  return _canvas.textFit(f, x, base, s, w, level);
}

// ---- the menu, the dialogs, the drop menu ------------------------------------------------------

namespace {
Rect centredBox(const Metrics& m, int w, int h) { return {(m.W - w) / 2, (m.H - h) / 2, w, h}; }

void panel(Canvas& c, Rect r, const char* title) {
  c.fill(r.inset(-6), false);
  c.frame(r.inset(-6), 1);
  c.fill(r, false);
  c.frame(r, 2);
  if (title) {
    const int w = Font(F_B25).width(title);
    c.fill({r.x + 14, r.y - 16, w + 12, 30}, false);
    c.text(F_B25, r.x + 20, r.y + 7, title);
  }
}
}  // namespace

void App::drawMenu() {
  const Metrics m = metrics(*this);
  const GameState& s = _game->st();
  const int rowH = m.touch ? 56 : 46;
  const int w = m.portrait ? m.W - 2 * 30 : 460;
  const int rows = menuRows();
  const Rect box = centredBox(m, w, rowH * rows + 40);
  _canvas.shade({0, 0, m.W, m.H}, 3);
  panel(_canvas, box, "paused");
  char sleep[32];
  if (_settings.sleepMinutes) snprintf(sleep, sizeof(sleep), "after %d min", _settings.sleepMinutes);
  else snprintf(sleep, sizeof(sleep), "never");
  static const char* const kLight[] = {"off", "firelight", "dim", "half", "full"};
  const char* labels[kMenuItems] = {"resume.", "lights", "hyper", "fights", "sleep", "glass", "frontlight", "restart."};
  const char* values[kMenuItems] = {"", s.settings.lightsOff ? "off" : "on", s.settings.hyper ? "on (x2)" : "off",
                                    tempoName(_settings.tempo), sleep, _settings.rotation == 1 ? "tall" : "wide",
                                    kLight[_settings.light % 5], ""};
  for (int row = 0; row < rows; ++row) {
    const int i = menuItem(row);
    const Rect r = {box.x + 16, box.y + 24 + row * rowH, box.w - 32, rowH - 6};
    const bool f = keyMode() && _menuFocus == row;
    if (f) _canvas.fill(r, true);
    const int base = r.y + r.h / 2 + 8;
    _canvas.text(F_R25, r.x + 10, base, labels[i], kInk, f);
    if (values[i][0]) _canvas.textRight(F_I25, r.right() - 10, base, values[i], kInk, f);
    addHot(r, static_cast<uint16_t>(A_MENU | i), true, false);
  }
  if (keyMode()) {
    snprintf(_detail, sizeof(_detail), "%s", "confirm: choose; left and right: change; back: resume");
    _canvas.fill({box.x - 6, box.bottom() + 10, box.w + 12, 36}, false);
    caption(box.x, box.bottom() + 36, box.w, _detail, kInk, true);
  }
}

void App::drawDialog() {
  const Metrics m = metrics(*this);
  const char* title = "";
  const char* text = "";
  const char* yes = "yes";
  const char* no = "no";
  switch (_dialog) {
    case Dialog::Restart:
      title = "Restart?";
      text = "restart the game?";
      break;
    case Dialog::Hyper:
      title = "Go Hyper?";
      text = "turning hyper mode speeds up the game to x2 speed. do you want to do that?";
      break;
    case Dialog::LiftOff:
      title = "Ready to Leave?";
      text = "time to get out of this place. won't be coming back.";
      yes = "lift off";
      no = "linger";
      break;
    default: break;
  }
  const int w = m.portrait ? m.W - 40 : 480;
  const int bh = m.btnH;
  // the text, wrapped
  const int len = static_cast<int>(strlen(text));
  int lines = 0;
  for (int at = 0; at < len;) {
    int next;
    wrapLine(F_R25, text + at, len - at, w - 40, next);
    at += next;
    lines++;
  }
  const Rect box = centredBox(m, w, 56 + lines * 30 + bh + 30);
  _canvas.shade({0, 0, m.W, m.H}, 3);
  panel(_canvas, box, title);
  int y = box.y + 48;
  for (int at = 0; at < len;) {
    int next;
    const int n = wrapLine(F_R25, text + at, len - at, w - 40, next);
    _canvas.text(F_R25, box.x + 20, y, text + at, n, kInk, false);
    at += next;
    y += 30;
  }
  const int bw = (w - 60) / 2;
  drawButton({box.x + 20, y + 4, bw, bh}, yes, true, 0, A_DIALOG | 0);
  drawButton({box.x + 40 + bw, y + 4, bw, bh}, no, true, 0, A_DIALOG | 1);
}

void App::drawDropMenu() {
  const Metrics m = metrics(*this);
  Game::DropOption opts[kStoreCount];
  const int n = _game->dropOptions(_dropRow, opts);
  const int bh = m.btnH;
  const int w = m.portrait ? m.W - 60 : 420;
  const Rect box = centredBox(m, w, 60 + (n + 1) * (bh + 8));
  _canvas.shade({0, 0, m.W, m.H}, 3);
  panel(_canvas, box, "drop:");
  int y = box.y + 34;
  for (int i = 0; i < n; ++i) {
    char label[48];
    snprintf(label, sizeof(label), "%s x%d", itemName(opts[i].item), opts[i].n);
    drawButton({box.x + 20, y, w - 40, bh}, label, true, 0, static_cast<uint16_t>(A_DROP | i));
    y += bh + 8;
  }
  drawButton({box.x + 20, y, w - 40, bh}, "nothing", true, 0, static_cast<uint16_t>(A_DROP | 0xFF));
  snprintf(_detail, sizeof(_detail), "no room to carry it: leave something behind?");
}

// ---- between the screens ------------------------------------------------------------------------

void App::drawDead() {
  const Metrics m = metrics(*this);
  _canvas.shade({0, 0, m.W, m.H}, 6);
  const Rect r = {m.W / 2 - 170, m.H / 2 - 44, 340, 76};
  _canvas.fill(r, false);
  _canvas.textCentered(F_I25, m.W / 2, m.H / 2 + 5, "the world fades.");
}

void App::drawEnding() {
  const Metrics m = metrics(*this);
  const GameState& s = _game->st();
  int y = m.portrait ? 170 : 90;
  const int cx = m.W / 2;
  if (s.stores[IT_FLEET_BEACON] > 0) {
    static const char* const kOutro[] = {
        "the beacon pulses gently as the ship glides through space.",
        "coordinates are locked. nothing to do but wait.",
        "",
        "the beacon glows a solid blue, and then goes dim. the ship slows.",
        "gradually, the vast wanderer homefleet comes into view.",
        "massive worldships drift unnaturally through clouds of debris, scarred and dead.",
        "",
        "the air is running out.",
        "the capsule is cold.",
    };
    const int w = m.W - 2 * 30;
    y = m.portrait ? 80 : 44;
    for (const char* line : kOutro) {
      const int len = static_cast<int>(strlen(line));
      if (!len) {
        y += 16;
        continue;
      }
      for (int at = 0; at < len;) {
        int next;
        const int n = wrapLine(F_I25, line + at, len - at, w, next);
        const Font font(F_I25);
        _canvas.text(F_I25, cx - font.width(line + at, n) / 2, y, line + at, n, kInk, false);
        at += next;
        y += 30;
      }
    }
    y += 36;
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "score for this game: %d", static_cast<int>(s.lastScore));
  _canvas.textCentered(F_R25, cx, y, buf);
  y += 36;
  snprintf(buf, sizeof(buf), "total score: %d", static_cast<int>(s.prevScore));
  _canvas.textCentered(F_R25, cx, y, buf);
  y += m.portrait ? 56 : 30;
  const int bw = 220;
  drawButton({cx - bw / 2, y, bw, m.btnH + 6}, "restart.", true, 0, A_ENDING | 0);
  snprintf(_detail, sizeof(_detail), "a new wanderer, and what the last one left behind.");
  caption(m.margin, y + m.btnH + 44, m.W - 2 * m.margin, _detail, kInk, true);
}

void App::renderSleep(bool lowBattery) {
  const GameState& s = _game->st();
  _hotN = 0;
  _canvas.clear();
  const Metrics m = metrics(*this);
  const int cx = m.W / 2;
  // the middle, clear of the keys' hints around the edges
  const int inner = m.W - 2 * 60;
  auto centred = [&](FontId f, const char* text, int& y, int lineH) {
    const int len = static_cast<int>(strlen(text));
    for (int at = 0; at < len;) {
      int next;
      const int n = wrapLine(f, text + at, len - at, inner, next);
      _canvas.text(f, cx - Font(f).width(text + at, n) / 2, y, text + at, n, kInk, false);
      at += next;
      y += lineH;
    }
  };
  const char* where = s.space.active ? "Space" : s.ev.event >= 0 ? kEvents[s.ev.event].title
                                                                  : _game->locationTitle(_game->activeModule());
  int y = m.H / 2 - 120;
  _canvas.textCentered(F_R34, cx, y, "A Dark Room");
  y += 26;
  _canvas.hline(cx - 70, y, 140);
  y += 60;
  centred(F_I25, where, y, 30);
  y += 14;
  char line[80];
  if (!s.ending) {
    snprintf(line, sizeof(line), "the fire is %s. the room is %s.", Game::fireText(s.fire), Game::tempText(s.temperature));
    centred(F_R25, line, y, 30);
    y += 4;
    if (s.population > 0) {
      snprintf(line, sizeof(line), "%d souls in the village.", static_cast<int>(s.population));
      centred(F_R25, line, y, 30);
      y += 4;
    }
  }
  y += 30;
  caption((m.W - inner) / 2, y, inner,
          lowBattery ? "the battery is spent. charge it, then press power." : "the world waits. press power to return.", kInk,
          true);
  drawKeyHints();
  if (s.settings.lightsOff) {
    uint8_t* fb = _canvas.buffer();
    for (int i = 0; i < _canvas.bytes(); ++i) fb[i] = static_cast<uint8_t>(~fb[i]);
  }
  _lastCrc = 0;
  _screenKey = 0xFFFFFFFF;
}

// ---- the keys, pointed out where they are on the device -----------------------------------------

namespace {
const char* keyLabel(KeyRole r) {
  switch (r) {
    case KR_UP: return "last place";
    case KR_DOWN: return "next place";
    case KR_BACK: return "menu";
    case KR_CONFIRM: return "choose";
    case KR_LEFT: return "previous";
    case KR_RIGHT: return "next";
    case KR_POWER: return "wake";
    case KR_HOME: return "menu";
  }
  return "";
}
}  // namespace

// Each key gets an arrow at the edge of the glass beside it, and what it does. The keys are placed as
// the device is held tall, and follow the glass round when it is turned wide: a label on the top or
// the bottom reads across, one on a side runs along it.
void App::drawKeyHints() {
  const DeviceProfile& d = *_profile;
  const int W = _canvas.width(), H = _canvas.height();
  const int tallW = d.panelH, tallH = d.panelW;  // the page held tall (R90)
  const FontId f = F_R25;
  const Font font(f);
  const int bezel = 8, arrow = 11, gap = 5, apart = 16;
  struct Hint {
    int edge;  // the page's: 0 top, 1 right, 2 bottom, 3 left
    int at;    // the arrow, along that edge
    int mid;   // the label's middle, along it
    int w;
    const char* label;
  } hints[8];
  int n = 0;
  for (int i = 0; i < d.keyN && n < 8; ++i) {
    const KeyMark& k = d.keys[i];
    // the key's place on the edge of the glass held tall, to the panel, to the page as it is turned
    int tx = k.at, ty = k.at;
    if (k.edge == KE_TOP) ty = 0;
    else if (k.edge == KE_BOTTOM) ty = tallH - 1;
    else tx = k.edge == KE_LEFT ? 0 : tallW - 1;
    const Point p = _canvas.toPage(ty, d.panelH - 1 - tx);
    Hint& h = hints[n++];
    h.edge = p.y == 0 ? 0 : p.y == H - 1 ? 2 : p.x == W - 1 ? 1 : 3;
    h.at = h.edge == 0 || h.edge == 2 ? p.x : p.y;
    h.label = keyLabel(k.role);
    h.w = font.width(h.label);
    h.mid = h.at;
  }
  // labels close together on one edge (the first X4's rocker) move apart; the arrows stay
  for (int pass = 0; pass < 3; ++pass)
    for (int i = 0; i < n; ++i)
      for (int j = 0; j < n; ++j) {
        Hint &a = hints[i], &b = hints[j];
        if (i == j || a.edge != b.edge || a.mid > b.mid) continue;
        const int over = a.mid + a.w / 2 + apart + b.w / 2 - b.mid;
        if (over <= 0) continue;
        a.mid -= over / 2;
        b.mid += over - over / 2;
      }
  for (int i = 0; i < n; ++i) {
    const Hint& h = hints[i];
    const int len = h.edge == 0 || h.edge == 2 ? W : H;
    const int lo = 12 + h.w / 2, hi = len - 12 - h.w / 2;
    const int c = h.mid < lo ? lo : h.mid > hi ? hi : h.mid;
    if (h.edge == 0 || h.edge == 2) {
      // on the top or the bottom: the arrow points out through the edge, the label inside it
      const bool top = h.edge == 0;
      const int tip = top ? bezel : H - 1 - bezel;
      const int dir = top ? 1 : -1;
      for (int r = 0; r < arrow; ++r) _canvas.hline(h.at - r * 7 / 10, tip + dir * r, r * 7 / 5 + 1);
      const int base = top ? tip + arrow + gap + font.ascent() : tip - arrow - gap - font.descent();
      _canvas.text(f, c - h.w / 2, base, h.label);
    } else {
      // on a side: the label runs along it, the tops of its letters toward the edge
      const bool right = h.edge == 1;
      const int tip = right ? W - 1 - bezel : bezel;
      const int dir = right ? -1 : 1;
      for (int r = 0; r < arrow; ++r) _canvas.vline(tip + dir * r, h.at - r * 7 / 10, r * 7 / 5 + 1);
      const int top = tip + dir * (arrow + gap);  // where the tops of the letters are
      if (right) _canvas.textTurned(f, top - font.ascent(), c - h.w / 2, h.label, true);
      else _canvas.textTurned(f, top + font.ascent(), c + h.w / 2, h.label, false);
    }
  }
}

}  // namespace adr

namespace adr {
namespace ui {

void describeFocus(const Game& g, uint16_t id, char* out, int cap) {
  const GameState& s = g.st();
  const int group = id & 0xFF00, idx = id & 0xFF;
  out[0] = 0;
  Amount a[3];
  int n = 0;
  switch (group) {
    case A_SINGLE:
      switch (idx) {
        case S_FIRE:
          snprintf(out, cap, "wood %d", s.fire == 0 ? 5 : 1);
          break;
        case S_GATHER: snprintf(out, cap, "wood +%d", s.buildings[BD_CART] > 0 ? 50 : 10); break;
        case S_TRAPS: snprintf(out, cap, "%d traps, %d baited", s.buildings[BD_TRAP],
                               static_cast<int>(g.store(IT_BAIT) < s.buildings[BD_TRAP] ? g.store(IT_BAIT) : s.buildings[BD_TRAP])); break;
        case S_EMBARK: snprintf(out, cap, "%s", g.canEmbark() ? "into the barren world" : "take cured meat to set out"); break;
        case S_HULL: case S_ENGINE: snprintf(out, cap, "alien alloy 1"); break;
        case S_LIFTOFF: snprintf(out, cap, "%s", s.hull > 0 ? "time to get out of this place" : "the hull needs reinforcing first"); break;
        default: break;
      }
      break;
    case A_BUILD: {
      g.craftCost(idx, a, n);
      costText(a, n, out, cap);
      break;
    }
    case A_BUY: {
      const TradeGood& t = kTrades[idx];
      for (const auto& c : t.cost)
        if (c.item != IT_NONE) a[n++] = {c.item, c.n};
      costText(a, n, out, cap);
      break;
    }
    case A_FAB: {
      a[n++] = {IT_ALIEN_ALLOY, kFabs[idx].alloy};
      costText(a, n, out, cap);
      break;
    }
    case A_WORKER: {
      const WorkerInfo& w = kWorkers[idx];
      int len = snprintf(out, cap, "each:");
      for (const auto& k : w.stores)
        if (k.item != IT_NONE && len < cap)
          len += snprintf(out + len, cap - len, " %s %+g", itemName(k.item), k.n2 / 2.0);
      if (len < cap) snprintf(out + len, cap - len, " per %ds", w.delay);
      break;
    }
    case A_OUTFIT: {
      const Item it = static_cast<Item>(idx);
      const WeaponInfo* wi = weaponFor(it);
      char what[48] = "";
      if (it == IT_CURED_MEAT) snprintf(what, sizeof(what), "restores %d hp", g.meatHeal());
      else if (it == IT_MEDICINE) snprintf(what, sizeof(what), "restores 20 hp");
      else if (it == IT_HYPO) snprintf(what, sizeof(what), "restores 30 hp");
      else if (it == IT_BULLETS) snprintf(what, sizeof(what), "use with rifle");
      else if (it == IT_ENERGY_CELL) snprintf(what, sizeof(what), "emits a soft red glow");
      else if (wi && wi->damage > 0) snprintf(what, sizeof(what), "damage %d", wi->damage);
      else if (wi) snprintf(what, sizeof(what), "stuns");
      const double w = Game::weight(it);
      snprintf(out, cap, "%s%sweight %g, available %d", what, what[0] ? "; " : "", w,
               static_cast<int>(g.store(it) - s.outfit[it]));
      break;
    }
    case A_EVBTN: {
      const Scene* sc = g.scene();
      if (sc && idx < sc->buttonN) costText(sc->buttons[idx].cost, sc->buttons[idx].costN, out, cap);
      break;
    }
    case A_COMBAT: {
      if (idx < kWeaponCount) {
        const WeaponInfo& w = kWeapons[idx];
        if (w.ammo != IT_NONE) snprintf(out, cap, "%s 1 (%d left)", itemName(w.ammo), static_cast<int>(s.outfit[w.ammo]));
        else if (w.damage > 0) snprintf(out, cap, "damage %d", w.damage);
      } else if (idx == CB_EAT) snprintf(out, cap, "cured meat 1 (%d left): +%d hp", static_cast<int>(s.outfit[IT_CURED_MEAT]), g.meatHeal());
      else if (idx == CB_MEDS) snprintf(out, cap, "medicine 1 (%d left): +20 hp", static_cast<int>(s.outfit[IT_MEDICINE]));
      else if (idx == CB_HYPO) snprintf(out, cap, "hypo 1 (%d left): +30 hp", static_cast<int>(s.outfit[IT_HYPO]));
      else if (idx == CB_STIM) snprintf(out, cap, "faster blows, for 10 hp");
      else if (idx == CB_SHIELD) snprintf(out, cap, "the next blow is turned");
      break;
    }
    case A_LOOT: {
      if (idx < s.ev.lootN)
        snprintf(out, cap, "confirm: take one; right: take all (weight %g, free %g)", Game::weight(s.ev.loot[idx].item),
                 std::floor(g.freeSpace() * 10 + 0.5) / 10);
      break;
    }
    default: break;
  }
}

}  // namespace ui
}  // namespace adr
