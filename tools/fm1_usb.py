#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sin Fetish Audio
"""Which FM-1s are on USB right now, as JSON, for `tools/sfa ports` and the console's one-unit-at-a-time check.
Run it with Windows Python (WSL has no USB MIDI): it needs mido with python-rtmidi, and finds each FM-1 the way the
installer does (base/tools/fm1ports.py): by the HELLO every fm1-base firmware answers, with its firmware name.
A port held by another program (a mirror tab, a DAW) cannot be asked, and shows up under "busy"."""
import json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "base" / "tools"))
try:
    import mido, fm1ports
except Exception as e:
    print(json.dumps({"error": f"mido / python-rtmidi missing on this Python ({e}). On Windows: pip install mido python-rtmidi"})); sys.exit(0)
try:
    names = mido.get_input_names()
    found = [{"port": f.input, "firmware": f.firmware} for f in fm1ports.ask(mido)]
    seen = {f["port"] for f in found}
    busy = [n for n in names if fm1ports.usual(n) and n not in seen]
    print(json.dumps({"found": found, "busy": busy, "inputs": len(names)}))
except Exception as e:
    print(json.dumps({"error": str(e)}))
