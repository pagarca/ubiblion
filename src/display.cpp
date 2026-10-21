#include "display.h"

#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "pins.h"

namespace Display {
namespace {

// Full page buffer: 200*200/8 = 5000 bytes. Comfortably fits the ESP8266 and
// lets us use display(partial) and displayWindow() without repainting.
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> epd(
    GxEPD2_154_D67(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

U8G2_FOR_ADAFRUIT_GFX u8g2;

// "_tf" fonts: cover U+0020..U+00FF, i.e. accents, umlauts and the Latin-1
// punctuation the UI's translated strings need.
const uint8_t* const kFonts[FONT_COUNT] = {
    u8g2_font_helvR10_tf,
    u8g2_font_helvR12_tf,
    u8g2_font_helvR14_tf,
};

FontId g_font = FONT_MEDIUM;
uint8_t g_lineHeight = 17;
uint8_t g_ascent = 12;

uint8_t g_fullRefreshEvery = 10;
uint8_t g_sincefull = 0;

#if UBIBLION_DEV
String g_mirror[MIRROR_SIZE];
int16_t g_mirrorX[MIRROR_SIZE];
int16_t g_mirrorY[MIRROR_SIZE];
bool g_mirrorInv[MIRROR_SIZE];
uint8_t g_mirrorN = 0;
uint32_t g_refreshes = 0;

void mirrorAdd(int16_t x, int16_t y, const char* utf8, bool inverted) {
  if (g_mirrorN >= MIRROR_SIZE) return;
  g_mirror[g_mirrorN] = utf8;
  g_mirrorX[g_mirrorN] = x;
  g_mirrorY[g_mirrorN] = y;
  g_mirrorInv[g_mirrorN] = inverted;
  g_mirrorN++;
}
#endif

void applyFont() {
  u8g2.setFont(kFonts[g_font]);
  g_ascent = u8g2.getFontAscent();
  // getFontDescent() is negative; +2px of line spacing.
  g_lineHeight = g_ascent - u8g2.getFontDescent() + 2;
}

// Steps back to the start of the UTF-8 code point before `i`.
size_t prevCodepoint(const String& s, size_t i) {
  if (i == 0) return 0;
  size_t j = i - 1;
  while (j > 0 && (static_cast<uint8_t>(s[j]) & 0xC0) == 0x80) j--;
  return j;
}

}  // namespace

void begin() {
  // The first parameter is GxEPD2's diagnostic bitrate: it must be 0, because
  // a nonzero value makes it call Serial.begin() in full-duplex mode
  // internally, stealing GPIO3 (RX), which here is the OK button.
  epd.init(0, true, 2, false);
  epd.setRotation(0);
  epd.setFullWindow();
  u8g2.begin(epd);
  u8g2.setFontMode(1);  // transparent
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(GxEPD_BLACK);
  u8g2.setBackgroundColor(GxEPD_WHITE);
  applyFont();
}

void setFont(FontId id) {
  if (id >= FONT_COUNT) id = FONT_MEDIUM;
  g_font = id;
  applyFont();
}

FontId font() { return g_font; }

uint8_t lineHeight() { return g_lineHeight; }
uint8_t fontAscent() { return g_ascent; }

// u8g2's glyph box: max_char_height is the total height and y_offset is the
// (negative) distance from its base to the text baseline, so the highest
// point sits at max_char_height + y_offset.
uint8_t glyphAscent() {
  const int8_t top = u8g2.u8g2.font_info.max_char_height + u8g2.u8g2.font_info.y_offset;
  return top > 0 ? top : g_ascent;
}

uint8_t glyphDescent() {
  const int8_t bottom = -u8g2.u8g2.font_info.y_offset;
  return bottom > 0 ? bottom : 3;
}

// Two pixels of breathing room above and below so the highlight doesn't feel
// cramped.
static const uint8_t kRowPad = 2;

uint8_t rowHeight() { return glyphAscent() + glyphDescent() + 2 * kRowPad; }

int16_t rowBaseline(int16_t rowTop) { return rowTop + kRowPad + glyphAscent(); }

uint16_t textWidth(const char* utf8) { return u8g2.getUTF8Width(utf8); }

uint16_t textAreaWidth() { return WIDTH - 2 * MARGIN_X; }

uint8_t linesPerPage() {
  int16_t usable = TEXT_BOTTOM - TEXT_TOP;
  uint8_t n = usable / g_lineHeight;
  return n > 0 ? n : 1;
}

void clear() {
  epd.fillScreen(GxEPD_WHITE);
#if UBIBLION_DEV
  g_mirrorN = 0;
#endif
}

void text(int16_t x, int16_t baselineY, const char* utf8) {
  u8g2.setForegroundColor(GxEPD_BLACK);
  u8g2.setCursor(x, baselineY);
  u8g2.print(utf8);
#if UBIBLION_DEV
  mirrorAdd(x, baselineY, utf8, false);
#endif
}

void text(int16_t x, int16_t baselineY, const __FlashStringHelper* utf8) {
  text(x, baselineY, String(utf8).c_str());
}

// u8g2_for_Adafruit_GFX only decodes glyphs correctly when the foreground
// color is 0 (black): with any other value (white, or even a plain "1") it
// draws shapeless blocks instead of letters. Confirmed on real hardware with
// a diagnostic pattern: black comes out perfect, white comes out broken, and
// Adafruit_GFX's own font (which doesn't go through u8g2) renders fine in any
// color. The workaround: render the text in black -- the only path that
// works -- onto an auxiliary in-RAM canvas, then blit it inverted by hand
// onto the real screen with drawPixel, which is reliable with any color since
// it never goes through u8g2's glyph decoder.
void textInverted(int16_t x, int16_t baselineY, const char* utf8) {
  const int16_t asc = glyphAscent();
  const int16_t desc = glyphDescent();
  // +2px of margin: getUTF8Width() can undershoot because of the last
  // character's right-side bearing, so this avoids clipping it on the canvas.
  const int16_t w = u8g2.getUTF8Width(utf8) + 2;
  const int16_t h = asc + desc;
  if (w <= 0 || h <= 0) return;

  GFXcanvas1 canvas(w, h);
  canvas.fillScreen(1);  // 1 = background; cleared to 0 wherever the glyph draws ink
  u8g2.begin(canvas);
  u8g2.setForegroundColor(0);  // the only value u8g2 decodes correctly
  u8g2.setCursor(0, asc);
  u8g2.print(utf8);
  u8g2.begin(epd);  // point back at the real screen

  const int16_t top = baselineY - asc;
  for (int16_t cy = 0; cy < h; cy++) {
    for (int16_t cx = 0; cx < w; cx++) {
      if (!canvas.getPixel(cx, cy)) epd.drawPixel(x + cx, top + cy, GxEPD_WHITE);
    }
  }
#if UBIBLION_DEV
  mirrorAdd(x, baselineY, utf8, true);
#endif
}

void textInverted(int16_t x, int16_t baselineY, const __FlashStringHelper* utf8) {
  textInverted(x, baselineY, String(utf8).c_str());
}

void hLine(int16_t y) { epd.drawFastHLine(0, y, WIDTH, GxEPD_BLACK); }

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool black) {
  epd.fillRect(x, y, w, h, black ? GxEPD_BLACK : GxEPD_WHITE);
}

void frameRect(int16_t x, int16_t y, int16_t w, int16_t h) {
  epd.drawRect(x, y, w, h, GxEPD_BLACK);
}

String ellipsize(const String& utf8, uint16_t maxWidth) {
  if (u8g2.getUTF8Width(utf8.c_str()) <= maxWidth) return utf8;
  const uint16_t dots = u8g2.getUTF8Width("...");
  String out = utf8;
  while (out.length() > 0) {
    out.remove(prevCodepoint(out, out.length()));
    if (u8g2.getUTF8Width(out.c_str()) + dots <= maxWidth) break;
  }
  out += "...";
  return out;
}

void header(const String& title) {
  String t = ellipsize(title, textAreaWidth());
  text(MARGIN_X, 13, t.c_str());
  hLine(HEADER_BOTTOM);
}

void footer(const String& left, const String& right) {
  hLine(FOOTER_TOP);
  const int16_t baseline = FOOTER_TOP + 3 + 10;
  if (left.length()) text(MARGIN_X, baseline, left.c_str());
  if (right.length()) {
    int16_t x = WIDTH - MARGIN_X - u8g2.getUTF8Width(right.c_str());
    text(x, baseline, right.c_str());
  }
}

void flush(bool partial) {
  if (!partial) {
    flushFull();
    return;
  }
  if (g_fullRefreshEvery && ++g_sincefull >= g_fullRefreshEvery) {
    flushFull();
    return;
  }
  epd.display(true);
#if UBIBLION_DEV
  g_refreshes++;
#endif
}

void flushFull() {
#if UBIBLION_DEV
  Serial.println("[disp] full flush start");
#endif
  g_sincefull = 0;
  epd.display(false);
#if UBIBLION_DEV
  g_refreshes++;
  Serial.printf("[disp] full flush done (%u)\n", g_refreshes);
#endif
}

void flushWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
  epd.displayWindow(x, y, w, h);
}

void setFullRefreshEvery(uint8_t pages) { g_fullRefreshEvery = pages; }

void hibernate() { epd.hibernate(); }

#if UBIBLION_DEV
uint8_t mirrorCount() { return g_mirrorN; }
const String& mirrorText(uint8_t index) {
  static const String empty;
  return index < g_mirrorN ? g_mirror[index] : empty;
}
int16_t mirrorX(uint8_t index) { return index < g_mirrorN ? g_mirrorX[index] : 0; }
int16_t mirrorY(uint8_t index) { return index < g_mirrorN ? g_mirrorY[index] : 0; }
bool mirrorInverted(uint8_t index) { return index < g_mirrorN ? g_mirrorInv[index] : false; }
uint32_t refreshCount() { return g_refreshes; }

// Mirrors Menu::drawRow(), using the SAME public functions the real menu uses
// (text()/textInverted()), so this pattern genuinely exercises the fix
// instead of a simplified version that bypassed the bug.
void patternRow(int16_t rowTop, const char* label, bool selected) {
  const int16_t rh = rowHeight();
  const int16_t baseline = rowBaseline(rowTop);
  if (selected) {
    fillRect(0, rowTop, WIDTH, rh, true);
    textInverted(MARGIN_X, baseline, label);
  } else {
    text(MARGIN_X, baseline, label);
  }
}

// Diagnostic pattern: exercises a highlighted row right after header() (the
// case that originally revealed the u8g2 white-ink bug, see textInverted)
// plus a normal row, both with accents to cover UTF-8 too.
void testPattern() {
  clear();
  header(F("Screen test"));

  int16_t y = HEADER_BOTTOM + 3;
  patternRow(y, "Highlighted áéíóú ñ ¿?", true);
  y += rowHeight();
  patternRow(y, "Normal áéíóú ñ ¿?", false);

  footer(F("footer"), "");
  flushFull();
}
#endif

}  // namespace Display
