// Logging.h — the FreeInk SDK's FrontlightManager logs through "the firmware's Logging.h"
// (LOG_INF(tag, fmt, ...)); the SDK does not ship one. This one writes a line on Serial.
#pragma once

#include <Arduino.h>

#define LOG_INF(tag, fmt, ...) Serial.printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_ERR(tag, fmt, ...) Serial.printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_DBG(tag, fmt, ...) ((void)0)
