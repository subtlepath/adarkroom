// A Dark Room — on the Xteink X4 Pro, the X4 Classic and the X3.
//
// Boot: the board's rails, which device this is and its panel's controller (asked, not assumed),
// input listening before the panel is up, then the save, and the game where it was left. The
// loop turns keys, touches and the Home key into the app's events, lets the game's time run,
// refreshes the glass as the app asks, and sleeps (the game paused, the save kept) when Power is
// pressed or the device has been left alone.
#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <XteinkDetect.h>
#include <esp_sleep.h>
#ifndef ADR_SIM
#include <RecoveryBoot.h>
#endif

#include "core/app.h"
#include "hal/device.h"

namespace {

EInkDisplay display(BoardConfig::DEFAULT_DEVICE.display.sclk, BoardConfig::DEFAULT_DEVICE.display.mosi,
                    BoardConfig::DEFAULT_DEVICE.display.cs, BoardConfig::DEFAULT_DEVICE.display.dc,
                    BoardConfig::DEFAULT_DEVICE.display.rst, BoardConfig::DEFAULT_DEVICE.display.busy);
InputManager input;
adr::DevicePlatform platform;
adr::Game game;
adr::App app;

uint32_t lastTick = 0;
uint32_t lastBatteryCheck = 0;
uint32_t logSeen = 0;  // the game's log lines already echoed on serial
uint32_t readyAt = 0;  // when setup finished: the waking press's echoes are ignored for a moment

// The seven keys: a press is delivered at once; the arrows repeat while held, and Confirm held
// half a second gives one long press (take all of it, ten at once).
constexpr uint32_t kLongMs = 500, kRepeatMs = 220;
int heldKey = -1;
uint32_t heldSince = 0, lastRepeat = 0;
bool longSent = false;

// Touch: a long press, and a finger held down (the ascent steers toward it).
bool touchDown = false, holdFired = false;
uint32_t touchSince = 0;
float touchX = 0, touchY = 0;
uint32_t lastHomeMs = 0;

// The game's story, echoed on serial as it is told (the tests wait on it).
void echoLog() {
  const adr::GameState& s = game.st();
  if (s.logSerial == logSeen) return;
  const uint32_t fresh = s.logSerial - logSeen;
  for (int i = fresh < s.logN ? s.logN - static_cast<int>(fresh) : 0; i < s.logN; ++i) Serial.printf("[adr] log: %s\n", s.log[i]);
  logSeen = s.logSerial;
}

void logScene(const char* why) {
  char d[200];
  app.describe(d, sizeof(d));
  Serial.printf("[adr] %s: %s\n", why, d);
}

// The driver may hand back another framebuffer after a refresh: draw into whichever it holds now.
void retarget() { app.setFramebuffer(display.getFrameBuffer()); }

void show(adr::Refresh r) {
  if (r == adr::Refresh::None) return;
  const EInkDisplay::RefreshMode mode = r == adr::Refresh::Full   ? EInkDisplay::FULL_REFRESH
                                        : r == adr::Refresh::Half ? EInkDisplay::HALF_REFRESH
                                                                  : EInkDisplay::FAST_REFRESH;
  display.displayBufferAsync(mode);
}

const char* pendingWhy = nullptr;  // what asked for a frame the panel was too busy to show yet

void frame(const char* why) {
  if (why) pendingWhy = why;
  if (display.refreshBusy()) return;  // the next frame, once this refresh is done
  retarget();
  const adr::Refresh r = app.render();
  show(r);
  if (r != adr::Refresh::None && pendingWhy) logScene(pendingWhy);
  pendingWhy = nullptr;
}

void deliver(const adr::Input& e, const char* what) {
  app.handle(e);
  frame(what);
}

[[noreturn]] void sleepNow(bool lowBattery, const char* why) {
  // (on the UART console too, where a device's early life can be read)
#ifdef ADR_SIM
  Serial.printf("[adr] sleep: %s\n", why);
#else
  log_e("[adr] sleep: %s", why);
#endif
  display.waitRefreshComplete();
  app.save();
  retarget();
  app.renderSleep(lowBattery);
  display.displayBuffer(EInkDisplay::FULL_REFRESH);
  Serial.println("[adr] sleep");
  Serial.flush();
  display.deepSleep();
  platform.lightOff();
  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();
}

bool batteryNearlyEmpty() {
  const int pct = platform.batteryPercent();
  return pct >= 0 && pct <= 2 && !platform.charging();
}

adr::Input::KeyCode keyFor(uint8_t b) {
  switch (b) {
    case InputManager::BTN_BACK: return adr::Input::Back;
    case InputManager::BTN_CONFIRM: return adr::Input::Confirm;
    case InputManager::BTN_LEFT: return adr::Input::Left;
    case InputManager::BTN_RIGHT: return adr::Input::Right;
    case InputManager::BTN_UP: return adr::Input::Up;
    default: return adr::Input::Down;
  }
}

bool repeats(uint8_t b) {
  return b == InputManager::BTN_LEFT || b == InputManager::BTN_RIGHT || b == InputManager::BTN_UP ||
         b == InputManager::BTN_DOWN;
}

// Power, pressed: sleep, unless it is still the press that woke the device (its edge can reach
// the input task after setup, while the first refresh ran).
bool powerPressed(uint8_t b) { return b == InputManager::BTN_POWER && millis() - readyAt > 1000; }

void pollKeys() {
  for (uint8_t b; input.popPress(b);) {
    if (b == InputManager::BTN_POWER) {
      if (powerPressed(b)) sleepNow(false, "power");
      continue;
    }
    deliver(adr::Input::keyPress(keyFor(b)), "key");
    heldKey = b;
    heldSince = lastRepeat = millis();
    longSent = false;
  }
  if (heldKey >= 0) {
    if (!input.isPressed(static_cast<uint8_t>(heldKey))) {
      heldKey = -1;
    } else if (millis() - heldSince >= kLongMs) {
      if (repeats(static_cast<uint8_t>(heldKey))) {
        if (millis() - lastRepeat >= kRepeatMs) {
          lastRepeat = millis();
          deliver(adr::Input::keyPress(keyFor(static_cast<uint8_t>(heldKey)), true), "key held");
        }
      } else if (!longSent && heldKey == InputManager::BTN_CONFIRM) {
        longSent = true;
        deliver(adr::Input::keyPress(adr::Input::Confirm, true), "key held");
      }
    }
  }
  // the ascent reads the keys as they are held
  const int dx = (input.isPressed(InputManager::BTN_RIGHT) ? 1 : 0) - (input.isPressed(InputManager::BTN_LEFT) ? 1 : 0);
  const int dy = (input.isPressed(InputManager::BTN_DOWN) ? 1 : 0) - (input.isPressed(InputManager::BTN_UP) ? 1 : 0);
  app.held(dx, dy);
}

adr::Point pagePoint(float nx, float ny) {
  const int px = static_cast<int>(nx * display.getDisplayWidth());
  const int py = static_cast<int>(ny * display.getDisplayHeight());
  return app.canvas().toPage(px, py);
}

void pollTouch() {
  for (uint8_t b; input.popPress(b);) {
    if (b == InputManager::BTN_POWER) {
      if (powerPressed(b)) sleepNow(false, "power");
      continue;
    }
    if (b == InputManager::BTN_UP || b == InputManager::BTN_DOWN) deliver(adr::Input::keyPress(keyFor(b)), "key");
  }
  for (float x0, y0, x1, y1; input.popSwipe(x0, y0, x1, y1);) {
    const adr::Point a = pagePoint(x0, y0), b = pagePoint(x1, y1);
    deliver(adr::Input::swipe(a.x, a.y, b.x, b.y), "swipe");
  }
  for (float x, y; input.popTouchTap(x, y);) {
    const adr::Point p = pagePoint(x, y);
    deliver(adr::Input::tap(p.x, p.y), "tap");
  }
  float hx, hy;
  if (input.isTouchHeldAt(hx, hy)) {
    const adr::Point p = pagePoint(hx, hy);
    app.touchSteer(true, p.x, p.y);
    if (!touchDown) {
      touchDown = true;
      holdFired = false;
      touchSince = millis();
      touchX = hx;
      touchY = hy;
    } else if (!holdFired) {
      const float dx = hx - touchX, dy = hy - touchY;
      if (dx * dx + dy * dy > 0.0009f) {
        holdFired = true;  // moving: a swipe in the making
      } else if (millis() - touchSince >= 550) {
        holdFired = true;
        input.suppressTouchContact();
        const adr::Point q = pagePoint(touchX, touchY);
        deliver(adr::Input::hold(q.x, q.y), "hold");
      }
    }
  } else {
    if (touchDown) app.touchSteer(false, 0, 0);
    touchDown = false;
  }
  // the capacitive Home key: the menu
  if (millis() - lastHomeMs > 300 && (input.wasHomeKeyTapped() || input.wasHomeKeyLongPressed())) {
    lastHomeMs = millis();
    deliver(adr::Input::keyPress(adr::Input::Home), "home");
  }
}

}  // namespace

void setup() {
#if !defined(ADR_SIM) && !defined(ADR_FULL_CLOCK)
  // The game waits on people and on the panel: 80 MHz draws a frame in a few tens of milliseconds
  // and spends much less of the battery doing nothing between them.
  setCpuFrequencyMhz(80);
#endif
  Serial.begin(115200);
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const bool woke = cause != ESP_SLEEP_WAKEUP_UNDEFINED;
#ifndef ADR_SIM
  // Up + Down held while Power wakes it: flash /update.bin from the card (a way back to other
  // firmware without a computer).
  freeink::recovery::SdUpdateOptions recovery;
  recovery.button1 = InputManager::BTN_UP;
  recovery.button2 = InputManager::BTN_DOWN;
  recovery.renameOnSuccess = true;
  freeink::recovery::checkBootCombo(recovery);
#endif
  BoardConfig::holdPowerRails();
  const bool x3 = freeink::selectXteinkDevice();
  if (x3) display.setDisplayX3();
  input.begin();
#ifndef ADR_SIM
  if (!x3) freeink::applyXteinkDisplayController();
#endif
  Serial.printf("[adr] %s, %s\n", BoardConfig::ACTIVE.name, woke ? "woke" : "cold boot");
  freeink::PowerManager::waitForPowerButtonRelease();
  input.beginAsync();
  // whatever the waking press left behind is not a key for the game
  for (uint8_t b; input.popPress(b);) {}
  for (float x, y; input.popTouchTap(x, y);) {}
  display.begin();
  platform.begin();
  app.begin(game, platform, display.getFrameBuffer());
  const bool resumed = app.loadOrStart();
  Serial.printf("[adr] %s\n", resumed ? "the save is back" : "a new game");
  if (batteryNearlyEmpty()) sleepNow(true, "battery");
  retarget();
  app.render();
  display.displayBuffer(woke ? EInkDisplay::HALF_REFRESH : EInkDisplay::FULL_REFRESH);
  lastTick = millis();
  lastBatteryCheck = millis();
  logSeen = game.st().logSerial;
  for (uint8_t b; input.popPress(b);) {}
  readyAt = millis();
  logScene("ready");
}

void loop() {
  if (input.hasTouch()) pollTouch();
  else pollKeys();
  const uint32_t now = millis();
  uint32_t dt = now - lastTick;
  lastTick = now;
  if (dt > 5000) dt = 5000;
  app.tick(dt);
  echoLog();
  if (app.wantsFrame() || pendingWhy) frame(nullptr);
  // left alone: sleep (never in the middle of a fight or the ascent)
  if (app.idleLimitMs() && app.idleMs() > app.idleLimitMs() && app.sleepAllowed()) sleepNow(false, "idle");
  if (now - lastBatteryCheck > 120000) {
    lastBatteryCheck = now;
    if (batteryNearlyEmpty()) sleepNow(true, "battery");
  }
  delay(10);
}
