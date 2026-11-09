#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sin Fetish Audio
"""Checks the files git would commit: big files, build output, WAVs, and things that look like secrets.
    python3 tools/repo_check.py            (exit 1 if something needs a look)"""
import re, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
BIG = 5 * 1024 * 1024
BAD_PATH = re.compile(r"(^|/)(build|dist|renders|render_tmp|__pycache__)/|\.(pyc|fwsc|o|elf)$")
SECRET = re.compile(r"(ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9]{20,}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|password\s*=\s*['\"][^'\"]+['\"])")
def main():
    files = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT, capture_output=True, text=True).stdout.split("\n")
    bad = []
    for f in filter(None, files):
        p = ROOT / f
        if not p.is_file() or f.startswith("base/"): continue
        if p.stat().st_size > BIG: bad.append(f"{f}: {p.stat().st_size // 1024} KB is over {BIG // 1024 // 1024} MB")
        if BAD_PATH.search(f): bad.append(f"{f}: build output or scratch, should be ignored")
        if f.lower().endswith(".wav") and not f.startswith(("assets/", "docs/")): bad.append(f"{f}: a WAV outside assets/ or docs/")
        if p.stat().st_size < 2_000_000 and f != "tools/repo_check.py":
            try: t = p.read_text(errors="strict")
            except Exception: continue
            m = SECRET.search(t)
            if m: bad.append(f"{f}: looks like a secret ({m.group(0)[:8]}...)")
    print("\n".join(bad) if bad else f"clean: {len([x for x in files if x])} files checked")
    return 1 if bad else 0
if __name__ == "__main__": sys.exit(main())
