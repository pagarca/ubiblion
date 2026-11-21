// Pin assignments for the two supported build targets. Whichever block is
// active wins; the other is compiled out. tools/make_pinout.py parses this
// file to generate the SVG wiring charts in docs/, so the on-die comment
// labels (the "D0" / "GPIO17" parts) are what those charts display.
#pragma once

#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP32

// ESP32 DevKitC / NodeMCU-32S (WROOM-32, 4MB, 30-pin dev board).
//
// The display rides the VSPI bus with its default SCK/MOSI. The button
// circuit holds each pin low at rest and presents a high level on press.
// None of these four are boot-strapping pins, and the serial port stays
// full-duplex (no GPIO3-as-button trick needed here).
//
// Free afterwards: GPIO12 (straps flash voltage at reset, keep unpressed at
// boot), all the 34-39 input-only pins for the battery ADC, and 32/33.
// --- e-paper (hardware SPI: SCK=GPIO18, MOSI=GPIO23) ---
static const uint8_t PIN_EPD_SCK = 18;   // GPIO18
static const uint8_t PIN_EPD_MOSI = 23;  // GPIO23
static const uint8_t PIN_EPD_BUSY = 17;  // GPIO17
static const uint8_t PIN_EPD_RST = 16;   // GPIO16
static const uint8_t PIN_EPD_DC = 4;     // GPIO4
static const uint8_t PIN_EPD_CS = 5;     // GPIO5

// --- buttons (active high, internal pull-up retained) ---
static const uint8_t PIN_BTN_PREV = 13;  // GPIO13
static const uint8_t PIN_BTN_NEXT = 14;  // GPIO14
static const uint8_t PIN_BTN_OK = 27;    // GPIO27
static const uint8_t PIN_BTN_BACK = 25;  // GPIO25
#else

// NodeMCU v3 (ESP-12E) pin assignments. See README.md for the full diagram.
//
// The display uses the same pinout as WeAct's official ESP8266 example.
// The buttons occupy GPIO12, GPIO0, GPIO3 and GPIO2:
//   - GPIO0 and GPIO2 are boot-strapping pins: they must be high at reset,
//     which INPUT_PULLUP guarantees as long as they aren't held down.
//   - GPIO3 is RX: it's freed up by opening the serial port in
//     SERIAL_TX_ONLY mode.
// --- e-paper (hardware SPI: SCK=GPIO14/D5, MOSI=GPIO13/D7) ---
static const uint8_t PIN_EPD_SCK = 14;   // D5
static const uint8_t PIN_EPD_MOSI = 13;  // D7
static const int8_t PIN_EPD_BUSY = 16;   // D0
static const int8_t PIN_EPD_RST = 5;     // D1
static const int8_t PIN_EPD_DC = 4;      // D2
static const int8_t PIN_EPD_CS = 15;     // D8

// --- buttons (to GND, internal pull-up) ---
static const uint8_t PIN_BTN_PREV = 12;  // D6
static const uint8_t PIN_BTN_NEXT = 0;   // D3
static const uint8_t PIN_BTN_OK = 3;     // RX
static const uint8_t PIN_BTN_BACK = 2;   // D4

#endif
