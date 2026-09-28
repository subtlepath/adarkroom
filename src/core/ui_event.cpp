// ui_event.cpp — an event on the glass: its story, its loot and its choices, and the fights.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "app.h"
#include "ui.h"

namespace adr {

using namespace ui;

// Wrapped text, left-aligned. Returns the y after it.
static int paragraph(Canvas& c, FontId f, int x, int y, int w, const char* s, int lineH, int level = kInk) {
  const int len = static_cast<int>(strlen(s));
  for (int at = 0; at < len;) {
    int next;
    const int n = wrapLine(f, s + at, len - at, w, next);
    c.text(f, x, y, s + at, n, level, false);
    at += next;
    y += lineH;
  }
  return y;
}

void App::drawEvent() {
  const Game& g = *_game;
  const GameState& s = g.st();
  const Metrics m = metrics(*this);
  const EventDef* ev = g.event();
  const Scene* sc = g.scene();
  if (!ev || !sc) return;
  const int left = m.margin + 6, width = m.W - 2 * left;
  // the title, as the event panel heads it
  _canvas.frame({m.margin - 4, 6, m.W - 2 * (m.margin - 4), m.H - 12}, 1);
  int y = 46;
  _canvas.textFit(F_B34, left, y, ev->title, width);
  y += 12;
  _canvas.hline(left, y, width);
  y += 34;
  if (sc->combat && !s.ev.lootShown) {
    drawCombat(y);
    return;
  }
  // the story (or, after a fight, how it ended)
  if (sc->combat) {
    y = paragraph(_canvas, F_R25, left, y, width, sc->deathMessage ? sc->deathMessage : "", 30);
  } else {
    for (int i = 0; i < sc->textN; ++i) y = paragraph(_canvas, F_R25, left, y, width, sc->text[i], 30) + 8;
  }
  y += 2;
  // the loot
  if (sc->loot) {
    const int rowH = m.btnH + 6;
    const int boxTop = y;
    y += 30;
    if (s.ev.lootN == 0) {
      _canvas.text(F_I25, left + 12, y + 14, "nothing to take", 10);
      y += 30;
    }
    const double free = g.freeSpace();
    for (int i = 0; i < s.ev.lootN; ++i) {
      const LootRow& lr = s.ev.loot[i];
      char label[48];
      snprintf(label, sizeof(label), "%s [%d]", itemLabel(lr.item), lr.numLeft);
      const int allW = 112;
      drawButton({left + 12, y, width - 24 - allW - 8, m.btnH}, label, true, 0, static_cast<uint16_t>(A_LOOT | i));
      const double w = Game::weight(lr.item);
      int fit = w > 0 ? static_cast<int>(std::floor(free / w + 1e-9)) : lr.numLeft;
      if (fit > lr.numLeft) fit = lr.numLeft;
      char all[24];
      if (fit >= lr.numLeft) snprintf(all, sizeof(all), "take all");
      else snprintf(all, sizeof(all), "take %d", fit);
      const Rect ar = {left + width - 12 - allW, y, allW, m.btnH};
      if (fit > 0) _canvas.frame(ar, 1);
      else _canvas.dottedFrame(ar);
      _canvas.textCentered(F_R25, ar.x + ar.w / 2, ar.y + m.btnH / 2 + 8, all, fit > 0 ? kInk : 8);
      addHot(ar, static_cast<uint16_t>(A_LOOT_ALL | i), fit > 0, false);
      if (_focus == (A_LOOT | i)) describeFocus(g, _focus, _detail, sizeof(_detail));
      y += rowH;
    }
    if (s.ev.lootN > 0) {
      // take everything (and, when it all fits and there is but one way on, leave too)
      char label[64];
      const bool everything = s.ev.lootFits;
      const char* leave = nullptr;
      if (s.ev.takeLeave) leave = (sc->combat && sc->buttonN == 0) ? "leave" : sc->buttons[0].text;
      snprintf(label, sizeof(label), "%s%s%s", everything ? "take everything" : "take all you can", leave ? " and " : "",
               leave ? leave : "");
      drawButton({left + 12, y, width - 24, m.btnH}, label, g.canTakeAnything(), 0, A_SINGLE | S_TAKE_EVERYTHING);
      y += rowH;
    }
    // the box around it, with its legend
    _canvas.frame({left, boxTop + 10, width, y - boxTop - 6}, 1);
    _canvas.fill({left + 8, boxTop, Font(F_R25).width("take:") + 8, 24}, false);
    _canvas.text(F_R25, left + 12, boxTop + 18, "take:");
    y += 16;
  }
  // the choices
  const int bh = m.btnH + 4;
  if (sc->combat && sc->buttonN == 0) {
    // after a fight with no way on but out: leave, and a bite to eat first
    const int per = m.portrait ? 2 : 3, bw = (width - 8 * (per - 1)) / per;
    int col = 0;
    for (int b : {static_cast<int>(CB_LEAVE), static_cast<int>(CB_EAT), static_cast<int>(CB_MEDS), static_cast<int>(CB_HYPO)}) {
      if (!g.combatButtonShown(b)) continue;
      drawButton({left + col * (bw + 8), y, bw, bh}, g.combatButtonText(b), g.combatButtonEnabled(b),
                 g.combatCooldownLeft(b), static_cast<uint16_t>(A_COMBAT | b), 4);
      if (_focus == (A_COMBAT | b)) describeFocus(g, _focus, _detail, sizeof(_detail));
      if (++col == per) {
        col = 0;
        y += bh + 8;
      }
    }
    if (col) y += bh + 8;
  } else {
    for (int i = 0; i < sc->buttonN; ++i) {
      const EventButton& b = sc->buttons[i];
      const Rect r = {left, y, width, bh};
      const bool en = g.buttonAvailable(i);
      drawButton(r, b.text, en, g.buttonCooldownLeft(i), static_cast<uint16_t>(A_EVBTN | i), 4);
      // what it costs, at its right, if the label leaves it room (the footer says it too)
      if (b.costN) {
        char cost[64];
        costText(b.cost, b.costN, cost, sizeof(cost));
        const bool f = keyMode() && _focus == (A_EVBTN | i);
        const int room = (width - Font(F_R25).width(b.text)) / 2 - 14;
        const FontId cf = Font(F_I25).width(cost) <= room ? F_I25 : F_I20;
        if (Font(cf).width(cost) <= room) _canvas.textRight(cf, r.right() - 10, r.y + bh / 2 + 8, cost, kInk, f);
      }
      if (_focus == (A_EVBTN | i)) describeFocus(g, _focus, _detail, sizeof(_detail));
      y += bh + 8;
    }
  }
  // the latest notices
  const int logTop = y + 6;
  if (logTop < m.H - 90) drawLog({left, logTop, width, m.H - logTop - 16}, 3, true);
}

// ---- a fight ------------------------------------------------------------------------------------

static const char* statusName(uint8_t st) {
  switch (st) {
    case ST_SHIELD: return "shielded";
    case ST_ENRAGED: return "enraged";
    case ST_MEDITATION: return "meditating";
    case ST_BOOST: return "boosted";
    case ST_ENERGISED: return "energised";
    case ST_VENOMOUS: return "venomous";
    default: return nullptr;
  }
}

void App::drawCombat(int y) {
  const Game& g = *_game;
  const GameState& s = g.st();
  const Metrics m = metrics(*this);
  const Scene& sc = *g.scene();
  const int left = m.margin + 6, width = m.W - 2 * left;
  if (sc.notification) y = paragraph(_canvas, F_I25, left, y, width, sc.notification, 30) + 6;
  // the fighters: the wanderer on the left, the enemy on the right, as the browser has them
  const int fy = y + 10, fh = 164;
  auto fighter = [&](const Fighter& f, const char* chara, int cx, bool enemy) {
    const bool gone = enemy && s.ev.fought;
    const char* label = enemy && s.ev.exploding ? "*" : chara;
    const int level = gone ? 5 : kInk;
    _canvas.textCentered(F_MB30, cx, fy + 40, label, level);
    char hp[16];
    snprintf(hp, sizeof(hp), "%d/%d", f.hp, f.maxHp);
    _canvas.textCentered(F_R25, cx, fy + 74, hp, level);
    // the hp as a bar
    const int bw = 110;
    const Rect bar = {cx - bw / 2, fy + 84, bw, 8};
    _canvas.frame(bar, 1);
    if (f.maxHp > 0) _canvas.fill({bar.x + 1, bar.y + 1, (bw - 2) * f.hp / f.maxHp, 6}, true);
    if (const char* st = statusName(f.status)) _canvas.textCentered(F_I25, cx, fy + 118, st);
    if (f.stunned) _canvas.textCentered(F_I25, cx, fy + 146, "stunned");
    // the last blow on it, while it is fresh
    if (f.floatText[0] && s.now - f.floatAt < 2500) {
      const int fw = Font(F_B25).width(f.floatText) + 12;
      const Rect r = {cx + 30, fy + 2, fw, 32};
      _canvas.fill(r, false);
      _canvas.frame(r, 1);
      _canvas.textCentered(F_B25, r.x + r.w / 2, r.y + 24, f.floatText);
    }
  };
  fighter(s.ev.wanderer, "@", left + width / 4, false);
  fighter(s.ev.enemy, sc.chara ? sc.chara : "?", left + width * 3 / 4, true);
  y = fy + fh;
  if (s.ev.fought) {
    _canvas.textCentered(F_I25, m.W / 2, y + 10, "...");
    return;
  }
  // the attacks, then the rest
  const int bh = m.btnH + 4;
  const int per = m.portrait ? 2 : 3, bw = (width - 8 * (per - 1)) / per;
  int col = 0;
  auto btn = [&](int b) {
    drawButton({left + col * (bw + 8), y, bw, bh}, g.combatButtonText(b), g.combatButtonEnabled(b), g.combatCooldownLeft(b),
               static_cast<uint16_t>(A_COMBAT | b), 4);
    if (_focus == (A_COMBAT | b)) describeFocus(g, _focus, _detail, sizeof(_detail));
    if (++col == per) {
      col = 0;
      y += bh + 8;
    }
  };
  for (int b = 0; b < kWeaponCount; ++b)
    if (g.combatButtonShown(b)) btn(b);
  if (col) {
    col = 0;
    y += bh + 8;
  }
  y += 8;
  for (int b = CB_EAT; b <= CB_SHIELD; ++b)
    if (g.combatButtonShown(b)) btn(b);
  if (col) y += bh + 8;
  const int logTop = y + 4;
  if (logTop < m.H - 90) drawLog({left, logTop, width, m.H - logTop - 48}, 3, true);
  if (_detail[0]) caption(left, m.H - 22, width, _detail);
}

}  // namespace adr
