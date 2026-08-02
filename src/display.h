// Thin wrapper around GxEPD2 + U8g2_for_Adafruit_GFX.
//
// Wraps the full framebuffer (200x200/8 = 5000 bytes), UTF-8 font selection,
// and the refresh policy: pages are drawn with a partial refresh and every
// FULL_REFRESH_EVERY updates a full one is forced to clear the SSD1681's
// ghosting.
#pragma once

#include <Arduino.h>

namespace Display {

// Panel geometry.
static const int16_t WIDTH = 200;
static const int16_t HEIGHT = 200;

// Fixed margins and bands of the layout.
static const int16_t MARGIN_X = 6;
static const int16_t HEADER_BOTTOM = 17;  // header's separator line
static const int16_t FOOTER_TOP = 182;    // footer's separator line
static const int16_t TEXT_TOP = HEADER_BOTTOM + 3;
static const int16_t TEXT_BOTTOM = FOOTER_TOP - 2;

enum FontId : uint8_t {
  FONT_SMALL = 0,
  FONT_MEDIUM = 1,
  FONT_LARGE = 2,
  FONT_COUNT = 3,
};

void begin();

// Active font. lineHeight() and linesPerPage() depend on it, and the
// pagination index gets invalidated when it changes (see
// Paginator::indexVersion).
void setFont(FontId id);
FontId font();

uint8_t lineHeight();
uint8_t fontAscent();

// Real extent of any glyph's box relative to the baseline. fontAscent() is
// only the height of an 'A': accented uppercase letters and opening
// punctuation reach higher. Highlighted rows must use these, or the white
// text spills outside the black rectangle and disappears against the
// background.
uint8_t glyphAscent();
uint8_t glyphDescent();
uint8_t rowHeight();                  // height of a menu row
int16_t rowBaseline(int16_t rowTop);  // baseline within that row
uint16_t textWidth(const char* utf8);
uint16_t textAreaWidth();
uint8_t linesPerPage();

// --- drawing primitives (operate on the framebuffer, don't refresh) ---
void clear();
void text(int16_t x, int16_t baselineY, const char* utf8);
void text(int16_t x, int16_t baselineY, const __FlashStringHelper* utf8);
void textInverted(int16_t x, int16_t baselineY, const char* utf8);
void textInverted(int16_t x, int16_t baselineY, const __FlashStringHelper* utf8);
void hLine(int16_t y);
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool black);
void frameRect(int16_t x, int16_t y, int16_t w, int16_t h);

// Trims the text to the given width, appending "..." if it doesn't fit.
String ellipsize(const String& utf8, uint16_t maxWidth);

// Standard header + footer; just paints onto the framebuffer, no refresh.
void header(const String& title);
void footer(const String& left, const String& right);

// --- refresh ---
// partial=true uses a differential update; every FULL_REFRESH_EVERY partial
// calls get promoted to a full one automatically.
void flush(bool partial);
void flushFull();
// Refreshes just one region (faster for the indexing progress bar).
void flushWindow(int16_t x, int16_t y, int16_t w, int16_t h);

void setFullRefreshEvery(uint8_t pages);
void hibernate();

#if UBIBLION_DEV
// --- development mode ---
// Text mirror of the last thing drawn: lets you see what's on screen from
// the web without having the device in front of you. Cleared on every
// clear().
static const uint8_t MIRROR_SIZE = 24;
uint8_t mirrorCount();
const String& mirrorText(uint8_t index);
int16_t mirrorX(uint8_t index);
int16_t mirrorY(uint8_t index);
bool mirrorInverted(uint8_t index);
uint32_t refreshCount();

// Diagnostic pattern for the white-on-black text bug: draws a highlighted
// row right after header() and a normal one, and does a full refresh.
void testPattern();
#endif

}  // namespace Display
