#!/usr/bin/env python3
"""Generates data/index.html from data/index.template.html.

The template is the maintained source: HTML/CSS/JS plus data-i18n hooks and
t() calls. The actual translations live in data/i18n/<code>.json, one file
per language. This script bundles the selected languages' JSON into the
template's %%I18N_JSON%% placeholder and writes the result to
data/index.html, which is what actually gets uploaded to the device with
`pio run -t uploadfs`.

Which languages get bundled is controlled by the UBIBLION_WEB_LANGS
environment variable, a comma-separated list of codes (default: all the
files found in data/i18n/). Trimming it down keeps unused languages out of
the LittleFS image, mirroring the UBIBLION_LANG_* firmware build flags for
the on-device menu (see platformio.ini). For example, an English-only build:

    UBIBLION_WEB_LANGS=en python tools/build_web.py

PlatformIO runs this automatically before every build (see the
extra_scripts line in platformio.ini), so `pio run -t uploadfs` always
uploads a fresh data/index.html; there's no need to run it by hand unless
you're just iterating on the web UI outside of PlatformIO.
"""

import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "data" / "index.template.html"
I18N_DIR = ROOT / "data" / "i18n"
OUTPUT = ROOT / "data" / "index.html"
PLACEHOLDER = "%%I18N_JSON%%"


def build(web_langs=None):
    available = {p.stem: p for p in sorted(I18N_DIR.glob("*.json"))}
    if not available:
        raise SystemExit(f"no language files found in {I18N_DIR}")

    if web_langs is None:
        web_langs = os.environ.get("UBIBLION_WEB_LANGS", "")
    codes = [c.strip() for c in web_langs.split(",") if c.strip()] or list(available)

    unknown = [c for c in codes if c not in available]
    if unknown:
        raise SystemExit(f"unknown language(s) in UBIBLION_WEB_LANGS: {unknown} "
                         f"(available: {sorted(available)})")

    bundle = {code: json.loads(available[code].read_text(encoding="utf-8")) for code in codes}

    template = TEMPLATE.read_text(encoding="utf-8")
    if PLACEHOLDER not in template:
        raise SystemExit(f"{TEMPLATE} is missing the {PLACEHOLDER} placeholder")

    # compact separators: this ends up embedded in the served page as-is.
    # Escaping "</" defends against a translation string that happens to
    # contain "</script", which would otherwise end the script block early.
    payload = json.dumps(bundle, ensure_ascii=False, separators=(",", ":"))
    payload = payload.replace("</", "<\\/")
    html = template.replace(PLACEHOLDER, payload)
    OUTPUT.write_text(html, encoding="utf-8")
    print(f"[build_web] wrote {OUTPUT} with languages: {', '.join(codes)}")


if __name__ == "__main__":
    build()
