// app.cpp — the app's frame: input, focus, the menu and its settings, time, saves, refresh.
// The screens themselves are drawn in ui_*.cpp.
#include "app.h"

#include <cstdio>
#include <cstring>

#include "save.h"
#include "ui.h"

namespace adr {

namespace {

uint32_t crc32(const uint8_t* p, int n) {
  uint32_t c = 0xFFFFFFFFu;
  for (int i = 0; i < n; ++i) {
    c ^= p[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

const uint8_t kSleepChoices[] = {5, 10, 15, 30, 60, 0};
const char* const kTempoNames[] = {"brisk", "measured", "slow"};
const double kTempo[] = {1.0, 1.5, 2.0};

}  // namespace

// ---- bring-up, saves and settings ---------------------------------------------------------------

void App::begin(Game& game, Platform& platform, uint8_t* fb) {
  _game = &game;
  _platform = &platform;
  _profile = &platform.profile();
  loadSettings();
  _canvas.attach(fb, _profile->panelW, _profile->panelH, static_cast<Rotation>(_settings.rotation));
  applySettings();
}

namespace {
uint8_t gSaveBuf[kMaxSave];  // one buffer for saving and loading
}

bool App::loadOrStart() {
  uint8_t* buf = gSaveBuf;
  const size_t n = _platform->load("game", buf, kMaxSave);
  const uint64_t seed = (static_cast<uint64_t>(_platform->random()) << 32) | _platform->random();
  if (n && loadGame(*_game, buf, n)) {
    _game->resume(seed);
    _logSerialSeen = _game->st().logSerial;
    applyLight();
    return true;
  }
  // Nothing kept, or a save this firmware cannot read: a new game (with any prestige it kept).
  if (n) loadPrestige(*_game, buf, n);
  _game->newGame(seed);
  save();
  applyLight();
  return false;
}

void App::save() {
  uint8_t* buf = gSaveBuf;
  const size_t n = saveGame(*_game, buf, kMaxSave);
  if (n) _platform->save("game", buf, n);
  _sinceSave = 0;
}

void App::loadSettings() {
  DeviceSettings s;
  if (_platform->load("cfg", reinterpret_cast<uint8_t*>(&s), sizeof(s)) == sizeof(s) && s.version == 1) _settings = s;
  if (_settings.rotation > 3) _settings.rotation = 1;
  if (_settings.tempo > 2) _settings.tempo = 1;
  if (_settings.light > 4) _settings.light = 0;
}

void App::saveSettings() { _platform->save("cfg", reinterpret_cast<const uint8_t*>(&_settings), sizeof(_settings)); }

void App::applySettings() {
  _canvas.setRotation(static_cast<Rotation>(_settings.rotation));
  setCombatTempo(kTempo[_settings.tempo]);
  _dirty = true;
}

// The X4 Pro's frontlight, warm: off, as bright as the room's fire (dead to roaring), or fixed.
void App::applyLight() {
  if (!_profile->light) return;
  static const uint8_t kFire[] = {0, 8, 16, 26, 38};
  static const uint8_t kFixed[] = {0, 0, 15, 45, 100};
  const int fire = _game->st().fire;
  const int pct = _settings.light == 1 ? kFire[fire < 0 ? 0 : fire > 4 ? 4 : fire] : kFixed[_settings.light % 5];
  if (pct == _lightShown) return;
  _lightShown = static_cast<int8_t>(pct);
  _platform->setLight(static_cast<uint8_t>(pct));
}

int App::menuRows() const { return _profile->light ? ui::kMenuItems : ui::kMenuItems - 1; }
int App::menuItem(int row) const { return (!_profile->light && row >= ui::M_LIGHT) ? row + 1 : row; }

uint32_t App::idleLimitMs() const { return _settings.sleepMinutes * 60u * 1000u; }

bool App::sleepAllowed() const {
  const GameState& s = _game->st();
  return !s.space.active && !(s.ev.event >= 0 && s.ev.combat && !s.ev.fought);
}

// ---- time ---------------------------------------------------------------------------------------

void App::held(int dx, int dy) {
  _heldX = static_cast<int8_t>(dx);
  _heldY = static_cast<int8_t>(dy);
}

void App::touchSteer(bool down, int x, int y) {
  if (!_game->st().space.active) return;
  if (!down) {
    held(0, 0);
    return;
  }
  const int cx = _shipRect.x + _shipRect.w / 2, cy = _shipRect.y + _shipRect.h / 2, d = _shipRect.w;
  held(x > cx + d ? 1 : x < cx - d ? -1 : 0, y > cy + d ? 1 : y < cy - d ? -1 : 0);
}

void App::tick(uint32_t ms) {
  _idleMs += ms;
  _sinceFrame += ms;
  _sinceSave += ms;
  const bool paused = _overlay == Overlay::Menu || _overlay == Overlay::Dialog;
  if (!paused) {
    _game->spaceSteer(_heldX, _heldY);
    _game->advance(ms);
  }
  if (_game->takeChanged()) {
    _gameDirty = true;
    applyLight();
  }
  // A save every minute or so of play; the big moments save themselves (see handle()).
  if (_sinceSave > 60000 && !_game->st().space.active) save();
}

bool App::wantsFrame() const {
  if (_dirty) return true;
  const GameState& s = _game->st();
  const uint32_t minGap = s.space.active ? 0 : (s.ev.event >= 0 && s.ev.combat) ? 250 : 400;
  if (_gameDirty && _sinceFrame >= minGap) return true;
  // a cooldown moving on: drawn again now and then, refreshed only when its bar moves a step
  return _sinceFrame >= 1000 && ui::anyCooldown(*_game);
}

// ---- drawing ------------------------------------------------------------------------------------

Refresh App::render() {
  const GameState& s = _game->st();
  _hotN = 0;
  _detail[0] = 0;
  _pages = 1;  // a place's blocks say how many pages they take; nothing else has more than one
  _canvas.clear();
  if (s.ending) drawEnding();
  else if (s.space.active) drawSpace();
  else if (s.trip.dead && s.active == M_WORLD) drawDead();
  else if (s.ev.event >= 0) drawEvent();
  else if (s.active == M_WORLD) (_overlay == Overlay::Overview ? drawOverview() : drawWorld());
  else drawLocation();
  // an overlay owns the input: what is under it cannot be pressed
  if (_overlay == Overlay::Menu || _overlay == Overlay::Dialog || _overlay == Overlay::Drop) _hotN = 0;
  if (_overlay == Overlay::Menu) drawMenu();
  else if (_overlay == Overlay::Dialog) drawDialog();
  else if (_overlay == Overlay::Drop) drawDropMenu();
  // the focus must be somewhere it can be seen
  if (keyMode() && !focused()) {
    for (int i = 0; i < _hotN; ++i) {
      if (!_hot[i].focusable) continue;
      if (i >= _focusIndex || i == _hotN - 1) {
        _focus = _hot[i].id;
        break;
      }
    }
    // drawn again with the focus in place
    if (focused()) return render();
  }
  if (s.settings.lightsOff) {
    uint8_t* fb = _canvas.buffer();
    for (int i = 0; i < _canvas.bytes(); ++i) fb[i] = static_cast<uint8_t>(~fb[i]);
  }
  _dirty = false;
  _gameDirty = false;
  _sinceFrame = 0;
  const uint32_t crc = crc32(_canvas.buffer(), _canvas.bytes());
  if (crc == _lastCrc) return Refresh::None;
  _lastCrc = crc;
  // a new screen gets a cleaner refresh, as do enough fast ones in a row
  uint32_t key = static_cast<uint32_t>(_overlay) | s.active << 4 | (s.ev.event + 1) << 8 | s.ev.scene << 16 |
                 (s.space.active ? 1u : 0u) << 24 | (s.ending ? 1u : 0u) << 25 | (_page & 7u) << 26 |
                 (s.ev.lootShown ? 1u : 0u) << 29;
  Refresh r = Refresh::Fast;
  const bool action = s.space.active || (s.ev.event >= 0 && s.ev.combat && !s.ev.fought);
  if (key != _screenKey && !action) r = Refresh::Half;
  _screenKey = key;
  if (r == Refresh::Fast && ++_fastCount >= (action ? _profile->cleanEvery * 3 : _profile->cleanEvery)) r = Refresh::Half;
  if (r != Refresh::Fast) _fastCount = 0;
  return r;
}

void App::addHot(Rect r, uint16_t id, bool enabled, bool focusable, bool counter) {
  if (_hotN >= kMaxHot) return;
  _hot[_hotN++] = {r, id, focusable, enabled, counter};
}

const Hot* App::focused() const {
  for (int i = 0; i < _hotN; ++i)
    if (_hot[i].focusable && _hot[i].id == _focus) return &_hot[i];
  return nullptr;
}

void App::describe(char* out, int cap) const {
  const GameState& s = _game->st();
  const char* where = s.ending              ? "ending"
                      : s.space.active      ? "space"
                      : s.ev.event >= 0     ? "event"
                      : s.active == M_WORLD ? "world"
                                            : _game->locationTitle(_game->activeModule());
  int n = snprintf(out, cap, "%s", where);
  if (s.ev.event >= 0 && n < cap)
    n += snprintf(out + n, cap - n, " %s/%s%s", kEvents[s.ev.event].key, _game->scene()->key,
                  s.ev.combat ? (s.ev.lootShown ? " loot" : " fight") : "");
  if (s.active == M_WORLD && s.trip.active && n < cap) n += snprintf(out + n, cap - n, " at %d,%d", s.trip.x, s.trip.y);
  if (s.space.active && n < cap) n += snprintf(out + n, cap - n, " alt %d hull %d", _game->altitude(), s.space.hull);
  if (_overlay != Overlay::None && n < cap) n += snprintf(out + n, cap - n, " overlay %d", static_cast<int>(_overlay));
  if (_pages > 1 && n < cap) n += snprintf(out + n, cap - n, " page %d/%d", _page + 1, _pages);
  if (focused() && n < cap) snprintf(out + n, cap - n, " focus %04x", focused()->id);
}

// ---- input --------------------------------------------------------------------------------------

bool App::handle(const Input& e) {
  _idleMs = 0;
  _dirty = true;
  const bool r = (e.type == Input::Key) ? handleKey(e) : handleTouch(e);
  if (_game->takeChanged()) _gameDirty = true;
  return r;
}

void App::moveFocus(int delta) {
  int idx = -1;
  for (int i = 0; i < _hotN; ++i)
    if (_hot[i].focusable && _hot[i].id == _focus) idx = i;
  int i = idx;
  for (int step = 0; step < _hotN; ++step) {
    i += delta;
    if (i < 0 || i >= _hotN) {
      // past the end of the page: the next one, or the last one
      if (_pages > 1 && ((delta > 0 && _page + 1 < _pages) || (delta < 0 && _page > 0))) {
        _page += delta > 0 ? 1 : -1;
        _focus = 0;
        _focusIndex = delta > 0 ? 0 : kMaxHot;
        return;
      }
      return;
    }
    if (_hot[i].focusable) {
      _focus = _hot[i].id;
      _focusIndex = i;
      return;
    }
  }
}

void App::travel(int delta) {
  Game& g = *_game;
  static const Module kOrder[] = {M_ROOM, M_OUTSIDE, M_PATH, M_FABRICATOR, M_SHIP};
  const Module cur = g.activeModule();
  int at = 0;
  for (int i = 0; i < 5; ++i)
    if (kOrder[i] == cur) at = i;
  for (int i = at + delta; i >= 0 && i < 5; i += delta) {
    if (g.unlocked(kOrder[i])) {
      g.travelTo(kOrder[i]);
      _page = 0;
      _focus = 0;
      _focusIndex = 0;
      _editing = false;
      return;
    }
  }
}

bool App::handleKey(const Input& e) {
  Game& g = *_game;
  GameState& s = g.st();
  const Input::KeyCode k = e.key;
  // the menu and the dialogs, whatever is under them
  if (_overlay == Overlay::Menu) {
    if (k == Input::Back || k == Input::Home) closeOverlay();
    else if (k == Input::Up) _menuFocus = _menuFocus > 0 ? _menuFocus - 1 : menuRows() - 1;
    else if (k == Input::Down) _menuFocus = (_menuFocus + 1) % menuRows();
    else if (k == Input::Confirm || k == Input::Right) menuChoose(menuItem(_menuFocus));
    else if (k == Input::Left) menuChoose(menuItem(_menuFocus) | 0x100);
    return true;
  }
  if (_overlay == Overlay::Dialog || _overlay == Overlay::Drop) {
    if (k == Input::Back || k == Input::Home) {
      closeOverlay();
      return true;
    }
    if (k == Input::Up || k == Input::Left) moveFocus(-1);
    else if (k == Input::Down || k == Input::Right) moveFocus(1);
    else if (k == Input::Confirm && focused()) activate(focused()->id);
    return true;
  }
  if (s.ending) {
    if (k == Input::Confirm && focused()) activate(focused()->id);
    else if (k == Input::Back || k == Input::Home) openMenu();
    return true;
  }
  if (k == Input::Back || k == Input::Home) {
    if (_overlay == Overlay::Overview) _overlay = Overlay::None;
    else if (_editing) _editing = false;
    else openMenu();
    return true;
  }
  if (s.space.active) {
    if (k == Input::Left) g.spaceNudge(-1, 0);
    else if (k == Input::Right) g.spaceNudge(1, 0);
    else if (k == Input::Up) g.spaceNudge(0, -1);
    else if (k == Input::Down) g.spaceNudge(0, 1);
    return true;
  }
  if (s.active == M_WORLD && s.trip.dead) return false;
  if (s.ev.event >= 0) {
    const Hot* f = focused();
    if (f && (f->id & 0xFF00) == ui::A_LOOT && (k == Input::Right || (k == Input::Confirm && e.longPress))) {
      activate(static_cast<uint16_t>(ui::A_LOOT_ALL | (f->id & 0xFF)));
    } else if (k == Input::Up || k == Input::Left) {
      moveFocus(-1);
    } else if (k == Input::Down || k == Input::Right) {
      moveFocus(1);
    } else if (k == Input::Confirm && f) {
      activate(f->id, e.longPress);
    }
    return true;
  }
  if (s.active == M_WORLD) {
    if (_profile->touch && (k == Input::Up || k == Input::Down)) {
      _overlay = _overlay == Overlay::Overview ? Overlay::None : Overlay::Overview;
      return true;
    }
    switch (k) {
      case Input::Up: g.move(DIR_NORTH); break;
      case Input::Down: g.move(DIR_SOUTH); break;
      case Input::Left: g.move(DIR_WEST); break;
      case Input::Right: g.move(DIR_EAST); break;
      case Input::Confirm: _overlay = _overlay == Overlay::Overview ? Overlay::None : Overlay::Overview; break;
      default: break;
    }
    if (s.ev.event >= 0 || s.active != M_WORLD) {
      _overlay = Overlay::None;
      _focus = 0;
      _focusIndex = 0;
    }
    return true;
  }
  // a location: the side keys go to the last place and the next (held, they go no further), Left
  // and Right go through what there is to do here
  if (k == Input::Up || k == Input::Down) {
    if (!e.longPress) travel(k == Input::Up ? -1 : 1);
    return true;
  }
  if (_profile->touch) return true;
  const Hot* f = focused();
  if (!f || !f->counter) _editing = false;
  switch (k) {
    case Input::Left:
    case Input::Right: {
      const int dir = k == Input::Left ? -1 : 1;
      if (_editing) adjust(f->id, dir * (e.longPress ? 10 : 1));
      else moveFocus(dir);
      break;
    }
    case Input::Confirm:
      // a worker or a supply: Confirm to change it with Left and Right, Confirm again when done
      if (f && f->counter) _editing = !_editing;
      else if (f) activate(f->id, e.longPress);
      break;
    default: break;
  }
  return true;
}

bool App::handleTouch(const Input& e) {
  Game& g = *_game;
  GameState& s = g.st();
  if (e.type == Input::Swipe) {
    const int dx = e.x2 - e.x, dy = e.y2 - e.y;
    const bool horizontal = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy);
    if (_overlay != Overlay::None && _overlay != Overlay::Overview) return false;
    if (s.space.active) {
      if (horizontal) g.spaceNudge(dx < 0 ? -1 : 1, 0);
      else g.spaceNudge(0, dy < 0 ? -1 : 1);
    } else if (s.active == M_WORLD && s.ev.event < 0 && !s.trip.dead) {
      if (horizontal) g.move(dx < 0 ? DIR_WEST : DIR_EAST);
      else g.move(dy < 0 ? DIR_NORTH : DIR_SOUTH);
    } else if (s.ev.event < 0 && !s.ending) {
      if (horizontal) travel(dx < 0 ? 1 : -1);
      else if (_pages > 1) {
        _page += dy < 0 ? 1 : -1;
        if (_page < 0) _page = 0;
        if (_page >= _pages) _page = _pages - 1;
      }
    }
    return true;
  }
  // a tap or a long press on something drawn: the small touch-only targets first
  const Hot* hit = nullptr;
  for (int pass = 0; pass < 2 && !hit; ++pass)
    for (int i = 0; i < _hotN; ++i) {
      const Hot& h = _hot[i];
      if (h.focusable == (pass == 1) && h.r.inset(-4).contains(e.x, e.y)) {
        hit = &h;
        break;
      }
    }
  if (hit) {
    if (hit->focusable) {
      _focus = hit->id;
      _focusIndex = static_cast<int>(hit - _hot);
    }
    if (e.type == Input::Hold) {
      // a long press shows what a thing costs or does; on a loot row it takes all of it
      if ((hit->id & 0xFF00) == ui::A_LOOT) activate(static_cast<uint16_t>(ui::A_LOOT_ALL | (hit->id & 0xFF)));
      return true;
    }
    if (!hit->counter) activate(hit->id);
    return true;
  }
  if (_overlay == Overlay::Menu || _overlay == Overlay::Dialog || _overlay == Overlay::Drop) {
    closeOverlay();  // a tap outside closes it
    return true;
  }
  // the map: a tap steps toward it, as the original's click did
  if (s.active == M_WORLD && !s.space.active && s.ev.event < 0 && !s.trip.dead && e.type == Input::Tap) {
    const Point c = ui::mapCentre(*this);
    const int cx = e.x - c.x, cy = e.y - c.y;
    if (cx > cy && cx < -cy) g.move(DIR_NORTH);
    else if (cx < cy && cx > -cy) g.move(DIR_SOUTH);
    else if (cx < cy && cx < -cy) g.move(DIR_WEST);
    else if (cx > cy && cx > -cy) g.move(DIR_EAST);
    if (s.ev.event >= 0) _overlay = Overlay::None;
    return true;
  }
  return false;
}

void App::adjust(uint16_t id, int delta) {
  Game& g = *_game;
  const int group = id & 0xFF00, idx = id & 0xFF;
  if (group == ui::A_WORKER) g.adjustWorker(static_cast<Worker>(idx), delta);
  else if (group == ui::A_OUTFIT) g.adjustOutfit(static_cast<Item>(idx), delta);
}

void App::activate(uint16_t id, bool longPress) {
  Game& g = *_game;
  GameState& s = g.st();
  const int group = id & 0xFF00, idx = id & 0xFF;
  switch (group) {
    case ui::A_TAB: g.travelTo(static_cast<Module>(idx)); _page = 0; _editing = false; break;
    case ui::A_SINGLE:
      switch (idx) {
        case ui::S_FIRE: s.fire == 0 ? g.lightFire() : g.stokeFire(); break;
        case ui::S_GATHER: g.gatherWood(); break;
        case ui::S_TRAPS: g.checkTraps(); break;
        case ui::S_EMBARK:
          g.embark();
          if (s.active == M_WORLD) {
            _focus = 0;
            save();
          }
          break;
        case ui::S_HULL: g.reinforceHull(); break;
        case ui::S_ENGINE: g.upgradeEngine(); break;
        case ui::S_LIFTOFF:
          if (s.hull > 0 && !g.onCooldown(CD_LIFTOFF)) {
            if (!s.seenWarning) {
              openOverlay(static_cast<uint8_t>(Overlay::Dialog));
              _dialog = Dialog::LiftOff;
            } else {
              save();
              g.liftOff();
            }
          }
          break;
        case ui::S_TAKE_EVERYTHING: g.takeEverything(); break;
        case ui::S_OVERVIEW: _overlay = _overlay == Overlay::Overview ? Overlay::None : Overlay::Overview; break;
        case ui::S_MENU: openMenu(); break;
        case ui::S_RESTART:
          openOverlay(static_cast<uint8_t>(Overlay::Dialog));
          _dialog = Dialog::Restart;
          break;
        case ui::S_PAGE_NEXT: if (_page + 1 < _pages) _page++; break;
        case ui::S_PAGE_PREV: if (_page > 0) _page--; break;
        default: break;
      }
      break;
    case ui::A_BUILD: g.build(idx); break;
    case ui::A_BUY: g.buy(idx); break;
    case ui::A_FAB: g.fabricate(idx); break;
    case ui::A_WORKER: g.adjustWorker(static_cast<Worker>(idx), longPress ? 10 : 1); break;
    case ui::A_WORKER_DEC10: g.adjustWorker(static_cast<Worker>(idx), -10); break;
    case ui::A_WORKER_DEC: g.adjustWorker(static_cast<Worker>(idx), -1); break;
    case ui::A_WORKER_INC: g.adjustWorker(static_cast<Worker>(idx), 1); break;
    case ui::A_WORKER_INC10: g.adjustWorker(static_cast<Worker>(idx), 10); break;
    case ui::A_OUTFIT: g.adjustOutfit(static_cast<Item>(idx), longPress ? 10 : 1); break;
    case ui::A_OUTFIT_DEC10: g.adjustOutfit(static_cast<Item>(idx), -10); break;
    case ui::A_OUTFIT_DEC: g.adjustOutfit(static_cast<Item>(idx), -1); break;
    case ui::A_OUTFIT_INC: g.adjustOutfit(static_cast<Item>(idx), 1); break;
    case ui::A_OUTFIT_INC10: g.adjustOutfit(static_cast<Item>(idx), 10); break;
    case ui::A_EVBTN: {
      const int before = s.ev.event;
      const uint8_t scene = s.ev.scene;
      g.chooseButton(idx);
      if (s.ev.event != before || s.ev.scene != scene) {
        _focus = 0;
        _focusIndex = 0;
      }
      if (s.ev.event < 0) save();
      break;
    }
    case ui::A_COMBAT:
      g.combatButton(idx);
      if (s.ev.event < 0 || !s.ev.combat) {
        _focus = 0;
        _focusIndex = 0;
      }
      break;
    case ui::A_LOOT: {
      Game::DropOption opts[kStoreCount];
      if (g.dropOptions(idx, opts) > 0) {  // no room: what to leave behind
        openOverlay(static_cast<uint8_t>(Overlay::Drop));
        _dropRow = idx;
      } else {
        g.takeLoot(idx, false);
      }
      break;
    }
    case ui::A_LOOT_ALL: g.takeLoot(idx, true); break;
    case ui::A_DROP: {
      Game::DropOption opts[kStoreCount];
      const int n = g.dropOptions(_dropRow, opts);
      if (idx < n) g.dropAndTake(_dropRow, opts[idx].item, opts[idx].n);
      closeOverlay();
      break;
    }
    case ui::A_DIALOG: {
      const Dialog d = _dialog;
      closeOverlay();
      if (idx == 0) {
        if (d == Dialog::Restart) {
          g.restart();
          _page = 0;
          save();
        } else if (d == Dialog::Hyper) {
          g.setHyper(!s.settings.hyper);
        } else if (d == Dialog::LiftOff) {
          save();
          g.liftOff();
        }
      }
      break;
    }
    case ui::A_MENU: menuChoose(idx); break;
    case ui::A_ENDING: g.restart(); _page = 0; save(); break;
    default: break;
  }
  (void)longPress;
}

// ---- the menu -----------------------------------------------------------------------------------

void App::openOverlay(uint8_t o) {
  if (_overlay == Overlay::None || _overlay == Overlay::Overview) {
    _focusUnder = _focus;
    _focusIndexUnder = _focusIndex;
  }
  _overlay = static_cast<Overlay>(o);
  _focus = 0;
  _focusIndex = 0;
  _dirty = true;
}

void App::closeOverlay() {
  _overlay = Overlay::None;
  _dialog = Dialog::None;
  _focus = _focusUnder;
  _focusIndex = _focusIndexUnder;
  _dirty = true;
}

void App::openMenu() {
  openOverlay(static_cast<uint8_t>(Overlay::Menu));
  _menuFocus = 0;
  save();  // the menu pauses the world: a good moment
}

// i: the row; with 0x100, stepped backwards (Left)
void App::menuChoose(int i) {
  const bool back = i & 0x100;
  i &= 0xFF;
  Game& g = *_game;
  GameState& s = g.st();
  switch (i) {
    case ui::M_RESUME: closeOverlay(); break;
    case ui::M_LIGHTS: s.settings.lightsOff = !s.settings.lightsOff; break;
    case ui::M_HYPER:
      if (!s.settings.hyper) {
        openOverlay(static_cast<uint8_t>(Overlay::Dialog));
        _dialog = Dialog::Hyper;
      } else {
        g.setHyper(false);
      }
      break;
    case ui::M_TEMPO:
      _settings.tempo = static_cast<uint8_t>((_settings.tempo + (back ? 2 : 1)) % 3);
      applySettings();
      saveSettings();
      break;
    case ui::M_SLEEP: {
      int at = 2;
      for (int k = 0; k < 6; ++k)
        if (kSleepChoices[k] == _settings.sleepMinutes) at = k;
      at = (at + (back ? 5 : 1)) % 6;
      _settings.sleepMinutes = kSleepChoices[at];
      saveSettings();
      break;
    }
    case ui::M_TURN:
      _settings.rotation = _settings.rotation == 1 ? 0 : 1;
      applySettings();
      saveSettings();
      break;
    case ui::M_LIGHT:
      _settings.light = static_cast<uint8_t>((_settings.light + (back ? 4 : 1)) % 5);
      applyLight();
      saveSettings();
      break;
    case ui::M_RESTART:
      openOverlay(static_cast<uint8_t>(Overlay::Dialog));
      _dialog = Dialog::Restart;
      break;
    default: break;
  }
  _dirty = true;
}

const char* ui::tempoName(int t) { return kTempoNames[t % 3]; }

}  // namespace adr
