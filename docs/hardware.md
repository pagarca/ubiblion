# Hardware

[← Back to the README](../README.md)

Wiring charts are in the [README](../README.md#hardware). They are generated from `src/pins.h` by
`tools/make_pinout.py` (don't edit the SVGs by hand).

## ESP8266 (NodeMCU v3)

- **GPIO0 (D3) and GPIO2 (D4) are boot-strapping pins** and must be high at reset. The internal
  pull-up keeps them there; it only fails if that button is held down during reset.
- **GPIO3 is RX**: the firmware opens the serial port in `SERIAL_TX_ONLY` mode to free it up, so logs
  still come out but nothing can be sent to the device over serial. Since esptool does transmit over
  RX while flashing, **don't hold OK down while uploading firmware**.
- For the same reason, `GxEPD2` is initialized with `epd.init(0, ...)`: a nonzero diagnostic bitrate
  would call `Serial.begin()` in full-duplex mode and steal GPIO3 right back.

`A0` is left free for measuring the battery once the build moves to LiPo.

## ESP32 (DevKitC / NodeMCU-32S, 4MB)

The firmware expects LOW at rest and HIGH on press. The button circuit must
hold each pin low when released because the firmware enables the internal pull-up.

Differences from the ESP8266 worth knowing:

- The serial port stays **full-duplex**: GPIO3 is not a button here, so nothing is ever stolen and
  logs work both ways.
- Build with the `esp32dev` or `dev_esp32` environments (see [Development](development.md)); the flash
  is split by `partitions_ubiblion.csv` (1MB app + ~2.94MB LittleFS). The binary weighs ~960KB, so
  the app partition runs at ~92% — it fits, but if a future build outgrows 1MB, trim 256KB off the
  littlefs row in the CSV first.
- GPIO16/17 are used by the display: fine on the WROOM-32 dev kits, not on a WROVER (they're the
  PSRAM pins).
- GPIO12 is left free but straps the flash voltage at reset — keep a button on it *unpressed* at
  boot, or better, don't wire it. All the 34–39 input-only pins are free for the battery ADC (they
  have no internal pull-ups, so not for buttons).

## Flash layout

The 4MB flash is split into ~1MB of program and ~3MB of LittleFS on both targets -- via
`eagle.flash.4m3m.ld` on the ESP8266 and `partitions_ubiblion.csv` on the ESP32 -- see the comment
above `board_build.ldscript` in `platformio.ini` for why giving the filesystem the lion's share costs
nothing. That leaves room for a real library: a 300KB text file is around 600 pages, and the
unabridged *Don Quijote* (2.1MB of plain text, 11475 pages) uses well under half the free space.

## Power

- The ESP32 build runs at 80MHz and light-sleeps between button presses whenever WiFi is
  off and no book is being indexed. WiFi stays on (and the chip awake) while its screen is open. On a
  dev board the regulator, USB-UART chip and power LED keep drawing current while the chip sleeps,
  which limits battery life.
- The ESP8266 build doesn't sleep yet: light sleep with a GPIO wakeup fits the four buttons, while
  deep sleep would need GPIO16 wired to RST, and that's where BUSY currently is.

## Tested on real hardware

Tested on real NodeMCU v3 hardware: display, LittleFS, WiFi, REST API, uploading books from a 227KB
sample up to the unabridged *Don Quijote* (2.1MB, 11475 pages), indexing, navigation and deletion. The
only thing still unverified on hardware is the four physical buttons, since they aren't wired up yet;
their logic has been exercised with development mode's virtual buttons.

Also brought up and verified on real ESP32 hardware (DevKitC, WROOM-32): the display through the new
pin map (splash, WiFi screen, full refreshes), the LittleFS partition, AP first boot and STA
connection, the REST API (status, library, upload with in-place EPUB-less text indexing of 300+ pages,
byte-exact book download), UTF-8 pagination with accents, and the development-mode screen mirror.
Since version 1.0 it has been in use on that board with the four physical buttons wired, including light
sleep between presses.
