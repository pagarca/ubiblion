#include "input.h"

#include "pins.h"

namespace Input {
namespace {

const uint8_t kPins[BTN_COUNT] = {PIN_BTN_PREV, PIN_BTN_NEXT, PIN_BTN_OK, PIN_BTN_BACK};
const char* const kNames[BTN_COUNT] = {"PREV", "NEXT", "OK", "BACK"};

#ifdef ARDUINO_ARCH_ESP32
constexpr int kPressedLevel = HIGH;
#else
constexpr int kPressedLevel = LOW;
#endif

struct State {
  bool stable = false;      // true = pressed
  bool lastRaw = false;
  uint32_t lastChange = 0;  // last time the raw level changed
  uint32_t pressedAt = 0;
  bool longFired = false;
};

State g_state[BTN_COUNT];
uint32_t g_lastEvent = 0;

#if UBIBLION_DEV
// Queue of events injected from the web, and a circular log of everything
// that's gone through poll(), real or injected.
Event g_injected[4];
uint8_t g_injectedCount = 0;
LogEntry g_log[LOG_SIZE];
uint8_t g_logCount = 0;
uint8_t g_logHead = 0;

void record(const Event& ev, bool injected) {
  g_log[g_logHead] = {millis(), ev.button, ev.longPress, injected};
  g_logHead = (g_logHead + 1) % LOG_SIZE;
  if (g_logCount < LOG_SIZE) g_logCount++;
}
#endif

}  // namespace

void begin() {
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    pinMode(kPins[i], INPUT_PULLUP);
    g_state[i] = State();
  }
  g_lastEvent = millis();
}

bool poll(Event& out) {
  const uint32_t now = millis();

#if UBIBLION_DEV
  if (g_injectedCount) {
    out = g_injected[0];
    for (uint8_t i = 1; i < g_injectedCount; i++) g_injected[i - 1] = g_injected[i];
    g_injectedCount--;
    g_lastEvent = now;
    record(out, true);
    return true;
  }
#endif

  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    State& s = g_state[i];
    const bool raw = digitalRead(kPins[i]) == kPressedLevel;

    if (raw != s.lastRaw) {
      s.lastRaw = raw;
      s.lastChange = now;
      continue;  // wait for it to settle
    }
    if (now - s.lastChange < DEBOUNCE_MS) continue;

    if (raw && !s.stable) {  // press edge, now stable
      s.stable = true;
      s.pressedAt = now;
      s.longFired = false;
      continue;
    }

    if (raw && s.stable && !s.longFired && now - s.pressedAt >= LONG_PRESS_MS) {
      s.longFired = true;
      g_lastEvent = now;
      out.button = static_cast<Button>(i);
      out.longPress = true;
#if UBIBLION_DEV
      record(out, false);
#endif
      return true;
    }

    if (!raw && s.stable) {  // released
      s.stable = false;
      if (s.longFired) continue;  // the long-press event already fired
      g_lastEvent = now;
      out.button = static_cast<Button>(i);
      out.longPress = false;
#if UBIBLION_DEV
      record(out, false);
#endif
      return true;
    }
  }
  return false;
}

bool isPressed(Button b) { return b < BTN_COUNT && g_state[b].stable; }

bool quiet() {
#if UBIBLION_DEV
  if (g_injectedCount) return false;
#endif
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    const State& s = g_state[i];
    if (s.stable || s.lastRaw || digitalRead(kPins[i]) == kPressedLevel) return false;
    if (millis() - s.lastChange < DEBOUNCE_MS) return false;
  }
  return true;
}

uint32_t idleMillis() { return millis() - g_lastEvent; }

const char* name(Button b) { return b < BTN_COUNT ? kNames[b] : "?"; }

#if UBIBLION_DEV
void inject(Button b, bool longPress) {
  if (b >= BTN_COUNT) return;
  if (g_injectedCount >= sizeof(g_injected) / sizeof(g_injected[0])) return;
  g_injected[g_injectedCount].button = b;
  g_injected[g_injectedCount].longPress = longPress;
  g_injectedCount++;
}

uint8_t logCount() { return g_logCount; }

LogEntry logAt(uint8_t index) {
  if (index >= g_logCount) return LogEntry{0, BTN_NONE, false, false};
  // g_logHead points at the next empty slot: walk backwards from there.
  const uint8_t pos = (g_logHead + LOG_SIZE - 1 - index) % LOG_SIZE;
  return g_log[pos];
}
#endif

}  // namespace Input
