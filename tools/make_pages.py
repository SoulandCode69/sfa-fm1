#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The laptop pages (landing, mirror, backup, quick start) in ECB's look, from fm1-base's templates.
#   python3 tools/make_pages.py OUT_DIR
import sys, json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "base" / "tools")); sys.path.insert(0, str(ROOT / "tools"))
import make_site  # noqa: E402
import ecb_site   # noqa: E402
out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "app" / "build" / "site")
pages = make_site.tool_pages(ROOT / "app")
for rel, html in pages.items():
    p = out / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(ecb_site.skin(html, rel.split("/")[0], 1), encoding="utf-8")
    print(p)
fw = ROOT / "app" / "build" / "firmware.json"
version = "dev"
try: version = json.loads(fw.read_text()).get("version", "dev")
except Exception: pass
pkgs = sorted((ROOT / "app" / "build").glob("*.fwsc"))
(out / "index.html").write_text(ecb_site.landing(version, pkgs[-1].name if pkgs else "ecb.fwsc", has_install=False), encoding="utf-8")
(out / "quickstart.html").write_text(ecb_site.quickstart(), encoding="utf-8")
print(out / "index.html")
