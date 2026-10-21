// WiFi on demand + REST API + web UI.
//
// The radio is off while reading (saves ~70mA and leaves more heap free for
// rendering); it's turned on from the menu or with a long BACK press.
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#include "library.h"

namespace WebUI {

void begin();  // turns WiFi on (STA, or AP if that fails) and starts the server
void stop();   // stops the server and turns the radio off
bool isActive();
void loop();

bool isAP();
IPAddress ip();
String ssid();
String url();

// An upload leaves the book ready but not indexed: the main loop picks up
// the pending id here and kicks off Paginator.
bool takePendingIndex(char* out, size_t len);

// Marks that the library changed (an addition or removal), so the UI knows
// to refresh.
bool takeLibraryChanged();

}  // namespace WebUI
