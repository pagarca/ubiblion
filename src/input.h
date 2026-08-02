// Reads the 4 buttons with debouncing and long-press detection.
//
// All four pins use INPUT_PULLUP and the buttons go to GND, so the resting
// level is high. A long press fires as soon as the threshold is reached (not
// on release), so the user notices the response on screen; a short press
// fires on release.
#pragma once

#include <Arduino.h>

namespace Input {

enum Button : uint8_t {
  BTN_PREV = 0,
  BTN_NEXT = 1,
  BTN_OK = 2,
  BTN_BACK = 3,
  BTN_COUNT = 4,
  BTN_NONE = 255,
};

static const uint16_t DEBOUNCE_MS = 25;
static const uint16_t LONG_PRESS_MS = 600;

struct Event {
  Button button = BTN_NONE;
  bool longPress = false;
};

// Sets up the pins. Call after Serial.begin(..., SERIAL_TX_ONLY), since
// BTN_OK shares a pin with RX.
void begin();

// Returns true and fills `out` when there's a pending event.
bool poll(Event& out);

bool isPressed(Button b);

// Milliseconds since the last event; used by the deferred progress save.
uint32_t idleMillis();

const char* name(Button b);

#if UBIBLION_DEV
// --- development mode ---
// Injects an event as if the button had been pressed: lets you drive the
// whole device from the web while no buttons are wired up yet.
void inject(Button b, bool longPress);

// Log of the latest events, to check the wiring from the web.
static const uint8_t LOG_SIZE = 12;
struct LogEntry {
  uint32_t ms;
  Button button;
  bool longPress;
  bool injected;
};
uint8_t logCount();
LogEntry logAt(uint8_t index);  // 0 = most recent
#endif

}  // namespace Input
