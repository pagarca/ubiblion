// Power saving. E-paper keeps its image without power, so between button
// presses there's nothing to do: the ESP32 light-sleeps until a button goes
// high (or the deferred progress save is due). RAM and pin state survive, so
// execution just carries on afterwards.
//
// ESP32 only: on the ESP8266 both calls are no-ops.
#pragma once

#include <Arduino.h>

namespace Power {

// Call once, early in setup().
void begin();

// Sleeps until a button is pressed or `maxMs` elapses (0 = no time limit).
// The caller must make sure nothing else needs the CPU: WiFi off, no
// indexing in progress, buttons released.
void sleep(uint32_t maxMs);

}  // namespace Power
