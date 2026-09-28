// device.h — the X4 Pro, the X4 Classic or the X3 behind the app's Platform: NVS for the saves,
// the battery gauge, the chip's random numbers, the X4 Pro's frontlight. Nothing above this layer
// names a pin.
#pragma once

#include <Arduino.h>

#include "../core/platform.h"

namespace adr {

class DevicePlatform : public Platform {
 public:
  void begin();
  const DeviceProfile& profile() override;
  uint32_t millis() override { return ::millis(); }
  uint32_t random() override;
  size_t load(const char* key, uint8_t* buf, size_t cap) override;
  bool save(const char* key, const uint8_t* buf, size_t len) override;
  int batteryPercent() override;
  bool charging() override;
  void setLight(uint8_t percent) override;
  void lightOff();

 private:
  bool _lightBegun = false;
};

}  // namespace adr
