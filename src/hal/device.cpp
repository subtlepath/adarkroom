#include "device.h"

#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <Preferences.h>

#if !defined(ADR_SIM) && FREEINK_CAP_FRONTLIGHT
#include <FrontlightManager.h>
#endif

namespace adr {

namespace {

BatteryMonitor battery;
#if !defined(ADR_SIM) && FREEINK_CAP_FRONTLIGHT
FrontlightManager frontlight;
#endif
constexpr const char* kNvsSpace = "adarkroom";

}  // namespace

void DevicePlatform::begin() {}

const DeviceProfile& DevicePlatform::profile() {
  const BoardConfig::Board b = BoardConfig::ACTIVE.board;
  if (b == BoardConfig::Board::XteinkX3 || b == BoardConfig::Board::XteinkX3Uc8279) return kX3;
  if (b == BoardConfig::Board::XteinkX4Classic) return kX4Classic;
  return b == BoardConfig::Board::XteinkX4Pro ? kX4Pro : kX4;
}

uint32_t DevicePlatform::random() { return esp_random() ^ micros(); }

size_t DevicePlatform::load(const char* key, uint8_t* buf, size_t cap) {
  Preferences p;
  if (!p.begin(kNvsSpace, true)) return 0;
  const size_t n = p.getBytesLength(key);
  const size_t got = n && n <= cap ? p.getBytes(key, buf, n) : 0;
  p.end();
  return got;
}

bool DevicePlatform::save(const char* key, const uint8_t* buf, size_t len) {
  Preferences p;
  if (!p.begin(kNvsSpace, false)) return false;
  const bool ok = p.putBytes(key, buf, len) == len;
  p.end();
  return ok;
}

int DevicePlatform::batteryPercent() {
  uint16_t pct = 0;
  return battery.readPercentageChecked(pct) ? static_cast<int>(pct) : -1;
}

bool DevicePlatform::charging() { return battery.isCharging(); }

void DevicePlatform::setLight(uint8_t percent) {
  if (BoardConfig::ACTIVE.frontlight.gpio == BoardConfig::PIN_UNASSIGNED) return;
#if !defined(ADR_SIM) && FREEINK_CAP_FRONTLIGHT
  if (!_lightBegun) {
#ifdef FREEINK_FRONTLIGHT_LS
    frontlight.releaseOnWake();
#endif
    frontlight.begin();
    _lightBegun = true;
  }
  frontlight.setColorTemperature(60);  // a warm light, as a fire gives
  frontlight.setBrightness(percent);
#else
  const BoardConfig::FrontlightConfig& fc = BoardConfig::ACTIVE.frontlight;
  if (!_lightBegun) {
    ledcAttach(fc.gpio, fc.pwmFrequency, fc.pwmResolutionBits);
    if (fc.gpioWarm != BoardConfig::PIN_UNASSIGNED) ledcAttach(fc.gpioWarm, fc.pwmFrequency, fc.pwmResolutionBits);
    _lightBegun = true;
  }
  const uint32_t full = (1u << fc.pwmResolutionBits) - 1;
  auto duty = [&](uint32_t pct) { return fc.activeHigh ? full * pct / 100 : full - full * pct / 100; };
  ledcWrite(fc.gpio, duty(percent * 40u / 100u));
  if (fc.gpioWarm != BoardConfig::PIN_UNASSIGNED) ledcWrite(fc.gpioWarm, duty(percent * 60u / 100u));
#endif
}

void DevicePlatform::lightOff() {
  if (!_lightBegun) return;
#if !defined(ADR_SIM) && FREEINK_CAP_FRONTLIGHT
  frontlight.off();
#ifdef FREEINK_FRONTLIGHT_LS
  frontlight.park();
#endif
#else
  setLight(0);
#endif
}

}  // namespace adr
