// Platform shims so the rest of the code can pretend it targets one chip.
//
// The build decides the real target via the platform's own macro
// (ARDUINO_ARCH_ESP32 vs. the ESP8266 core); no UBIBLION-* flag needs to
// know about it. Only what actually differs between the two cores lives
// here:
//
//   - WiFi/WebServer/mDNS headers and the server class name
//   - WiFi.forceSleepWake()/forceSleepBegin(): ESP8266-only radio power
//     management; on ESP32 WiFi.mode(WIFI_OFF) is all there is, so the
//     wrappers are no-ops
//   - MDNS.update(): the ESP8266 core needs it pumped; the ESP32 one
//     doesn't even have it
//   - Serial: on the ESP8266 the OK button lives on RX (GPIO3), so the
//     port opens in TX-only mode; ESP32 builds keep full-duplex serial
//   - collectHeaders(): the ESP8266 core wants the variadic form (the
//     classic array+size one is ambiguous there, see webserver.cpp); the
//     ESP32 core 2.x takes array+size
#pragma once

#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP32

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <uri/UriBraces.h>  // same path as the ESP8266 core

using UBWebServer = WebServer;

// ESP32 core 2.x signature: collectHeaders(const char* headerKeys[],
// const size_t). The ESP8266 core's array overload takes String[] instead,
// so the two can't share one shim body.
inline void ubCollectHeaders(UBWebServer& server, const char* name1, const char* name2) {
  const char* names[2] = {name1, name2};
  server.collectHeaders(names, 2);
}

// Full radios off: mode(WIFI_OFF) already does everything there is to do.
inline void ubWiFiRadioOff() {
  WiFi.mode(WIFI_OFF);
}
inline void ubWiFiRadioWake() {}

// The ESP32 mDNS responder services queries internally; nothing to pump.
inline void ubMDNSUpdate() {}

// No button shares the serial RX pin, so full-duplex is safe.
inline void ubSerialBegin() { Serial.begin(115200); }

#else  // ESP8266

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <uri/UriBraces.h>

using UBWebServer = ESP8266WebServer;

// ESP8266 core: variadic overload (an explicit array+size call is
// ambiguous against it, which is why webserver.cpp started variadic).
// Raw literals instead of F(): two short header names aren't worth the
// indirection through a shim that can't take __FlashStringHelper by value.
template <typename... Args>
inline void ubCollectHeaders(UBWebServer& server, Args... names) {
  server.collectHeaders(names...);
}

inline void ubWiFiRadioOff() {
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
}
inline void ubWiFiRadioWake() {
  WiFi.forceSleepWake();
  delay(1);
}
inline void ubMDNSUpdate() { MDNS.update(); }

// TX-only keeps GPIO3 (RX) free for the OK button. Logs still work;
// nothing can be sent to the device over serial, though.
inline void ubSerialBegin() { Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY); }

#endif  // ARDUINO_ARCH_ESP32
