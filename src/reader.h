// Reading screen: keeps track of the open book, the current page and the
// deferred progress save.
#pragma once

#include <Arduino.h>

#include "library.h"

namespace Reader {

// Loads the book. Returns false if it doesn't exist or its index isn't good
// for the current layout (in which case it needs reindexing before trying
// again).
bool open(const char* id);
void close();
bool isOpen();

const Library::BookMeta& meta();
uint32_t page();
uint32_t pages();

// Moves `delta` pages (clamped at both ends) and returns true if it changed.
bool move(int32_t delta);
bool goPage(uint32_t page);

void draw(bool partial);

// Flushes progress to /books/<id>.json once there's been no activity for a
// while.
void tick();
// Milliseconds until tick() has something to save; 0 if nothing is pending.
uint32_t msUntilSave();
void flushProgress();

}  // namespace Reader
