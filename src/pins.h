// NodeMCU v3 (ESP-12E) pin assignments. See README.md for the full diagram.
//
// The display uses the same pinout as WeAct's official ESP8266 example.
// The buttons occupy GPIO12, GPIO0, GPIO3 and GPIO2:
//   - GPIO0 and GPIO2 are boot-strapping pins: they must be high at reset,
//     which INPUT_PULLUP guarantees as long as they aren't held down.
//   - GPIO3 is RX: it's freed up by opening the serial port in
//     SERIAL_TX_ONLY mode.
#pragma once

#include <Arduino.h>

// --- e-paper (hardware SPI: SCK=GPIO14/D5, MOSI=GPIO13/D7) ---
static const int8_t PIN_EPD_BUSY = 16;  // D0
static const int8_t PIN_EPD_RST = 5;    // D1
static const int8_t PIN_EPD_DC = 4;     // D2
static const int8_t PIN_EPD_CS = 15;    // D8

// --- buttons (to GND, internal pull-up) ---
static const uint8_t PIN_BTN_PREV = 12;  // D6
static const uint8_t PIN_BTN_NEXT = 0;   // D3
static const uint8_t PIN_BTN_OK = 3;     // RX
static const uint8_t PIN_BTN_BACK = 2;   // D4
