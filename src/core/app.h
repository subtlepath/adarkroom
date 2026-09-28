// app.h — A Dark Room on e-paper: the screens, the keys and the touch, the settings, the saves,
// and when the glass should refresh.
//
// The firmware (main.cpp) and the host driver (test/host) feed it the same events and read the
// same framebuffer: keys (the X3's and the X4 Classic's seven), touches and swipes (the X4 Pro's),
// and time. The app draws only when asked (render), and says how the glass should show it.
#pragma once

#include <cstdint>

#include "canvas.h"
#include "device.h"
#include "game.h"
#include "platform.h"

namespace adr {

struct Input {
  enum Type : uint8_t { Key, Tap, Hold, Swipe };
  enum KeyCode : uint8_t { Up, Down, Left, Right, Confirm, Back, Home };
  Type type = Key;
  KeyCode key = Confirm;
  bool longPress = false;
  int x = 0, y = 0, x2 = 0, y2 = 0;  // page coordinates
  static Input keyPress(KeyCode k, bool held = false) {
    Input e;
    e.type = Key;
    e.key = k;
    e.longPress = held;
    return e;
  }
  static Input tap(int x, int y) {
    Input e;
    e.type = Tap;
    e.x = x;
    e.y = y;
    return e;
  }
  static Input hold(int x, int y) {
    Input e = tap(x, y);
    e.type = Hold;
    return e;
  }
  static Input swipe(int x0, int y0, int x1, int y1) {
    Input e;
    e.type = Swipe;
    e.x = x0;
    e.y = y0;
    e.x2 = x1;
    e.y2 = y1;
    return e;
  }
};

enum class Refresh : uint8_t { None, Fast, Half, Full };

// The device's own settings (kept apart from the game's save).
struct DeviceSettings {
  uint8_t version = 1;
  uint8_t rotation = 1;       // Rotation: R90, portrait, by default
  uint8_t sleepMinutes = 15;  // 0: never
  uint8_t tempo = 1;          // combat: 0 brisk (the original's), 1 measured (x1.5), 2 slow (x2)
  uint8_t light = 0;          // the frontlight: 0 off, 1 as bright as the fire, 2 dim, 3 half, 4 full
};

// What can be pressed, and where: filled in as a screen is drawn.
struct Hot {
  Rect r;
  uint16_t id;
  bool focusable;  // keys can reach it; touch-only parts (a counter's +/-) are not
  bool enabled;
  bool counter;    // Left and Right adjust it
};
constexpr int kMaxHot = 96;

class App {
 public:
  void begin(Game& game, Platform& platform, uint8_t* framebuffer);
  // A new game, or the save kept on the device.
  bool loadOrStart();
  void save();  // now (sleep, a big moment); otherwise the app saves itself now and then

  // Input. Returns true when the screen should be drawn again.
  bool handle(const Input& e);
  // The keys held down right now (for the ascent): -1/0/+1 on each axis.
  void held(int dx, int dy);
  // A finger held on the glass (the X4 Pro): in the ascent, the ship steers toward it.
  void touchSteer(bool down, int x, int y);
  // Time passing: real milliseconds since the last call. Paused behind the menu.
  void tick(uint32_t ms);
  // Whether a new frame is due (something changed, or a cooldown moved a step on).
  bool wantsFrame() const;
  // Draw into the framebuffer. Returns how the glass should show it (None: nothing new).
  Refresh render();
  // The screen to leave on the glass while asleep.
  void renderSleep(bool lowBattery);
  // Seconds without a key or a touch before the device may sleep (0: never).
  uint32_t idleLimitMs() const;
  uint32_t idleMs() const { return _idleMs; }
  bool sleepAllowed() const;  // not in the middle of a fight or the ascent

  void setFramebuffer(uint8_t* fb) { _canvas.retarget(fb); }
  Canvas& canvas() { return _canvas; }
  const DeviceSettings& settings() const { return _settings; }
  Game& game() { return *_game; }
  const DeviceProfile& profile() const { return *_profile; }
  // A location's blocks (ui_location.cpp).
  struct Block;
  // A line for the serial log and the tests: which screen, and what has the focus.
  void describe(char* out, int cap) const;

 private:
  // screens
  void drawLocation();
  void drawHeader(int y);
  int drawLog(Rect area, int lines, bool fromTop);
  void drawWorld();
  void drawOverview();
  void drawEvent();
  void drawCombat(int y);
  void drawSpace();
  void drawEnding();
  void drawDead();
  void drawMenu();
  void drawDialog();
  void drawDropMenu();
  void drawFooter();
  void drawKeyHints();  // the sleep screen's arrows to the device's keys
  // A caption in w from x: the body's italic when it fits, else a size smaller. Returns the pen.
  int caption(int x, int base, int w, const char* s, int level = kInk, bool centred = false);
  void drawButton(Rect r, const char* label, bool enabled, float cooldown, uint16_t id, int cdSteps = 0);

  // Returns where the page's blocks end.
  int layoutBlocks(struct Block* blocks, int n, Rect area);

  // input
  void addHot(Rect r, uint16_t id, bool enabled, bool focusable = true, bool counter = false);
  const Hot* focused() const;
  void moveFocus(int delta);
  void activate(uint16_t id, bool longPress = false);
  void adjust(uint16_t id, int delta);
  bool handleKey(const Input& e);
  bool handleTouch(const Input& e);
  void travel(int delta);
  void openMenu();
  void openOverlay(uint8_t o);  // saves the focus under it
  void closeOverlay();           // gives it back
  void menuChoose(int i);
  void applySettings();
  void applyLight();
  int menuRows() const;  // the menu's rows on this device (no light, no light row)
  int menuItem(int row) const;
  int8_t _lightShown = -1;
  void loadSettings();
  void saveSettings();
  bool keyMode() const { return !_profile->touch; }

  Game* _game = nullptr;
  Platform* _platform = nullptr;
  const DeviceProfile* _profile = nullptr;
  Canvas _canvas;
  DeviceSettings _settings;

  Hot _hot[kMaxHot];
  int _hotN = 0;
  uint16_t _focus = 0;     // the focused hot spot's id
  int _focusIndex = 0;     // where it was, for when it goes
  int _page = 0, _pages = 1;
  bool _editing = false;   // the focused counter is being changed: Left and Right give and take
  char _detail[160] = {};  // the footer's line: the focused thing's cost or use

  enum class Overlay : uint8_t { None, Menu, Dialog, Drop, Overview };
  Overlay _overlay = Overlay::None;
  int _menuFocus = 0;
  enum class Dialog : uint8_t { None, Restart, Hyper, LiftOff };
  Dialog _dialog = Dialog::None;
  int _dropRow = -1;
  uint16_t _focusUnder = 0;  // the focus before an overlay opened
  int _focusIndexUnder = 0;

  // refresh bookkeeping
  uint32_t _sinceFrame = 0;
  uint32_t _idleMs = 0;
  uint32_t _sinceSave = 0;
  uint32_t _lastCrc = 0;
  uint32_t _screenKey = 0;  // which screen was last drawn: a new one wants a cleaner refresh
  int _fastCount = 0;
  bool _dirty = true;
  bool _gameDirty = true;
  uint32_t _logSerialSeen = 0;
  int8_t _heldX = 0, _heldY = 0;
  Rect _shipRect;  // where the ship was drawn
};

// The combat tempo (game_events.cpp).
void setCombatTempo(double t);
double combatTempo();

}  // namespace adr
