// Menus: library, settings and delete confirmation.
#pragma once

#include <Arduino.h>

#include "input.h"

namespace Menu {

enum Action : uint8_t {
  ACT_NONE = 0,
  ACT_REDRAW,          // redraw without changing screens
  ACT_OPEN_BOOK,       // outId holds the book's id
  ACT_DELETE_BOOK,     // outId holds the book's id
  ACT_REINDEX_BOOK,    // outId holds the book's id
  ACT_BACK_TO_READER,  // return to reading (if a book is open)
  ACT_TOGGLE_WIFI,
  ACT_FONT_CHANGED,  // layout changed: whatever gets opened needs reindexing
};

void openLibrary();
void openSettings();
bool isOpen();

void draw();
Action handle(const Input::Event& ev, char* outId, size_t outIdLen);

}  // namespace Menu
