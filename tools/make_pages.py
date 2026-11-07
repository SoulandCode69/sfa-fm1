#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The laptop pages (mirror, backup) from fm1-base's templates, without the whole site (no release, no emulator).
#   python3 tools/make_pages.py OUT_DIR
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "base" / "tools"))
import make_site  # noqa: E402
out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "app" / "build" / "site")
pages = make_site.tool_pages(ROOT / "app")
for rel, html in pages.items():
    p = out / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(html, encoding="utf-8")
    print(p)
(out / "index.html").write_text("<!doctype html><meta charset=utf-8><title>SFA</title><body style='font:16px system-ui;padding:24px;background:#111;color:#eee'>"
    "<h1>SFA Suite</h1><p><a style='color:#ff40a0' href='mirror/'>Mirror: the FM-1's screen and panel, live</a></p>"
    "<p><a style='color:#ff40a0' href='backup/'>Backup and restore</a></p>", encoding="utf-8")
