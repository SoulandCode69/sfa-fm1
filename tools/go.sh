#!/bin/sh
# SFA Suite: build, flash the FM-1 (Unit A), write the sample bank, open the laptop UI. Run in WSL/Ubuntu:
#   tools/go.sh            build + install + open the mirror in Chrome
#   tools/go.sh --no-flash just build and (re)open the mirror
#   tools/go.sh --pull     git pull first
# Needs: the toolchain and SDK (README), Windows Python with mido + python-rtmidi (the installer runs on Windows:
# WSL has no USB MIDI), Chrome or Edge. The mirror page is served from WSL at http://localhost:8577/mirror/.
set -e
cd "$(dirname "$0")/.."
PORT=8577
for a in "$@"; do case "$a" in --pull) git pull --recurse-submodules;; --no-flash) NOFLASH=1;; esac; done

echo "== build"
python3 base/tools/build.py app
python3 base/tools/make_site.py app app/build/sfa.fwsc dev app/build/site >/dev/null
cp app/build/sfa.fwsc app/build/sfa-res.bin app/build/site/ 2>/dev/null || true

if [ -z "$NOFLASH" ]; then
    echo "== install (Windows Python; the FM-1 on USB; close the hub / mirror tabs first)"
    WPY=$(command -v python.exe || true)
    if [ -z "$WPY" ]; then echo "python.exe not found: install Python on Windows (winget install Python.Python.3.12)"; exit 1; fi
    W=$(wslpath -w "$PWD")
    "$WPY" "$W\\base\\tools\\fm1_install.py" "$W\\app\\build\\sfa.fwsc" --yes
fi

echo "== laptop UI"
if ! (ss -ltn 2>/dev/null | grep -q ":$PORT "); then
    nohup python3 -m http.server "$PORT" --bind 127.0.0.1 --directory app/build/site >/dev/null 2>&1 &
    sleep 1
fi
URL="http://localhost:$PORT/mirror/"
cmd.exe /c start "" "$URL" >/dev/null 2>&1 || powershell.exe -c "Start-Process '$URL'" >/dev/null 2>&1 || echo "open $URL"
echo "mirror: $URL   (press Connect; the FM-1 must not be open in another tab)"
