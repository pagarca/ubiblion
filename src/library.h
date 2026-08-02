// Book management on LittleFS.
//
// Each book is three files sharing a short id (b0001, ...):
//   /books/<id>.txt   plain UTF-8 text
//   /books/<id>.idx   binary page index (built by Paginator)
//   /books/<id>.json  metadata and reading progress
#pragma once

#include <Arduino.h>

namespace Library {

static const uint8_t MAX_BOOKS = 40;
static const uint8_t ID_LEN = 6;  // "b0001" + '\0'

struct BookMeta {
  char id[ID_LEN] = {0};
  String title;
  String author;
  uint32_t bytes = 0;       // size of the .txt file
  uint32_t pages = 0;       // 0 = not indexed yet
  uint32_t page = 0;        // current page (0-based)
  uint32_t idxVersion = 0;  // layout version the .idx was built with
  uint32_t added = 0;       // millis() at creation (relative ordering)
};

void begin();

// Rereads the /books directory. Called automatically after additions and
// removals.
void refresh();

uint8_t count();
// Access by position in the listing (sorted by ascending id).
bool at(uint8_t index, BookMeta& out);
const char* idAt(uint8_t index);
int16_t indexOf(const char* id);

bool exists(const char* id);
bool load(const char* id, BookMeta& out);
bool save(const BookMeta& meta);
bool remove(const char* id);

// Reserves the next free id. Returns "" if there's no room left.
String nextId();

String textPath(const char* id);
String idxPath(const char* id);
String metaPath(const char* id);

uint32_t freeBytes();
uint32_t totalBytes();
uint32_t usedBytes();

}  // namespace Library
