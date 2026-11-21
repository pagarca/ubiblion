#!/usr/bin/env python3
"""Generates the wiring charts (SVG) for the supported boards -- README.md
embeds them.

Instead of duplicating the pin assignments, this reads the two conditional
blocks of src/pins.h directly, so the SVGs can never drift from the firmware
they document. After touching src/pins.h, run:

    python tools/make_pinout.py            # all boards
    python tools/make_pinout.py --board esp32dev

Output: docs/pinout-<board>.svg, one colored table per board. Standard
library only.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PINS_H = ROOT / "src" / "pins.h"

BOARDS = {
    "nodemcuv2": "NodeMCU v3 (ESP8266, ESP-12E)",
    "esp32dev": "ESP32 DevKitC / NodeMCU-32S (WROOM-32)",
}

# Matrix-style row order and titles. Rows not listed here get appended at
# the end under an "Other" group, so a new PIN_* added to pins.h without a
# matching entry still shows up.
SECTIONS = {
    "e-paper": ["PIN_EPD_SCK", "PIN_EPD_MOSI", "PIN_EPD_CS", "PIN_EPD_DC", "PIN_EPD_RST", "PIN_EPD_BUSY"],
    "buttons": ["PIN_BTN_PREV", "PIN_BTN_NEXT", "PIN_BTN_OK", "PIN_BTN_BACK"],
}
SECTION_TITLES = {
    "e-paper": "e-paper (SCK/MOSI: hardware SPI bus)",
    "buttons": "buttons (pin to GND, internal pull-up)",
}

# Signal -> [text fill, row fill, stroke], per kind.
COLORS = {
    "e-paper": ("#5b2a86", "#f0e8fb"),   # violet: data/clock (SPI) rows are #f5fbf8? keep one
    "buttons": ("#1a6b3c", "#e8f7ee"),   # green
    "other": ("#444444", "#f2f2f2"),
}
SPI_ROW = ("#3d2075", "#eef0fb")  # slightly bluer row for the shared SPI pins
CTRL_ROW = ("#7a3f00", "#fdf3e8")  # amber row for CS/DC/RST/BUSY


def kind_of(name: str) -> str:
    if name.startswith("PIN_EPD_"):
        if name in ("PIN_EPD_SCK", "PIN_EPD_MOSI"):
            return "e-paper-spi"
        return "e-paper-ctrl"
    if name.startswith("PIN_BTN_"):
        return "e-paper-buttons"
    return "other"


ROW_STYLE = {
    "e-paper-spi": SPI_ROW,
    "e-paper-ctrl": CTRL_ROW,
    "e-paper-buttons": COLORS["buttons"],
    "other": COLORS["other"],
}


def parse_pins_h():
    """Returns {board: {PIN_NAME: (pin, label)}} from the two blocks of
    src/pins.h. The #ifdef/#else structure is mirrored: the ESP32 branch
    feeds 'esp32dev', the #else branch feeds 'nodemcuv2'."""

    line_re = re.compile(
        r"^\s*static const (int8_t|uint8_t)\s+"
        r"(PIN_[A-Z0-9_]+)\s*=\s*(-?\d+)\s*;"
        r"(?:\s*/\*\s*(.*?)\s*\*/)?"
        r"(?:\s*//\s*(.*))?"
    )

    boards = {"esp32dev": {}, "nodemcuv2": {}}
    stack = []  # each entry: (condition, which) where which in {esp32dev, nodemcuv2, skip}
    active = {"nodemcuv2"}
    target_map = {"esp32": "esp32dev", "else": None}

    def resolve(stack):
        """Which board is being edited at this #if nesting level."""
        for cond, which in reversed(stack):
            if which is not None:
                return which
        return None

    at_else = {}  # depth -> already switched once

    for raw in PINS_H.read_text(encoding="utf-8").splitlines():
        stripped = raw.strip()
        depth = len(stack)
        if stripped.startswith("#ifdef ARDUINO_ARCH_ESP32"):
            stack.append(("esp32", "esp32dev"))
            continue
        if stripped.startswith("#else") and stack:
            depth = len(stack)
            cur = stack[-1]
            if cur == ("esp32", "esp32dev"):
                stack[-1] = ("else", "nodemcuv2")
            continue
        if stripped.startswith("#endif") and stack:
            stack.pop()
            continue

        m = line_re.match(raw)
        if not m:
            continue
        board = resolve(stack)
        if board is None:
            continue
        pin_name, pin, label = m.group(2), int(m.group(3)), m.group(5)
        if not label:
            continue
        boards[board][pin_name] = (pin, label.strip())

    return boards


def esc(text):
    return (
        text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    )


def svg_for(board, pins):
    title = BOARDS[board]
    row_h = 26
    head_h = 30
    pad = 20
    title_h = 52
    width = 560

    used = set()
    body = []
    y = title_h
    for section, order in SECTIONS.items():
        section_title = SECTION_TITLES[section]
        if section == "buttons" and board == "esp32dev":
            section_title = "buttons (LOW at rest, HIGH on press)"
        rows = [(n, pins[n]) for n in order if n in pins]
        used.update(n for n, _ in rows)
        if not rows:
            continue
        # section header band
        body.append(
            f'<rect x="{pad}" y="{y}" width="{width - 2*pad}" height="{row_h}" fill="#1b1f24"/>')
        body.append(
            f'<text x="{pad+12}" y="{y+row_h-8}" font-family="DejaVu Sans, Arial, sans-serif" '
            f'font-size="13" font-weight="bold" fill="#ffffff">{esc(section_title)}</text>')
        y += row_h
        # column header
        body.append(
            f'<rect x="{pad}" y="{y}" width="{width - 2*pad}" height="{row_h}" fill="#eef0f3"/>')
        for cx, label in ((16, "signal"), (300, "pin"), (400, "GPIO")):
            body.append(
                f'<text x="{pad+cx}" y="{y+row_h-8}" font-family="DejaVu Sans, Arial, sans-serif" '
                f'font-size="12" font-weight="bold" fill="#37474f">{label}</text>')
        y += row_h
        for name, (pin, label) in rows:
            txt, row = ROW_STYLE[kind_of(name)]
            body.append(
                f'<rect x="{pad}" y="{y}" width="{width - 2*pad}" height="{row_h}" fill="{row}" stroke="#d0d4d8" stroke-width="1"/>')
            body.append(
                f'<text x="{pad+16}" y="{y+row_h-8}" font-family="DejaVu Sans Mono, Consolas, monospace" '
                f'font-size="13" font-weight="bold" fill="{txt}">{esc(name.replace("PIN_", ""))}</text>')
            body.append(
                f'<text x="{pad+300}" y="{y+row_h-8}" font-family="DejaVu Sans Mono, Consolas, monospace" '
                f'font-size="13" fill="#222222">{esc(label)}</text>')
            body.append(
                f'<text x="{pad+400}" y="{y+row_h-8}" font-family="DejaVu Sans Mono, Consolas, monospace" '
                f'font-size="13" fill="#222222">{pin}</text>')
            y += row_h
        y += 14

    # anything parsed but not in SECTIONS
    extra = [(n, v) for n, v in pins.items() if n not in used]
    if extra:
        body.append(
            f'<rect x="{pad}" y="{y}" width="{width - 2*pad}" height="{row_h}" fill="#1b1f24"/>')
        body.append(
            f'<text x="{pad+12}" y="{y+row_h-8}" font-family="DejaVu Sans, Arial, sans-serif" '
            f'font-size="13" font-weight="bold" fill="#ffffff">other</text>')
        y += row_h
        for name, (pin, label) in extra:
            txt, row = ROW_STYLE["other"]
            body.append(
                f'<rect x="{pad}" y="{y}" width="{width - 2*pad}" height="{row_h}" fill="{row}" stroke="#d0d4d8" stroke-width="1"/>')
            body.append(
                f'<text x="{pad+16}" y="{y+row_h-8}" font-family="DejaVu Sans Mono, Consolas, monospace" '
                f'font-size="13" font-weight="bold" fill="{txt}">{esc(name.replace("PIN_", ""))}</text>')
            body.append(
                f'<text x="{pad+300}" y="{y+row_h-8}" font-family="DejaVu Sans Mono, Consolas, monospace" '
                f'font-size="13" fill="#222222">{esc(label)}</text>')
            body.append(
                f'<text x="{pad+400}" y="{y+row_h-8}" font-family="DejaVu Sans Mono, Consolas, monospace" '
                f'font-size="13" fill="#222222">{pin}</text>')
            y += row_h
        y += 14

    height = y + pad
    ret = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
           f'height="{height}" viewBox="0 0 {width} {height}">']
    ret.append(f'<rect width="{width}" height="{height}" fill="white"/>')
    ret.append(
        f'<text x="{pad}" y="30" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" '
        f'font-weight="bold" fill="#111111">ubiblion wiring - {esc(title)}</text>')
    ret.append(
        f'<text x="{pad}" y="{head_h + 14}" font-family="DejaVu Sans, Arial, sans-serif" font-size="11" '
        f'fill="#666666">generated by tools/make_pinout.py from src/pins.h - do not edit by hand</text>')
    ret.extend(body)
    ret.append("</svg>")
    return "\n".join(ret)


def main():
    ap = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    ap.add_argument("--board", action="append", choices=sorted(BOARDS),
                    help="board to generate (repeatable; default: all)")
    ap.add_argument("--out", default=str(ROOT / "docs"),
                    help="output directory (default: docs/)")
    args = ap.parse_args()

    boards = parse_pins_h()
    missing = [k for k, v in boards.items() if not v]
    if missing:
        print(f"error: no pins parsed for {', '.join(missing)} -- "
              "src/pins.h format changed?", file=sys.stderr)
        return 1

    wanted = args.board or sorted(BOARDS)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for board in wanted:
        path = out / f"pinout-{board}.svg"
        path.write_text(svg_for(board, boards[board]), encoding="utf-8")
        print(f"{path} ({len(boards[board])} pins)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
