// On-device UI strings, translated into up to five languages.
//
// Which languages get compiled into the firmware is controlled by the
// UBIBLION_LANG_EN/ES/FR/DE/IT build flags (see platformio.ini): each is a
// PROGMEM string table, so disabling the ones you don't need trims the
// firmware binary. At least one language must stay enabled; English is the
// fallback if the active index somehow points nowhere.
//
// The active language is a Config setting, persisted like everything else,
// and only shows up as a menu option when more than one is compiled in.
#pragma once

#include <Arduino.h>

namespace I18n {

enum Str : uint8_t {
  STR_WIFI = 0,
  STR_PRESS_ANY_BUTTON,
  STR_INDEXING,
  STR_PREPARING_PAGES,
  STR_BACK_CANCEL,
  STR_OFF,
  STR_NETWORK_PREFIX,
  STR_AP_PREFIX,
  STR_KEY_PREFIX,
  STR_INDEXING_PCT_FMT,
  STR_BOOKS_FREE_FMT,
  STR_BACK_WIFI_OFF,
  STR_COULD_NOT_INDEX,
  STR_UNREADABLE_BOOK,
  STR_CONNECTING,
  STR_FONT_PREFIX,
  STR_FONT_SMALL,
  STR_FONT_MEDIUM,
  STR_FONT_LARGE,
  STR_FULL_REFRESH_NEVER,
  STR_FULL_REFRESH_PREFIX,
  STR_WIFI_TURN_OFF,
  STR_WIFI_TURN_ON,
  STR_LANGUAGE_PREFIX,
  STR_REINDEX_CURRENT,
  STR_KB_FREE_FMT,
  STR_BACK_ITEM,
  STR_LIBRARY,
  STR_SETTINGS_ROW,
  STR_SETTINGS,
  STR_NO_BOOKS_1,
  STR_NO_BOOKS_2,
  STR_NO_BOOKS_3,
  STR_OK_OPEN_LONG_DELETE,
  STR_OK_CHANGE,
  STR_DELETE_BOOK,
  STR_WILL_DELETE,
  STR_OK_CONFIRM,
  STR_EMPTY_PAGE,
  STR_PAGES_FMT,
  STR_COUNT,
};

// Reads the saved language index from Config and clamps it to the set of
// languages actually compiled in.
void begin();

// Returns the string for `id` in the active language, as a flash-resident
// pointer. Safe to pass straight to u8g2.print(), String concatenation, etc.
const __FlashStringHelper* t(Str id);

// Same string as t(), but as a plain const char* for use as a printf/snprintf
// format string (whose parameter type doesn't accept __FlashStringHelper*).
// Safe on ESP8266: PROGMEM data is directly byte-readable through a normal
// pointer, which is exactly what the String(const __FlashStringHelper*)
// constructor relies on too.
const char* fmt(Str id);

uint8_t languageCount();                  // how many languages were compiled in
uint8_t currentLanguage();                // index into the compiled set, 0-based
void setLanguage(uint8_t index);          // out-of-range wraps to 0
const char* languageCode(uint8_t index);  // "en", "es", "fr", "de", "it"
const char* languageName(uint8_t index);  // native name: "English", "Español", ...

}  // namespace I18n
