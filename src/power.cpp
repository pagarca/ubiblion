#include "power.h"

#ifdef ARDUINO_ARCH_ESP32

#include <driver/gpio.h>
#include <esp_sleep.h>

#include "display.h"
#include "pins.h"

namespace Power {

void begin() {
  // 80MHz is plenty for drawing text and still supports WiFi.
  setCpuFrequencyMhz(80);

  // Buttons are active high. Level wakeup works on any GPIO in light sleep.
  const uint8_t pins[] = {PIN_BTN_PREV, PIN_BTN_NEXT, PIN_BTN_OK, PIN_BTN_BACK};
  for (uint8_t pin : pins) {
    gpio_wakeup_enable(static_cast<gpio_num_t>(pin), GPIO_INTR_HIGH_LEVEL);
  }
  esp_sleep_enable_gpio_wakeup();
}

void sleep(uint32_t maxMs) {
  // A partial refresh leaves the panel's charge pump running.
  Display::powerOff();

  if (maxMs) {
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(maxMs) * 1000);
  } else {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  }
  Serial.flush();  // the UART stops mid-byte otherwise
  esp_light_sleep_start();
}

}  // namespace Power

#else  // ESP8266: not implemented

namespace Power {

void begin() {}
void sleep(uint32_t) {}

}  // namespace Power

#endif
