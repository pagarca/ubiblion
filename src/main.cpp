// ubiblion - ebook reader on an ESP8266 + WeAct 1.54" e-paper (SSD1681)
//
// Main loop and state machine. The modules do the work:
//   display/input  -> hardware
//   library        -> files on LittleFS
//   paginator      -> page index and line wrapping
//   reader/menu    -> screens
//   webserver      -> WiFi on demand, REST API and the web UI
//   power          -> sleeping between button presses
//   screensavers.h -> generated images (tools/make_screensavers.py)
//   i18n           -> on-device UI strings, up to six languages
#include <Arduino.h>

#include "compat.h"
#include "config.h"
#include "display.h"

#include "i18n.h"
#include "input.h"
#include "library.h"
#include "menu.h"
#include "paginator.h"
#include "power.h"
#include "reader.h"
#if __has_include("screensavers_local.h")
#include "screensavers_local.h"  // your own images, not in git
#else
#include "screensavers.h"
#endif
#include "webui.h"

namespace {

enum State : uint8_t {
  ST_READING,
  ST_MENU,
  ST_INDEXING,
  ST_WIFI,
  ST_MESSAGE,
  ST_SCREENSAVER,
};

State g_state = ST_MENU;
char g_openAfterIndex[Library::ID_LEN] = {0};
uint8_t g_lastProgress = 255;
State g_stateBeforeSaver = ST_MENU;
uint8_t g_nextSaver = 0;

const int16_t kBarX = Display::MARGIN_X;
const int16_t kBarW = Display::WIDTH - 2 * Display::MARGIN_X;
const int16_t kBarH = 12;

#if UBIBLION_DEV && defined(ARDUINO_ARCH_ESP32)
// --- loop-stall watchdog (diagnostics only, ESP32 dev builds) ---
// The e-paper and WiFi code can block inside loop() and, because ICMP
// replies never touch it, the device keeps answering ping while the web
// server is silently dead (that's how the first on-hardware freeze showed
// up). This FreeRTOS task sits on the other core and shouts on serial when
// loop() stops making progress. It never restarts anything: the point is a
// traceable stall, not a reboot that hides the bug.
volatile uint32_t g_loopTick = 0;

void stallWatcher(void* arg) {
  (void)arg;
  uint32_t prev = 0;
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(3000));
    const uint32_t tick = g_loopTick;
    if (tick == prev && tick != 0) {
      Serial.printf("[wdt] loop stalled for %u ms (last tick at %u)\n", millis() - tick, tick);
    }
    prev = tick;
  }
}
#endif

void showMessage(const String& text) {
  g_state = ST_MESSAGE;
  Display::clear();
  Display::header(F("ubiblion"));
  Display::text(Display::MARGIN_X, Display::HEADER_BOTTOM + 20 + Display::fontAscent(),
                Display::ellipsize(text, Display::textAreaWidth()).c_str());
  Display::footer(I18n::t(I18n::STR_PRESS_ANY_BUTTON), "");
  Display::flushFull();
}

int16_t indexingBarY() { return Display::HEADER_BOTTOM + 4 * Display::lineHeight(); }

void drawIndexing(bool full) {
  Library::BookMeta meta;
  const bool ok = Library::load(Paginator::indexingId(), meta);
  const uint8_t pct = Paginator::progress();

  Display::clear();
  Display::header(I18n::t(I18n::STR_INDEXING));

  int16_t y = Display::HEADER_BOTTOM + 6 + Display::fontAscent();
  Display::text(Display::MARGIN_X, y,
                Display::ellipsize(ok ? meta.title : String(Paginator::indexingId()),
                                   Display::textAreaWidth())
                    .c_str());
  y += Display::lineHeight();
  Display::text(Display::MARGIN_X, y, I18n::t(I18n::STR_PREPARING_PAGES));

  const int16_t barY = indexingBarY();
  Display::frameRect(kBarX, barY, kBarW, kBarH);
  const int16_t fill = static_cast<int32_t>(kBarW - 4) * pct / 100;
  if (fill > 0) Display::fillRect(kBarX + 2, barY + 2, fill, kBarH - 4, true);

  char pctText[8];
  snprintf(pctText, sizeof(pctText), "%u%%", pct);
  Display::text(Display::MARGIN_X, barY + kBarH + 4 + Display::fontAscent(), pctText);

  char pages[24];
  snprintf(pages, sizeof(pages), I18n::fmt(I18n::STR_PAGES_FMT), Paginator::indexedPages());
  Display::footer(I18n::t(I18n::STR_BACK_CANCEL), pages);

  if (full) {
    Display::flushFull();
  } else {
    // Just the progress bar band: much faster.
    Display::flushWindow(0, barY - 2, Display::WIDTH, kBarH + Display::lineHeight() + 8);
  }
  g_lastProgress = pct;
}

void drawWifi() {
  Display::clear();
  Display::header(I18n::t(I18n::STR_WIFI));
  int16_t y = Display::HEADER_BOTTOM + 6 + Display::fontAscent();
  const int16_t lh = Display::lineHeight();

  if (!WebUI::isActive()) {
    Display::text(Display::MARGIN_X, y, I18n::t(I18n::STR_OFF));
  } else {
    const String prefix = WebUI::isAP() ? I18n::t(I18n::STR_AP_PREFIX)
                                        : I18n::t(I18n::STR_NETWORK_PREFIX);
    Display::text(Display::MARGIN_X, y, (prefix + WebUI::ssid()).c_str());
    y += lh;
    Display::text(Display::MARGIN_X, y, WebUI::ip().toString().c_str());
    y += lh;
    Display::text(Display::MARGIN_X, y,
                  ("http://" + Config::get().hostname + ".local").c_str());
    y += lh;
    if (WebUI::isAP()) {
      Display::text(Display::MARGIN_X, y,
                    (String(I18n::t(I18n::STR_KEY_PREFIX)) + Config::get().apPass).c_str());
      y += lh;
    }
    y += lh / 2;
    if (Paginator::indexing()) {
      char line[32];
      snprintf(line, sizeof(line), I18n::fmt(I18n::STR_INDEXING_PCT_FMT), Paginator::progress());
      Display::text(Display::MARGIN_X, y, line);
    } else {
      char line[32];
      snprintf(line, sizeof(line), I18n::fmt(I18n::STR_BOOKS_FREE_FMT), Library::count(),
               Library::freeBytes() / 1024);
      Display::text(Display::MARGIN_X, y, line);
    }
  }
  Display::footer(I18n::t(I18n::STR_BACK_WIFI_OFF), "");
  Display::flushFull();
}

void goMenu() {
  g_state = ST_MENU;
  Menu::openLibrary();
  Menu::draw();
}

void startIndexing(const char* id, bool openAfter) {
  if (!Library::exists(id)) return;
  if (!Paginator::startIndexing(id)) {
    showMessage(I18n::t(I18n::STR_COULD_NOT_INDEX));
    return;
  }
  strlcpy(g_openAfterIndex, openAfter ? id : "", sizeof(g_openAfterIndex));
  if (g_state != ST_WIFI) {
    g_state = ST_INDEXING;
    drawIndexing(true);
  }
}

// allowIndex avoids an "open -> index -> open" loop if the index is still no
// good after being rebuilt.
void openBook(const char* id, bool allowIndex = true) {
  if (!Library::exists(id)) {
    goMenu();
    return;
  }
  if (Reader::open(id)) {
    g_state = ST_READING;
    Reader::draw(false);
    return;
  }
  if (allowIndex) {
    startIndexing(id, true);
    return;
  }
  showMessage(I18n::t(I18n::STR_UNREADABLE_BOOK));
}

void onIndexingFinished() {
  char id[Library::ID_LEN];
  strlcpy(id, g_openAfterIndex, sizeof(id));
  g_openAfterIndex[0] = 0;
  Library::refresh();

  if (g_state == ST_WIFI) {
    drawWifi();
    return;
  }
  if (id[0]) {
    openBook(id, /*allowIndex=*/false);
    return;
  }
  goMenu();
}

void toggleWifi() {
  if (WebUI::isActive()) {
    WebUI::stop();
    if (Paginator::indexing()) {
      // An upload is still being indexed: show its progress.
      g_state = ST_INDEXING;
      drawIndexing(true);
    } else if (Reader::isOpen()) {
      g_state = ST_READING;
      Reader::draw(false);
    } else {
      goMenu();
    }
    return;
  }
  Display::clear();
  Display::header(I18n::t(I18n::STR_WIFI));
  Display::text(Display::MARGIN_X, Display::HEADER_BOTTOM + 20 + Display::fontAscent(),
                I18n::t(I18n::STR_CONNECTING));
  Display::flushFull();
  WebUI::begin();
#if UBIBLION_DEV
  Serial.println("[boot] wifi up, drawing wifi screen");
#endif
  g_state = ST_WIFI;
  drawWifi();
}

void showScreensaver() {
  Reader::flushProgress();
  g_stateBeforeSaver = g_state;
  g_state = ST_SCREENSAVER;
  Display::fullScreenImage(Screensavers::kImages[g_nextSaver]);
  Display::flushFull();
  g_nextSaver = (g_nextSaver + 1) % Screensavers::COUNT;
}

// Back to whatever was on screen. The menu keeps its own position.
void leaveScreensaver() {
  g_state = g_stateBeforeSaver;
  if (g_state == ST_READING) {
    Reader::draw(false);
  } else if (g_state == ST_MENU) {
    Menu::draw();
  } else {
    goMenu();
  }
}

void handleReading(const Input::Event& ev) {
  switch (ev.button) {
    case Input::BTN_PREV:
      if (Reader::move(ev.longPress ? -10 : -1)) Reader::draw(true);
      break;
    case Input::BTN_NEXT:
      if (Reader::move(ev.longPress ? 10 : 1)) Reader::draw(true);
      break;
    case Input::BTN_OK:
      Reader::flushProgress();
      if (ev.longPress) {
        g_state = ST_MENU;
        Menu::openSettings();
        Menu::draw();
      } else {
        goMenu();
      }
      break;
    case Input::BTN_BACK:
      Reader::flushProgress();
      goMenu();
      break;
    default:
      break;
  }
}

void handleMenu(const Input::Event& ev) {
  char id[Library::ID_LEN] = {0};
  switch (Menu::handle(ev, id, sizeof(id))) {
    case Menu::ACT_MOVE_SELECTION:
      Menu::draw(true);
      break;
    case Menu::ACT_REDRAW:
      Menu::draw();
      break;
    case Menu::ACT_OPEN_BOOK:
      openBook(id);
      break;
    case Menu::ACT_DELETE_BOOK:
      if (Reader::isOpen() && strcmp(Reader::meta().id, id) == 0) Reader::close();
      if (Config::get().currentBook == id) {
        Config::get().currentBook = "";
        Config::save();
      }
      Library::remove(id);
      Menu::draw();
      break;
    case Menu::ACT_REINDEX_BOOK:
      Reader::close();
      startIndexing(id, true);
      break;
    case Menu::ACT_BACK_TO_READER:
      if (Reader::isOpen()) {
        g_state = ST_READING;
        Reader::draw(false);
      } else if (Config::get().currentBook.length()) {
        openBook(Config::get().currentBook.c_str());
      } else {
        goMenu();
      }
      break;
    case Menu::ACT_TOGGLE_WIFI:
      toggleWifi();
      break;
    case Menu::ACT_FONT_CHANGED:
      // The line wrapping changed: every index is now stale and gets rebuilt
      // when its book is opened. Reindex the one being read right away so
      // the user doesn't wait twice.
      Reader::close();
      if (Config::get().currentBook.length()) {
        startIndexing(Config::get().currentBook.c_str(), false);
      } else {
        Menu::draw();
      }
      break;
    default:
      break;
  }
}

void handleIndexing(const Input::Event& ev) {
  if (ev.button == Input::BTN_BACK) {
    Paginator::cancel();
    g_openAfterIndex[0] = 0;
    goMenu();
  }
}

// WiFi only lives as long as its screen: leaving it turns the radio off, so it
// can never stay on in the background draining the battery.
void handleWifi(const Input::Event& ev) {
  if (ev.button == Input::BTN_BACK) {
    toggleWifi();  // turns it off and goes back
    return;
  }
#if UBIBLION_DEV
  if (ev.button == Input::BTN_OK) {
    // Development only, so the web panel can drive the other screens: leaves
    // WiFi on and goes back to reading or to the library.
    if (Reader::isOpen()) {
      g_state = ST_READING;
      Reader::draw(false);
    } else {
      goMenu();
    }
  }
#endif
}

void dispatch(const Input::Event& ev) {
  // Long BACK shows the screensaver. Indexing and WiFi treat BACK as
  // cancel/exit however long it's held.
  if (ev.button == Input::BTN_BACK && ev.longPress &&
      (g_state == ST_READING || g_state == ST_MENU || g_state == ST_MESSAGE)) {
    showScreensaver();
    return;
  }
  switch (g_state) {
    case ST_READING:
      handleReading(ev);
      break;
    case ST_MENU:
      handleMenu(ev);
      break;
    case ST_INDEXING:
      handleIndexing(ev);
      break;
    case ST_WIFI:
      handleWifi(ev);
      break;
    case ST_MESSAGE:
      goMenu();
      break;
    case ST_SCREENSAVER:
      leaveScreensaver();  // the press only wakes: it does nothing else
      break;
  }
}

void splash() {
  Display::clear();
  Display::setFont(Display::FONT_LARGE);
  Display::text(Display::MARGIN_X, 90, "ubiblion");
  Display::setFont(static_cast<Display::FontId>(Config::get().fontId));
  Display::text(Display::MARGIN_X, 115, "v" UBIBLION_VERSION);
  Display::flushFull();
}

}  // namespace

void setup() {
  Power::begin();

  // On the ESP8266, TX-only frees up GPIO3 (RX) for the OK button; the
  // ESP32 keeps the port full-duplex.
  ubSerialBegin();
  Serial.println();
  Serial.println(F("[boot] ubiblion " UBIBLION_VERSION));

  // The radio starts off: it only comes on when actually needed.
  WiFi.persistent(false);
  ubWiFiRadioOff();

#if UBIBLION_DEV && defined(ARDUINO_ARCH_ESP32)
  // Pinned to core 0: loopTask (and everything it might block) runs on core
  // 1, so this watcher keeps printing even while core 1 is wedged.
  xTaskCreatePinnedToCore(stallWatcher, "stallWatch", 2048, nullptr, 1, nullptr, 0);
#endif

  Library::begin();
  Config::begin();
  I18n::begin();

  Display::begin();
  Display::setFont(static_cast<Display::FontId>(Config::get().fontId));
  Display::setFullRefreshEvery(Config::get().fullRefreshEvery);
  Input::begin();

  splash();
  delay(600);
#if UBIBLION_DEV
  Serial.println("[boot] splash done");
#endif

  const String& current = Config::get().currentBook;
  uint8_t wifiRule = Config::get().wifiOnBoot;
#if UBIBLION_DEV
  // Development mode always brings up the network, regardless of
  // config.json: it's what lets you rescue a device that's been left
  // unreachable.
  wifiRule = Config::WIFI_BOOT_ALWAYS;
#endif
  if (wifiRule == Config::WIFI_BOOT_ALWAYS ||
      (wifiRule == Config::WIFI_BOOT_IF_EMPTY && Library::count() == 0)) {
    Serial.printf("[boot] turning WiFi on (rule %u, %u books)\n", wifiRule, Library::count());
    toggleWifi();
  } else if (current.length() && Library::exists(current.c_str())) {
    openBook(current.c_str());
  } else {
    goMenu();
  }
}

void loop() {
#if UBIBLION_DEV && defined(ARDUINO_ARCH_ESP32)
  g_loopTick = millis();
#endif
  Input::Event ev;
  if (Input::poll(ev)) {
    Serial.printf("[in] %s%s\n", Input::name(ev.button), ev.longPress ? " (long)" : "");
    dispatch(ev);
  }

  if (Paginator::indexing()) {
    if (Paginator::step()) {
      onIndexingFinished();
    } else if (g_state == ST_INDEXING) {
      const uint8_t pct = Paginator::progress();
      if (pct >= g_lastProgress + 5 || g_lastProgress == 255) drawIndexing(false);
    }
  }

  WebUI::loop();

  if (WebUI::takeLibraryChanged()) {
    Library::refresh();
    if (g_state == ST_MENU) Menu::draw();
    if (g_state == ST_WIFI) drawWifi();
  }

  char pending[Library::ID_LEN];
  if (WebUI::takePendingIndex(pending, sizeof(pending))) {
    startIndexing(pending, /*openAfter=*/false);
  }

  Reader::tick();

  if (!WebUI::isActive() && !Paginator::indexing() && Input::quiet()) {
    Power::sleep(Reader::msUntilSave());
  }
  yield();
}
