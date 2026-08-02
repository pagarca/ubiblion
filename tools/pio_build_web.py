# PlatformIO pre-build hook: regenerates data/index.html from
# data/index.template.html + data/i18n/*.json before every build, so
# `pio run -t uploadfs` always uploads a fresh file. See build_web.py for
# the actual logic and UBIBLION_WEB_LANGS.
#
# SCons execs this file without setting __file__, so the tools/ directory is
# located via PlatformIO's PROJECT_DIR instead.
import sys
from pathlib import Path

Import("env")  # noqa: F821 -- injected by PlatformIO's SCons environment

sys.path.insert(0, str(Path(env["PROJECT_DIR"]) / "tools"))  # noqa: F821
import build_web  # noqa: E402

build_web.build()
