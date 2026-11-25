# Development

[← Back to the README](../README.md)

## Build environments

| Environment | What it builds |
|---|---|
| `nodemcuv2` | ESP8266 |
| `nodemcuv2_en` | ESP8266, English only |
| `esp32dev` | ESP32 |
| `dev`, `dev_esp32` | Development mode on the ESP8266 and the ESP32 |

Always pass `-e`: without it PlatformIO runs every environment in turn.

## Development mode

```bash
pio run -e dev -t upload        # ESP8266
pio run -e dev_esp32 -t upload  # ESP32
```

The `dev`/`dev_esp32` environments compile the same firmware with
`-DUBIBLION_DEV=1` and add:

- **WiFi always on at boot**, ignoring `wifiOnBoot`. It's also the rescue path if a device becomes
  unreachable (with books loaded and no buttons, there's otherwise no way to turn the network back on).
- **WiFi can stay on**: on the WiFi screen, **OK** leaves it with the radio still on, so the web panel
  can drive the other screens.
- **Screen mirror**: `GET /api/debug/screen` returns the text that's painted on screen, with its
  coordinates. Lets you see what the e-ink display shows without having it in front of you.
- **Virtual buttons**: `POST /api/debug/press?button=PREV|NEXT|OK|BACK&long=0|1` injects a press.
  Useful for driving the whole device from the browser before wiring up the real buttons.
- **Button log**: `GET /api/debug/input` lists the latest events, distinguishing real presses from
  injected ones, which is how you check the wiring without watching the serial port.
- **Page inspector**: `GET /api/debug/page?id=b0001&page=7` returns that page's lines with their pixel
  width, exactly as the reader computes them.

The web UI detects the `dev` firmware from the `dev` field in `/api/status` and shows a panel with all
of the above. None of these endpoints exist in the normal build.

## Layout tests

`tools/test_layout.py` checks the line-wrapping invariants without hardware (see
[Internals](internals.md#pagination)):

```bash
python tools/test_layout.py
```

## Languages

Two independent, separately-configured translation systems, since they live in different places with
different constraints:

- **The reader's own screen** (menus, headers, footers) is compiled into the firmware binary, which
  has a tight flash budget. `src/i18n.h`/`i18n.cpp` hold one PROGMEM string table per language; which
  ones actually get compiled in is controlled by the `UBIBLION_LANG_EN/ES/FR/DE/IT/CA` flags in
  `platformio.ini` (all on by default). Compiling in all six costs a few KB of flash over an
  English-only build — cheap on this board's 4MB chip, but the flags exist to trim it down on tighter
  budgets. See `[env:nodemcuv2_en]` in `platformio.ini` for a worked single-language example. When more
  than one language is compiled in, a **Language** row shows up in the on-device Settings menu to
  cycle between them (persisted, same as every other setting); it's simply absent when only one is
  built in, since there'd be nothing to switch to.
- **The web UI** (`data/index.html`) lives in LittleFS, not in the firmware, so its flash budget is
  the spacious ~3MB filesystem partition, not the tight program one — six languages of UI labels there
  cost on the order of 18KB, negligible next to the books themselves. It defaults to the browser's own
  language (falling back to English) and can be switched from a selector at the top of the page,
  entirely independent of whatever the device's own screen is set to; the choice is remembered in
  `localStorage`. See "Web UI internals" below for how it's built and how to trim its language set too.

Adding a language to either system means adding one translated file (a `src/i18n.cpp` language block,
or a `data/i18n/<code>.json` file) with the same set of keys as an existing one — nothing else needs
to change.

## Web UI internals

`data/index.html` — the file actually served by the device and uploaded by `pio run -e <env> -t uploadfs` — is
**generated**, not edited directly. The maintained source is `data/index.template.html` (all the
HTML/CSS/JS, with `data-i18n="key"` attributes on translatable elements and `t("key", vars)` calls in
the JavaScript) plus `data/i18n/<code>.json`, one flat key→string dictionary per language.

`tools/build_web.py` bundles the selected languages' JSON into the template's `%%I18N_JSON%%`
placeholder and writes `data/index.html`. PlatformIO runs it automatically before every build (see
`extra_scripts` in `platformio.ini`), so `uploadfs` always uploads a fresh file — there's
no separate step to remember. Which languages get bundled is controlled by the `UBIBLION_WEB_LANGS`
environment variable, a comma-separated list of codes (default: every file found in `data/i18n/`):

```bash
UBIBLION_WEB_LANGS=en,es pio run -e esp32dev -t uploadfs
```

Trimming it down keeps the unused languages' strings out of the LittleFS image; on this project that's
only a few KB per language, so it mostly matters if you're already tight on filesystem space for other
reasons. The page picks its own language independently at runtime (browser language, overridable from
a selector, remembered in `localStorage`) — that's separate from which languages were baked in at
build time, and separate from the device's own on-screen menu language (see "Languages" above).

## User manuals

The PDFs in `docs/manuals/` are generated, one per language, from `tools/manual/`:

```bash
pip install reportlab svglib
python tools/manual/build.py
```

The layout and the Spanish text live in `generate.py`, the other languages in `translations.txt`. The
script uses the Arial fonts shipped with Windows.
