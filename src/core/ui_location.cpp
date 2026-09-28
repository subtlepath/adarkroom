// ui_location.cpp — the room, the village, the path, the fabricator and the ship.
//
// Each place is a set of blocks, as the browser draws them: the things to do on the left
// (buttons, the workers, the supplies), the things counted on the right (the stores, the village,
// the perks), each in its bordered box with its legend. When a late game holds more than the
// glass, the blocks run on to a second page (the focus, a swipe, or the footer turns it). Held
// tall, the glass is too narrow for two columns of the body's type: the blocks go one under another.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app.h"
#include "ui.h"

namespace adr {

using namespace ui;

namespace {

enum RowType : uint8_t { RB_BUTTON, RB_TEXT, RB_COUNTER, RB_TEXT2 };

struct Row {
  RowType type;
  const char* label;
  const char* sub;  // RB_TEXT: a small note before the value; RB_TEXT2: the second line
  char value[16];
  uint16_t id;
  bool enabled;
  bool canDec, canInc;
  float cd;
  uint8_t steps;
  uint8_t level;
  char subBuf[16];
};

constexpr int kMaxRows = 40;
constexpr int kMaxBlocks = 7;

}  // namespace

struct App::Block {
  const char* legend;
  bool actions;
  uint8_t perRow;
  Row rows[kMaxRows];
  int n;
  Row& add(RowType t, const char* label, uint16_t id = 0) {
    Row& r = rows[n < kMaxRows ? n++ : kMaxRows - 1];
    memset(&r, 0, sizeof(r));
    r.type = t;
    r.label = label;
    r.id = id;
    r.enabled = true;
    r.level = kInk;
    return r;
  }
};

namespace {

App::Block gBlocks[kMaxBlocks];

// Where a block went: which of its units (a row of buttons, or a line) on which page and column.
struct Placed {
  int block, from, to, page, col, y, h;
};
Placed gPlaced[kMaxBlocks * 4];
int gPlacedN;

}  // namespace

// ---- the stores, as the room's panel lists them --------------------------------------------------

static bool storeShown(const Game& g, Item i, bool weapons) {
  if (!g.storeSeen(i)) return false;
  const ItemType t = itemType(i);
  if (t == ItemType::Upgrade || t == ItemType::Blueprint || t == ItemType::Pseudo) return false;
  return (t == ItemType::Weapon) == weapons;
}

static int byName(const void* a, const void* b) {
  return strcmp(itemLabel(*static_cast<const Item*>(a)), itemLabel(*static_cast<const Item*>(b)));
}

// The net income of a store, per ten seconds (every source's delay is ten).
static double netIncome(const Game& g, Item it) {
  const GameState& s = g.st();
  double total = 0;
  for (int k = 0; k < s.incomeN; ++k) {
    const IncomeRec& inc = s.income[s.incomeOrder[k]];
    for (int j = 0; j < inc.n; ++j)
      if (inc.item[j] == it) total += inc.amount[j];
  }
  return total;
}

static void addStores(const Game& g, App::Block& b, bool weapons) {
  Item items[kStoreCount];
  int n = 0;
  for (int i = 0; i < kStoreCount; ++i)
    if (storeShown(g, static_cast<Item>(i), weapons)) items[n++] = static_cast<Item>(i);
  qsort(items, n, sizeof(Item), byName);
  // the special things (the compass) after the rest, as the original appends them
  for (int pass = 0; pass < 2; ++pass)
    for (int i = 0; i < n; ++i) {
      if ((itemType(items[i]) == ItemType::Special) != (pass == 1)) continue;
      Row& r = b.add(RB_TEXT, itemLabel(items[i]));
      count(g.store(items[i]), r.value, sizeof(r.value));
      const double inc = weapons ? 0 : netIncome(g, items[i]);
      if (inc != 0) {
        snprintf(r.subBuf, sizeof(r.subBuf), inc == std::floor(inc) ? "%+.0f" : "%+.1f", inc);
        r.sub = r.subBuf;
      }
    }
}

// ---- the places ---------------------------------------------------------------------------------

void App::drawLocation() {
  const Game& g = *_game;
  const GameState& s = g.st();
  const Metrics m = metrics(*this);
  int nb = 0;
  auto block = [&](const char* legend, bool actions, int perRow) -> Block& {
    Block& b = gBlocks[nb < kMaxBlocks ? nb++ : kMaxBlocks - 1];
    b.legend = legend;
    b.actions = actions;
    b.perRow = static_cast<uint8_t>(perRow);
    b.n = 0;
    return b;
  };
  char buf[64];

  switch (g.activeModule()) {
    case M_ROOM: {
      Block& a = block(nullptr, true, 2);
      Row& fire = a.add(RB_BUTTON, s.fire == 0 ? "light fire" : "stoke fire", A_SINGLE | S_FIRE);
      fire.cd = g.cooldownLeft(CD_FIRE);
      fire.steps = 10;
      Block& build = block("build:", true, 2);
      Block& craft = block("craft:", true, 2);
      for (int i = 0; i < kCraftCount; ++i) {
        if (!g.craftVisible(i) || g.craftAtMax(i)) continue;
        Block& to = kCrafts[i].building != BD_COUNT ? build : craft;
        to.add(RB_BUTTON, kCrafts[i].key, static_cast<uint16_t>(A_BUILD | i));
      }
      Block& buy = block("buy:", true, 2);
      for (int i = 0; i < kTradeCount; ++i) {
        if (!g.tradeVisible(i) || g.tradeAtMax(i)) continue;
        buy.add(RB_BUTTON, itemLabel(kTrades[i].item), static_cast<uint16_t>(A_BUY | i));
      }
      Block& st = block("stores", false, 1);
      addStores(g, st, false);
      Block& wp = block("weapons", false, 1);
      addStores(g, wp, true);
      break;
    }
    case M_OUTSIDE: {
      Block& a = block(nullptr, true, 2);
      Row& gather = a.add(RB_BUTTON, "gather wood", A_SINGLE | S_GATHER);
      gather.cd = g.cooldownLeft(CD_GATHER);
      gather.steps = 12;
      if (s.buildingSeen >> BD_TRAP & 1) {
        Row& traps = a.add(RB_BUTTON, "check traps", A_SINGLE | S_TRAPS);
        traps.cd = g.cooldownLeft(CD_TRAPS);
        traps.steps = 12;
        traps.enabled = s.buildings[BD_TRAP] > 0;
      }
      if (s.population > 0 || s.workerSeen > 1) {
        Block& w = block("workers", true, 1);
        Row& gat = w.add(RB_TEXT, "gatherer");
        snprintf(gat.value, sizeof(gat.value), "%d", g.numGatherers());
        // the others, by name
        int order[WK_COUNT], n = 0;
        for (int k = 1; k < WK_COUNT; ++k)
          if (s.workerSeen >> k & 1) order[n++] = k;
        for (int i = 1; i < n; ++i)
          for (int j = i; j > 0 && strcmp(kWorkers[order[j]].key, kWorkers[order[j - 1]].key) < 0; --j) {
            const int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
          }
        for (int i = 0; i < n; ++i) {
          const int k = order[i];
          Row& r = w.add(RB_COUNTER, kWorkers[k].key, static_cast<uint16_t>(A_WORKER | k));
          snprintf(r.value, sizeof(r.value), "%d", static_cast<int>(s.workers[k]));
          r.canDec = s.workers[k] > 0;
          r.canInc = g.numGatherers() > 0;
        }
      }
      // the village: its people, then its buildings by name (the traps told apart by their bait)
      Block& v = block(s.buildings[BD_HUT] == 0 ? "forest" : "village", false, 1);
      struct Named {
        const char* name;
        int n;
      } rows[BD_COUNT + 1];
      int nr = 0;
      for (int b = 0; b < BD_COUNT; ++b) {
        if (!(s.buildingSeen >> b & 1)) continue;
        if (b == BD_TRAP) {
          const int bait = static_cast<int>(g.store(IT_BAIT));
          const int traps = s.buildings[BD_TRAP];
          if (traps - bait > 0) rows[nr++] = {"trap", traps - bait};
          if ((bait > traps ? traps : bait) > 0) rows[nr++] = {"baited trap", bait > traps ? traps : bait};
        } else if (s.buildings[b] > 0) {
          rows[nr++] = {kBuildingNames[b], s.buildings[b]};
        }
      }
      if (nr) {
        for (int i = 1; i < nr; ++i)
          for (int j = i; j > 0 && strcmp(rows[j].name, rows[j - 1].name) < 0; --j) {
            const Named t = rows[j];
            rows[j] = rows[j - 1];
            rows[j - 1] = t;
          }
        Row& pop = v.add(RB_TEXT, "pop");
        snprintf(pop.value, sizeof(pop.value), "%d/%d", static_cast<int>(s.population), g.maxPopulation());
        for (int i = 0; i < nr; ++i) {
          Row& r = v.add(RB_TEXT, rows[i].name);
          snprintf(r.value, sizeof(r.value), "%d", rows[i].n);
        }
      } else {
        nb--;  // an empty forest has no box
      }
      Block& st = block("stores", false, 1);
      addStores(g, st, false);
      break;
    }
    case M_PATH: {
      Block& a = block(nullptr, true, 1);
      Row& em = a.add(RB_BUTTON, "embark", A_SINGLE | S_EMBARK);
      em.cd = g.cooldownLeft(CD_EMBARK);
      em.steps = 12;
      em.enabled = g.canEmbark();
      Block& o = block("supplies:", true, 1);
      Row& bag = o.add(RB_TEXT, "free");
      double used = 0;
      for (int i = 0; i < kStoreCount; ++i)
        if (s.outfit[i] > 0 && s.outfit[i] <= g.store(static_cast<Item>(i)) + 1e-9)
          used += s.outfit[i] * Game::weight(static_cast<Item>(i));
      snprintf(bag.value, sizeof(bag.value), "%d/%d", static_cast<int>(std::floor(g.capacity() - used)), g.capacity());
      Row& arm = o.add(RB_TEXT, "armour");
      snprintf(arm.value, sizeof(arm.value), "%s", g.armourName());
      Row& wat = o.add(RB_TEXT, "water");
      snprintf(wat.value, sizeof(wat.value), "%d", g.maxWater());
      Item items[kStoreCount];
      int n = 0;
      for (int i = 0; i < kStoreCount; ++i)
        if (Game::carryable(static_cast<Item>(i)) && g.store(static_cast<Item>(i)) > 0) items[n++] = static_cast<Item>(i);
      qsort(items, n, sizeof(Item), byName);
      const double free = g.freeSpace();
      for (int i = 0; i < n; ++i) {
        const Item it = items[i];
        Row& r = o.add(RB_COUNTER, itemLabel(it), static_cast<uint16_t>(A_OUTFIT | static_cast<uint16_t>(it)));
        int taken = s.outfit[it];
        if (taken > g.store(it)) taken = static_cast<int>(g.store(it));
        snprintf(r.value, sizeof(r.value), "%d", taken);
        r.canDec = taken > 0;
        r.canInc = taken < g.store(it) && free >= Game::weight(it) - 1e-9;
      }
      if (s.perks) {
        Block& p = block("perks", false, 1);
        for (int k = 0; k < PK_COUNT; ++k)
          if (g.hasPerk(static_cast<Perk>(k))) {
            Row& r = p.add(RB_TEXT2, kPerks[k].key);
            r.sub = kPerks[k].desc;
          }
      }
      Block& st = block("stores", false, 1);
      addStores(g, st, false);
      Block& wp = block("weapons", false, 1);
      addStores(g, wp, true);
      break;
    }
    case M_FABRICATOR: {
      Block& f = block("fabricate:", true, 2);
      for (int i = 0; i < kFabCount; ++i) {
        if (!g.fabVisible(i)) continue;
        const char* name = itemLabel(kFabs[i].item);
        Row& r = f.add(RB_BUTTON, name, static_cast<uint16_t>(A_FAB | i));
        if (kFabs[i].quantity > 1) {
          snprintf(r.subBuf, sizeof(r.subBuf), "%s (x%d)", name, kFabs[i].quantity);
          r.label = r.subBuf;
        }
        r.enabled = !g.fabAtMax(i);
      }
      if (s.blueprints) {
        Block& bp = block("blueprints", false, 1);
        for (int i = 0; i < kFabCount; ++i)
          if (kFabs[i].blueprint && g.hasBlueprint(kFabs[i].item)) bp.add(RB_TEXT, itemLabel(kFabs[i].item));
      }
      Block& st = block("stores", false, 1);
      addStores(g, st, false);
      Block& wp = block("weapons", false, 1);
      addStores(g, wp, true);
      break;
    }
    case M_SHIP: {
      Block& a = block(nullptr, true, 1);
      a.add(RB_BUTTON, "reinforce hull", A_SINGLE | S_HULL);
      a.add(RB_BUTTON, "upgrade engine", A_SINGLE | S_ENGINE);
      Row& lift = a.add(RB_BUTTON, "lift off", A_SINGLE | S_LIFTOFF);
      lift.cd = g.cooldownLeft(CD_LIFTOFF);
      lift.steps = 12;
      lift.enabled = s.hull > 0;
      Block& sh = block(nullptr, false, 1);
      Row& h = sh.add(RB_TEXT, "hull:");
      snprintf(h.value, sizeof(h.value), "%d", s.hull);
      Row& e = sh.add(RB_TEXT, "engine:");
      snprintf(e.value, sizeof(e.value), "%d", s.thrusters);
      Block& st = block("stores", false, 1);
      addStores(g, st, false);
      break;
    }
    default: break;
  }
  (void)buf;

  // The frame. Tall: the places, then what there is to do and to count (anchored under them, so a
  // button stays where it was), then the log in all the room that leaves — most of the glass at
  // first, when the game is its story, and never less than five lines. Wide: the log beside.
  if (m.portrait) {
    drawHeader(0);
    const int minLog = 5 * 29 + 24;
    const Rect content = {m.margin, m.headerH + 14, m.W - 2 * m.margin, m.H - m.footerH - m.headerH - 14 - minLog};
    const int bottom = layoutBlocks(gBlocks, nb, content);
    const int logTop = bottom + 10;
    _canvas.hline(m.margin, logTop, m.W - 2 * m.margin);
    drawLog({m.margin, logTop + 4, m.W - 2 * m.margin, m.H - m.footerH - logTop - 8}, 40, true);
  } else {
    drawLog({m.margin, 16, 200, m.H - 32}, 30, true);
    _canvas.vline(224, 12, m.H - 24);
    drawHeader(0);
    layoutBlocks(gBlocks, nb, {238, m.headerH + 12, m.W - 238 - m.margin, m.H - m.footerH - m.headerH - 18});
  }
  drawFooter();
}

// ---- the blocks: measured, placed in two columns and pages, drawn -------------------------------

static int unitCount(const App::Block& b) {
  if (b.n == 0) return 0;
  // a block of buttons counts in rows of buttons
  bool buttons = b.rows[0].type == RB_BUTTON;
  return buttons ? (b.n + b.perRow - 1) / b.perRow : b.n;
}

// A description under a name (a perk's), wrapped: how many lines it takes in `w` pixels.
static int subLines(const char* sub, int w) {
  int lines = 0;
  const int len = static_cast<int>(strlen(sub));
  for (int at = 0; at < len && lines < 3; ++lines) {
    int next;
    wrapLine(F_I20, sub + at, len - at, w, next);
    at += next;
  }
  return lines ? lines : 1;
}

static int gSubWidth = 200;  // the narrower column's text width, set by the layout

static int unitHeight(const App::Block& b, int u, const Metrics& m) {
  if (b.rows[0].type == RB_BUTTON) return m.btnH + 7;
  const Row& r = b.rows[u];
  if (r.type == RB_COUNTER) return m.counterH;
  if (r.type == RB_TEXT2) return m.rowH + 23 * (r.sub ? subLines(r.sub, gSubWidth) : 1) + 4;
  return m.rowH;
}

static int legendPad(const App::Block& b) { return b.legend ? 26 : 0; }

int App::layoutBlocks(Block* blocks, int n, Rect area) {
  const Metrics m = metrics(*this);
  const int gap = 14;
  // one column when two would be too narrow for a name, its count and its income side by side
  const bool one = area.w < 480;
  // the actions column the wider, but on touch as wide as the other: counters spill into it, and
  // their -/+ need the room
  const int col0W = one ? area.w : (area.w - gap) * (m.touch ? 50 : 55) / 100;
  const int colX[2] = {area.x, one ? area.x : area.x + col0W + gap};
  const int colW[2] = {col0W, one ? area.w : area.w - col0W - gap};
  gSubWidth = colW[1] - 2 * 10 - 8;
  // buttons two to a row only when every label fits its half
  for (int bi = 0; bi < n; ++bi) {
    Block& b = blocks[bi];
    if (b.n == 0 || b.rows[0].type != RB_BUTTON || b.perRow < 2) continue;
    const int bw = ((colW[0] < colW[1] ? colW[0] : colW[1]) - (b.legend ? 20 : 0) - 8) / 2;  // in either column
    for (int i = 0; i < b.n; ++i)
      if (Font(F_R25).width(b.rows[i].label) + 12 > bw) b.perRow = 1;
  }
  int colY[2] = {area.y, area.y};
  int page = 0;
  gPlacedN = 0;
  auto fits = [&](const Block& b, int from, int col) {
    int h = legendPad(b) + 8, k = from;
    const int units = unitCount(b);
    while (k < units && colY[col] + h + unitHeight(b, k, m) <= area.bottom()) h += unitHeight(b, k++, m);
    return k - from;
  };
  for (int bi = 0; bi < n; ++bi) {
    Block& b = blocks[bi];
    const int units = unitCount(b);
    if (!units) continue;
    const int pref = (b.actions || one) ? 0 : 1;
    int from = 0;
    int guard = 0;
    // What does not fit reads on: from the left column into the right, and from the right onto the
    // next page (never back into the left, which is read first). A block may go whole into the
    // other column when that is emptier and holds all of it.
    while (from < units && guard++ < 32) {
      int col = pref;
      int k = fits(b, from, col);
      if (k < units - from && !one) {
        const int other = 1 - pref;
        const int ko = fits(b, from, other);
        if (from == 0 && ko == units && colY[other] < colY[pref]) {
          col = other;
          k = ko;
        } else if (pref == 0 && (k == 0 || colY[0] >= area.bottom() - m.rowH * 2)) {
          col = 1;  // on into the right column
          k = ko;
        } else if (pref == 1 && from > 0 && colY[1] > area.y) {
          k = 0;  // the rest of a list on the right: the next page (and on it, as much as fits)
        }
      }
      if (k == 0) {  // a new page
        page++;
        colY[0] = colY[1] = area.y;
        continue;
      }
      int h = legendPad(b) + 8;
      for (int u = from; u < from + k; ++u) h += unitHeight(b, u, m);
      if (gPlacedN < kMaxBlocks * 4) gPlaced[gPlacedN++] = {bi, from, from + k, page, col, colY[col], h};
      colY[col] += h + gap;
      from += k;
    }
  }
  _pages = page + 1;
  if (_page >= _pages) _page = _pages - 1;
  // draw this page
  int bottom = area.y;
  for (int pi = 0; pi < gPlacedN; ++pi) {
    const Placed& p = gPlaced[pi];
    if (p.page != _page) continue;
    if (p.y + p.h > bottom) bottom = p.y + p.h;
    Block& b = blocks[p.block];
    const Rect box = {colX[p.col], p.y, colW[p.col], p.h};
    if (b.legend) {
      _canvas.frame({box.x, box.y + 10, box.w, box.h - 10}, 1);
      const int lw = Font(F_R25).width(b.legend);
      _canvas.fill({box.x + 8, box.y, lw + 8, 24}, false);
      _canvas.text(F_R25, box.x + 12, box.y + 18, b.legend);
    }
    const int inner = b.legend ? 10 : 0;
    int y = box.y + legendPad(b) + (b.legend ? 4 : 0);
    const int x = box.x + inner, w = box.w - 2 * inner;
    const bool buttons = b.rows[0].type == RB_BUTTON;
    for (int u = p.from; u < p.to; ++u) {
      if (buttons) {
        const int per = b.perRow;
        const int bw = (w - (per - 1) * 8) / per;
        for (int c = 0; c < per; ++c) {
          const int ri = u * per + c;
          if (ri >= b.n) break;
          Row& r = b.rows[ri];
          drawButton({x + c * (bw + 8), y, bw, m.btnH}, r.label, r.enabled, r.cd, r.id, r.steps);
          if (keyMode() && _focus == r.id) ui::describeFocus(*_game, r.id, _detail, sizeof(_detail));
          else if (!keyMode() && _focus == r.id) ui::describeFocus(*_game, r.id, _detail, sizeof(_detail));
        }
        y += m.btnH + 7;
        continue;
      }
      Row& r = b.rows[u];
      const int h = unitHeight(b, u, m);
      if (r.type == RB_TEXT || r.type == RB_TEXT2) {
        const int base = y + 22;
        _canvas.text(F_R25, x, base, r.label, r.level);
        if (r.value[0]) _canvas.textRight(F_R25, x + w, base, r.value, r.level);
        if (r.type == RB_TEXT && r.sub) {
          // the income, between the name and the count: smaller only when the column is narrow
          const int vw = Font(F_R25).width(r.value);
          const int room = w - vw - 12 - Font(F_R25).width(r.label) - 10;
          const FontId sf = Font(F_I25).width(r.sub) <= room ? F_I25 : F_I20;
          if (Font(sf).width(r.sub) <= room) _canvas.textRight(sf, x + w - vw - 12, base, r.sub);
        }
        if (r.type == RB_TEXT2 && r.sub) {
          const int len = static_cast<int>(strlen(r.sub));
          int ly = base + 24;
          for (int at = 0, lines = 0; at < len && lines < 3; ++lines) {
            int next;
            const int n = wrapLine(F_I20, r.sub + at, len - at, gSubWidth, next);
            _canvas.text(F_I20, x + 8, ly, r.sub + at, n, 12, false);
            at += next;
            ly += 23;
          }
        }
      } else if (r.type == RB_COUNTER) {
        const bool focus = keyMode() && _focus == r.id;
        const bool editing = focus && _editing;
        const Rect row = {x - 2, y, w + 4, h - 2};
        if (focus) {
          _canvas.fill(row, true);
          if (editing) {
            snprintf(_detail, sizeof(_detail), "left, right: fewer, more (held: ten); confirm: done");
          } else {
            char what[120];
            ui::describeFocus(*_game, r.id, what, sizeof(what));
            snprintf(_detail, sizeof(_detail), "confirm: how many; %s", what);
          }
        }
        const int base = y + (h + 14) / 2;
        // the name, in whatever the controls leave (the touch ones are wider)
        _canvas.textFit(F_R25, x + 2, base, r.label, m.touch ? w - 118 - 8 : w - 80, kInk, focus);
        addHot(row, r.id, true, true, true);
        const int group = r.id & 0xFF00, idx = r.id & 0xFF;
        const uint16_t dec = static_cast<uint16_t>((group == A_WORKER ? A_WORKER_DEC : A_OUTFIT_DEC) | idx);
        const uint16_t inc = static_cast<uint16_t>((group == A_WORKER ? A_WORKER_INC : A_OUTFIT_INC) | idx);
        if (m.touch) {
          // [-] n [+] at the right, as big as a finger; a long press on the row gives the detail
          const int bw = 38, vw = 42;
          const Rect plus = {x + w - bw, y + 2, bw, h - 6};
          const Rect val = {plus.x - vw, y, vw, h};
          const Rect minus = {val.x - bw, y + 2, bw, h - 6};
          (r.canDec ? _canvas.frame(minus, 1) : _canvas.dottedFrame(minus));
          (r.canInc ? _canvas.frame(plus, 1) : _canvas.dottedFrame(plus));
          _canvas.hline(minus.x + 12, minus.y + minus.h / 2, bw - 24, 2);
          _canvas.hline(plus.x + 12, plus.y + plus.h / 2, bw - 24, 2);
          _canvas.vline(plus.x + bw / 2 - 1, plus.y + 8, plus.h - 16, 2);
          _canvas.textCentered(F_R25, val.x + vw / 2, base, r.value);
          addHot(minus, dec, r.canDec, false);
          addHot(plus, inc, r.canInc, false);
        } else {
          // ‹ n › — the arrows grey when there is no more to give or take. Being changed (Confirm,
          // then Left and Right), the count stands in a white box on the black row.
          const int right = x + w - 4;
          const int vw = 48, aw = 8;
          const int cy = y + h / 2 - 1;
          const bool ink = !focus || editing;
          if (editing) _canvas.fill({right - 2 * aw - vw - 10, y + 3, 2 * aw + vw + 14, h - 8}, false);
          for (int i = 0; i < aw; ++i) {  // right arrow
            for (int j = -i; j <= i; ++j)
              if (r.canInc || ((i + j) & 1)) _canvas.pixel(right - i, cy + j, ink);
          }
          _canvas.textCentered(F_R25, right - aw - 3 - vw / 2, base, r.value, kInk, !ink);
          const int lx = right - aw - 6 - vw;
          for (int i = 0; i < aw; ++i)
            for (int j = -i; j <= i; ++j)
              if (r.canDec || ((i + j) & 1)) _canvas.pixel(lx - aw + 1 + i, cy + j, ink);
        }
      }
      y += h;
    }
  }
  return bottom;
}

}  // namespace adr
