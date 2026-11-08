#!/bin/sh
# Earth Creature Beats: build, flash the FM-1 (Unit A), write the sample bank, open the laptop UI. Run in WSL/Ubuntu:
#   tools/go.sh            build + install + open the mirror in Chrome
#   tools/go.sh --no-flash just build and (re)open the mirror
#   tools/go.sh --pull     git pull first
# Needs: the toolchain and SDK (README), Windows Python with mido + python-rtmidi (the installer runs on Windows:
# WSL has no USB MIDI), Chrome or Edge. The mirror page is served from WSL at http://localhost:8577/mirror/.
set -e
APP="${1:-app}"                                              # which firmware: app (ECB, default), visitors (Alien Stack) or proto (SFA Proto)
case "$APP" in app|visitors|proto) ;; *) echo "usage: tools/go.sh [app|visitors|proto]"; exit 2;; esac
cd "$(dirname "$0")/.."
PORT=8577
[ "$APP" = visitors ] && PORT=8578                          # Alien Stack's pages on their own port
[ "$APP" = proto ] && PORT=8579
# the toolchain and SDK, wherever the README put them (a non-interactive shell skips ~/.bashrc)
[ -n "$JIELI_TOOLCHAIN" ] || { [ -d "$HOME/.jieli/toolchain" ] && export JIELI_TOOLCHAIN="$HOME/.jieli/toolchain"; }
[ -n "$AC79_SDK" ] || { [ -d "$HOME/fw-AC79_AIoT_SDK" ] && export AC79_SDK="$HOME/fw-AC79_AIoT_SDK"; }
for a in "$@"; do case "$a" in --pull) git pull --recurse-submodules;; --no-flash) NOFLASH=1;; esac; done

echo "== build"
python3 base/tools/build.py "$APP"
python3 tools/make_pages.py "$APP"/build/site "$APP" >/dev/null

if [ -z "$NOFLASH" ]; then
    echo "== install (Windows Python; the FM-1 on USB; close the hub / mirror tabs first)"
    WPY=$(command -v python.exe || true)
    if [ -z "$WPY" ]; then echo "python.exe not found: install Python on Windows (winget install Python.Python.3.12)"; exit 1; fi
    rm -f "$APP"/build/sfa.fwsc                                # (an old SFA package must never be flashed again)
    PKG=$(ls -t "$APP"/build/*.fwsc | head -1)                 # the package this build just wrote (ECB.fwsc)
    echo "== package: $PKG ($(date -r "$PKG" +%H:%M:%S))"
    try=0                                                      # (no --port: the unit answers to whatever firmware it runs now)
    until "$WPY" "$(wslpath -w "$PWD/base/tools/fm1_install.py")" "$(wslpath -w "$PWD/$PKG")" --yes; do
        try=$((try + 1))
        if [ "$try" -ge 3 ]; then
            echo
            echo "!! the FM-1 is on USB but could not be claimed. Almost always: a browser tab holds its MIDI port."
            echo "   Close EVERY localhost:$PORT tab (the mirror), and any other MIDI app, then run tools/go.sh $APP again."
            exit 3
        fi
        echo "-- not claimed; closing nothing myself. Retrying in 4 s (close the mirror tab now)..."
        sleep 4
    done
fi

echo "== release package (dist/)"
python3 tools/release.py --app "$APP" || echo "release: skipped"

echo "== laptop UI"
if ! (ss -ltn 2>/dev/null | grep -q ":$PORT "); then
    nohup python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$APP"/build/site >/dev/null 2>&1 &
    sleep 1
fi
URL="http://localhost:$PORT/mirror/"
cmd.exe /c start "" "$URL" >/dev/null 2>&1 || powershell.exe -c "Start-Process '$URL'" >/dev/null 2>&1 || echo "open $URL"
echo "mirror: $URL   (press Connect; the FM-1 must not be open in another tab)"
