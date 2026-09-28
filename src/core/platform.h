// platform.h — what the app asks of the device. The firmware (src/hal) and the host tests each
// provide one; the app itself never touches hardware.
#pragma once

#include <cstddef>
#include <cstdint>

#include "device.h"

namespace adr {

class Platform {
 public:
  virtual ~Platform() = default;
  virtual const DeviceProfile& profile() = 0;
  virtual uint32_t millis() = 0;
  virtual uint32_t random() = 0;
  // Small records (the save, the settings): whole blobs by key.
  virtual size_t load(const char* key, uint8_t* buf, size_t cap) = 0;  // bytes read, 0 if none
  virtual bool save(const char* key, const uint8_t* buf, size_t len) = 0;
  virtual int batteryPercent() = 0;  // -1 when unknown
  virtual bool charging() = 0;
  virtual void setLight(uint8_t percent) { (void)percent; }
};

}  // namespace adr
