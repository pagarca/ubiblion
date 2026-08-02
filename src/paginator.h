// Pagination of the plain-text books.
//
// The tricky part of this project: over a .txt file you can't go back a page
// without recomputing the line wrap from scratch, so as soon as a book is
// added a binary index is built with the starting offset of every page. From
// then on, jumping to page N is a direct read.
//
// /books/<id>.idx layout:
//   [0..3]  "UBIX" magic
//   [4..7]  layout version (font + width + lines per page)
//   [8..]   little-endian uint32 with the starting offset of each page
//
// Building it is incremental (step(), called from the main loop) so it
// doesn't trip the watchdog on large books.
#pragma once

#include <Arduino.h>

#include "library.h"

namespace Paginator {

static const uint8_t MAX_LINES = 14;

// Identifies the current line-wrap settings. If the font changes, the
// existing index stops being valid and needs rebuilding.
uint32_t layoutVersion();
bool indexValid(const Library::BookMeta& meta);

// --- incremental indexing ---
bool startIndexing(const char* id);
bool indexing();
const char* indexingId();
uint8_t progress();  // 0..100
uint32_t indexedPages();
// Processes a batch of pages. Returns true once indexing has finished.
bool step();
void cancel();

// --- reading ---
uint32_t pageCount(const char* id);
// Returns the number of lines written into `lines`.
uint8_t loadPage(const char* id, uint32_t page, String* lines, uint8_t maxLines);

}  // namespace Paginator
