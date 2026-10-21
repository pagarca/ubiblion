#include "menu.h"

#include "config.h"
#include "display.h"
#include "i18n.h"
#include "library.h"
#include "webui.h"

namespace Menu {
namespace {

enum Screen : uint8_t { SCR_NONE, SCR_LIBRARY, SCR_SETTINGS, SCR_CONFIRM_DELETE };

enum SettingsItem : uint8_t {
  SET_FONT = 0,
  SET_REFRESH,
  SET_WIFI,
  SET_LANG,
  SET_REINDEX,
  SET_INFO,
  SET_BACK,
  SET_COUNT,
};

Screen g_screen = SCR_NONE;
uint8_t g_sel = 0;   // selected row, as a VISIBLE index (see settingsItemAt)
uint8_t g_top = 0;   // first visible row
char g_pendingId[Library::ID_LEN] = {0};

uint8_t rowsPerScreen() {
  const int16_t usable = Display::FOOTER_TOP - Display::HEADER_BOTTOM - 4;
  const uint8_t n = usable / Display::rowHeight();
  return n > 0 ? n : 1;
}

void clampScroll(uint8_t total) {
  const uint8_t rows = rowsPerScreen();
  if (g_sel >= total) g_sel = total ? total - 1 : 0;
  if (g_sel < g_top) g_top = g_sel;
  if (g_sel >= g_top + rows) g_top = g_sel - rows + 1;
}

void drawRow(uint8_t row, const String& label, bool selected) {
  const int16_t rh = Display::rowHeight();
  const int16_t y = Display::HEADER_BOTTOM + 3 + row * rh;
  const int16_t baseline = Display::rowBaseline(y);
  const String text = Display::ellipsize(label, Display::textAreaWidth() - 4);
  if (selected) {
    // The rectangle covers the whole glyph box: if it fell short at the top,
    // accents and opening punctuation would paint white outside the black
    // fill and vanish.
    Display::fillRect(0, y, Display::WIDTH, rh, true);
    Display::textInverted(Display::MARGIN_X, baseline, text.c_str());
  } else {
    Display::text(Display::MARGIN_X, baseline, text.c_str());
  }
}

// The Settings menu has one row that only shows up when more than one
// language is compiled in: settingsRowCount()/settingsItemAt() translate
// between "visible row index" (what g_sel/g_top track) and the actual
// SettingsItem, skipping SET_LANG when it's hidden.
bool langRowVisible() { return I18n::languageCount() > 1; }

uint8_t settingsRowCount() { return langRowVisible() ? SET_COUNT : SET_COUNT - 1; }

SettingsItem settingsItemAt(uint8_t row) {
  if (!langRowVisible() && row >= SET_LANG) row++;
  return static_cast<SettingsItem>(row);
}

String settingsLabel(SettingsItem item) {
  Config::Settings& s = Config::get();
  switch (item) {
    case SET_FONT: {
      I18n::Str name = I18n::STR_FONT_MEDIUM;
      if (s.fontId == Display::FONT_SMALL) name = I18n::STR_FONT_SMALL;
      else if (s.fontId == Display::FONT_LARGE) name = I18n::STR_FONT_LARGE;
      return String(I18n::t(I18n::STR_FONT_PREFIX)) + I18n::t(name);
    }
    case SET_REFRESH: {
      if (s.fullRefreshEvery == 0) return I18n::t(I18n::STR_FULL_REFRESH_NEVER);
      return String(I18n::t(I18n::STR_FULL_REFRESH_PREFIX)) + s.fullRefreshEvery;
    }
    case SET_WIFI:
      return WebUI::isActive() ? I18n::t(I18n::STR_WIFI_TURN_OFF) : I18n::t(I18n::STR_WIFI_TURN_ON);
    case SET_LANG:
      return String(I18n::t(I18n::STR_LANGUAGE_PREFIX)) +
             I18n::languageName(I18n::currentLanguage());
    case SET_REINDEX:
      return I18n::t(I18n::STR_REINDEX_CURRENT);
    case SET_INFO: {
      char kb[24];
      snprintf(kb, sizeof(kb), I18n::fmt(I18n::STR_KB_FREE_FMT), Library::freeBytes() / 1024);
      return String("v") + UBIBLION_VERSION + "  " + kb;
    }
    case SET_BACK:
      return I18n::t(I18n::STR_BACK_ITEM);
    default:
      return String();
  }
}

// The list has one virtual row at the end ("Settings...") so the settings
// screen is reachable even with an empty library.
uint8_t libraryRows() { return Library::count() + 1; }
bool isSettingsRow(uint8_t index) { return index == Library::count(); }

void drawLibrary() {
  const uint8_t total = libraryRows();
  clampScroll(total);

  Display::clear();
  Display::header(I18n::t(I18n::STR_LIBRARY));

  const uint8_t rows = rowsPerScreen();
  for (uint8_t i = 0; i < rows && g_top + i < total; i++) {
    const uint8_t idx = g_top + i;
    String label;
    if (isSettingsRow(idx)) {
      label = I18n::t(I18n::STR_SETTINGS_ROW);
    } else {
      Library::BookMeta m;
      if (Library::at(idx, m)) {
        label = m.title;
        if (m.pages > 1) label += String(" (") + ((m.page * 100) / (m.pages - 1)) + "%)";
      } else {
        label = Library::idAt(idx);
      }
    }
    drawRow(i, label, idx == g_sel);
  }

  if (Library::count() == 0) {
    const int16_t y = Display::HEADER_BOTTOM + 3 + 2 * Display::rowHeight();
    Display::text(Display::MARGIN_X, y + Display::fontAscent(), I18n::t(I18n::STR_NO_BOOKS_1));
    Display::text(Display::MARGIN_X, y + Display::lineHeight() + Display::fontAscent(),
                  I18n::t(I18n::STR_NO_BOOKS_2));
    Display::text(Display::MARGIN_X, y + 2 * Display::lineHeight() + Display::fontAscent(),
                  I18n::t(I18n::STR_NO_BOOKS_3));
  }

  char count[24];
  snprintf(count, sizeof(count), "%u/%u", g_sel + 1, total);
  Display::footer(I18n::t(I18n::STR_OK_OPEN_LONG_DELETE), count);
  Display::flushFull();
}

void drawSettings() {
  const uint8_t total = settingsRowCount();
  clampScroll(total);
  Display::clear();
  Display::header(I18n::t(I18n::STR_SETTINGS));
  const uint8_t rows = rowsPerScreen();
  for (uint8_t i = 0; i < rows && g_top + i < total; i++) {
    drawRow(i, settingsLabel(settingsItemAt(g_top + i)), (g_top + i) == g_sel);
  }
  Display::footer(I18n::t(I18n::STR_OK_CHANGE), "");
  Display::flushFull();
}

void drawConfirmDelete() {
  Library::BookMeta m;
  const bool ok = Library::load(g_pendingId, m);
  Display::clear();
  Display::header(I18n::t(I18n::STR_DELETE_BOOK));
  const int16_t lh = Display::lineHeight();
  int16_t y = Display::HEADER_BOTTOM + 6 + Display::fontAscent();
  Display::text(Display::MARGIN_X, y, I18n::t(I18n::STR_WILL_DELETE));
  y += lh;
  Display::text(Display::MARGIN_X, y,
                Display::ellipsize(ok ? m.title : String(g_pendingId),
                                   Display::textAreaWidth())
                    .c_str());
  y += 2 * lh;
  Display::text(Display::MARGIN_X, y, I18n::t(I18n::STR_OK_CONFIRM));
  y += lh;
  Display::text(Display::MARGIN_X, y, I18n::t(I18n::STR_BACK_CANCEL));
  Display::footer("", "");
  Display::flushFull();
}

Action handleLibrary(const Input::Event& ev, char* outId, size_t outIdLen) {
  const uint8_t total = libraryRows();
  switch (ev.button) {
    case Input::BTN_PREV:
      if (g_sel > 0) {
        g_sel--;
        return ACT_REDRAW;
      }
      return ACT_NONE;
    case Input::BTN_NEXT:
      if (g_sel + 1 < total) {
        g_sel++;
        return ACT_REDRAW;
      }
      return ACT_NONE;
    case Input::BTN_OK:
      if (isSettingsRow(g_sel)) {
        if (ev.longPress) return ACT_NONE;
        g_screen = SCR_SETTINGS;
        g_sel = 0;
        g_top = 0;
        return ACT_REDRAW;
      }
      if (ev.longPress) {
        strlcpy(g_pendingId, Library::idAt(g_sel), sizeof(g_pendingId));
        g_screen = SCR_CONFIRM_DELETE;
        return ACT_REDRAW;
      }
      strlcpy(outId, Library::idAt(g_sel), outIdLen);
      return ACT_OPEN_BOOK;
    case Input::BTN_BACK:
      if (ev.longPress) return ACT_TOGGLE_WIFI;
      return ACT_NONE;  // returning to the reader is handled in handle()
    default:
      return ACT_NONE;
  }
}

Action handleSettings(const Input::Event& ev, char* outId, size_t outIdLen) {
  Config::Settings& s = Config::get();
  const uint8_t total = settingsRowCount();
  switch (ev.button) {
    case Input::BTN_PREV:
      if (g_sel > 0) {
        g_sel--;
        return ACT_REDRAW;
      }
      return ACT_NONE;
    case Input::BTN_NEXT:
      if (g_sel + 1 < total) {
        g_sel++;
        return ACT_REDRAW;
      }
      return ACT_NONE;
    case Input::BTN_BACK:
      g_screen = SCR_LIBRARY;
      g_sel = 0;
      g_top = 0;
      return ACT_REDRAW;
    case Input::BTN_OK:
      switch (settingsItemAt(g_sel)) {
        case SET_FONT: {
          s.fontId = (s.fontId + 1) % Display::FONT_COUNT;
          Display::setFont(static_cast<Display::FontId>(s.fontId));
          Config::save();
          return ACT_FONT_CHANGED;
        }
        case SET_REFRESH: {
          // 5 -> 10 -> 20 -> never -> 5
          if (s.fullRefreshEvery == 5) s.fullRefreshEvery = 10;
          else if (s.fullRefreshEvery == 10) s.fullRefreshEvery = 20;
          else if (s.fullRefreshEvery == 20) s.fullRefreshEvery = 0;
          else s.fullRefreshEvery = 5;
          Display::setFullRefreshEvery(s.fullRefreshEvery);
          Config::save();
          return ACT_REDRAW;
        }
        case SET_WIFI:
          return ACT_TOGGLE_WIFI;
        case SET_LANG: {
          const uint8_t count = I18n::languageCount();
          if (count > 1) I18n::setLanguage((I18n::currentLanguage() + 1) % count);
          return ACT_REDRAW;
        }
        case SET_REINDEX:
          if (s.currentBook.length()) {
            strlcpy(outId, s.currentBook.c_str(), outIdLen);
            return ACT_REINDEX_BOOK;
          }
          return ACT_NONE;
        case SET_INFO:
          return ACT_REDRAW;
        case SET_BACK:
          g_screen = SCR_LIBRARY;
          g_sel = 0;
          g_top = 0;
          return ACT_REDRAW;
        default:
          return ACT_NONE;
      }
    default:
      return ACT_NONE;
  }
}

Action handleConfirm(const Input::Event& ev, char* outId, size_t outIdLen) {
  if (ev.button == Input::BTN_OK) {
    strlcpy(outId, g_pendingId, outIdLen);
    g_screen = SCR_LIBRARY;
    return ACT_DELETE_BOOK;
  }
  if (ev.button == Input::BTN_BACK) {
    g_screen = SCR_LIBRARY;
    return ACT_REDRAW;
  }
  return ACT_NONE;
}

}  // namespace

void openLibrary() {
  g_screen = SCR_LIBRARY;
  g_sel = 0;
  g_top = 0;
  Library::refresh();
  // Leaves the cursor on the book that was being read.
  const int16_t idx = Library::indexOf(Config::get().currentBook.c_str());
  if (idx >= 0) g_sel = idx;
}

void openSettings() {
  g_screen = SCR_SETTINGS;
  g_sel = 0;
  g_top = 0;
}

bool isOpen() { return g_screen != SCR_NONE; }

void draw() {
  switch (g_screen) {
    case SCR_LIBRARY:
      drawLibrary();
      break;
    case SCR_SETTINGS:
      drawSettings();
      break;
    case SCR_CONFIRM_DELETE:
      drawConfirmDelete();
      break;
    default:
      break;
  }
}

Action handle(const Input::Event& ev, char* outId, size_t outIdLen) {
  switch (g_screen) {
    case SCR_LIBRARY: {
      // Short BACK returns to the reader if a book is open; otherwise opens
      // settings.
      if (ev.button == Input::BTN_BACK && !ev.longPress &&
          Config::get().currentBook.length()) {
        g_screen = SCR_NONE;
        return ACT_BACK_TO_READER;
      }
      return handleLibrary(ev, outId, outIdLen);
    }
    case SCR_SETTINGS:
      return handleSettings(ev, outId, outIdLen);
    case SCR_CONFIRM_DELETE:
      return handleConfirm(ev, outId, outIdLen);
    default:
      return ACT_NONE;
  }
}

}  // namespace Menu
