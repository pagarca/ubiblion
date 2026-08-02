// Persistent settings, stored in /config.json.
#pragma once

#include <Arduino.h>

namespace Config {

// Values for Settings::wifiOnBoot.
enum WifiOnBoot : uint8_t {
  WIFI_BOOT_NEVER = 0,     // only on demand (button or menu)
  WIFI_BOOT_IF_EMPTY = 1,  // only if there are no books yet
  WIFI_BOOT_ALWAYS = 2,    // always
};

// Overridable at build time to rescue a device that's been left with no way
// to turn the radio on.
#ifndef UBIBLION_DEFAULT_WIFI_ON_BOOT
#define UBIBLION_DEFAULT_WIFI_ON_BOOT Config::WIFI_BOOT_IF_EMPTY
#endif

struct Settings {
  String ssid;
  String pass;
  String apiKey;                  // empty = no API authentication
  String apPass = "ubiblion";     // fallback AP password (min. 8 characters)
  String hostname = "ubiblion";   // used for mDNS and for the AP's SSID
  uint8_t fontId = 1;             // Display::FontId
  uint8_t fullRefreshEvery = 10;  // full refresh every N pages
  String currentBook;             // id of the currently open book
  // When to turn the radio on at boot. IF_EMPTY handles the cold start
  // (with no books, the only useful thing to do is receive the first one)
  // and ALWAYS is the sensible choice for a device with no buttons wired up
  // yet or one that's permanently on USB power: without it, a reader with
  // books and no buttons has no way to ever turn WiFi back on.
  uint8_t wifiOnBoot = UBIBLION_DEFAULT_WIFI_ON_BOOT;
  uint8_t lang = 0;  // index into the languages compiled in, see I18n
};

void begin();
Settings& get();
bool save();

}  // namespace Config
