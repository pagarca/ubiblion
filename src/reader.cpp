#include "reader.h"

#include "config.h"
#include "display.h"
#include "i18n.h"
#include "input.h"
#include "paginator.h"

namespace Reader {
namespace {

// Writing to flash on every page turn would wear the chip out, so progress
// is flushed once the user has been idle for a while.
const uint32_t kSaveIdleMs = 10000;

Library::BookMeta g_meta;
bool g_open = false;
bool g_dirty = false;
String g_lines[Paginator::MAX_LINES];

}  // namespace

bool open(const char* id) {
  close();
  if (!id || !*id) return false;
  if (!Library::load(id, g_meta)) return false;
  if (!Paginator::indexValid(g_meta)) return false;

  g_meta.pages = Paginator::pageCount(id);
  if (g_meta.pages == 0) return false;
  if (g_meta.page >= g_meta.pages) g_meta.page = g_meta.pages - 1;

  g_open = true;
  g_dirty = false;
  Config::get().currentBook = id;
  Config::save();
  return true;
}

void close() {
  if (g_open) flushProgress();
  g_open = false;
}

bool isOpen() { return g_open; }

const Library::BookMeta& meta() { return g_meta; }
uint32_t page() { return g_meta.page; }
uint32_t pages() { return g_meta.pages; }

bool goPage(uint32_t page) {
  if (!g_open || g_meta.pages == 0) return false;
  if (page >= g_meta.pages) page = g_meta.pages - 1;
  if (page == g_meta.page) return false;
  g_meta.page = page;
  g_dirty = true;
  return true;
}

bool move(int32_t delta) {
  if (!g_open || g_meta.pages == 0) return false;
  int64_t target = static_cast<int64_t>(g_meta.page) + delta;
  if (target < 0) target = 0;
  if (target >= g_meta.pages) target = g_meta.pages - 1;
  return goPage(static_cast<uint32_t>(target));
}

void draw(bool partial) {
  if (!g_open) return;

  // g_lines is the hard cap: linesPerPage() should never exceed it, but a
  // future font change must not be able to overflow the array.
  uint8_t maxLines = Display::linesPerPage();
  if (maxLines > Paginator::MAX_LINES) maxLines = Paginator::MAX_LINES;
  const uint8_t n = Paginator::loadPage(g_meta.id, g_meta.page, g_lines, maxLines);

  Display::clear();
  Display::header(g_meta.title);

  int16_t y = Display::TEXT_TOP + Display::fontAscent();
  if (n == 0) {
    Display::text(Display::MARGIN_X, y, I18n::t(I18n::STR_EMPTY_PAGE));
  }
  for (uint8_t i = 0; i < n; i++) {
    if (g_lines[i].length()) Display::text(Display::MARGIN_X, y, g_lines[i].c_str());
    y += Display::lineHeight();
  }

  char left[24];
  snprintf(left, sizeof(left), "%u/%u", g_meta.page + 1, g_meta.pages);
  char right[8];
  const uint32_t pct = g_meta.pages > 1 ? (g_meta.page * 100) / (g_meta.pages - 1) : 100;
  snprintf(right, sizeof(right), "%u%%", pct);
  Display::footer(left, right);

  Display::flush(partial);
}

void tick() {
  if (!g_open || !g_dirty) return;
  if (Input::idleMillis() < kSaveIdleMs) return;
  flushProgress();
}

void flushProgress() {
  if (!g_open || !g_dirty) return;
  Library::BookMeta stored;
  if (Library::load(g_meta.id, stored)) {
    stored.page = g_meta.page;
    Library::save(stored);
  }
  g_dirty = false;
}

}  // namespace Reader
