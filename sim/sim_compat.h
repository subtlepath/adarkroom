// sim_compat.h — force-included into every unit of a simulator bundle (scripts/sim.sh).
//
// The FreeInk simulator's platform shims cover what its demo firmware uses. this firmware links more of
// the SDK (SDCardManager, PowerManager, Rtc, XteinkDetect), and those libraries lean on a few
// names the shims do not yet declare. They are supplied here rather than by patching the SDK
// checkout; each line can go when the SDK's tools/simulator/platform grows the same thing.
#pragma once

#ifdef __cplusplus
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>  // SDCardManager.cpp calls vTaskDelay without including it

using TwoWire = SimWire;  // Rtc.cpp names the Arduino class

// SDCardManager gives the S3 boards' card its own bus: `SPIClass sd(HSPI); sd.begin(...) && ...`.
class SPIClass : public SimSPI {
 public:
  using SimSPI::SimSPI;
  bool begin(int8_t sclk = -1, int8_t miso = -1, int8_t mosi = -1, int8_t cs = -1) {
    SimSPI::begin(sclk, miso, mosi, cs);
    return true;
  }
};
#ifndef USER_SPI_BEGIN
#define USER_SPI_BEGIN 0
#endif
#endif

// A bundle is a host process: memory that survives deep sleep is simply memory. Note that in
// the simulator *every* static survives a sleep, so state that must not is reset by hand.
#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif
#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif
