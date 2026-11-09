#!/usr/bin/env bash
# Writes a clean snapshot of this working tree (tracked + new, minus ignored files, no history) into a target repo
# folder, with base/ as the fm1-base submodule at the commit this tree uses. Does not commit or push.
#   tools/make_clean_repo.sh ~/sfa
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)"; DST="${1:?target folder}"
mkdir -p "$DST"; cd "$SRC"
python3 tools/repo_check.py
git ls-files -co --exclude-standard -z | grep -zv '^base/' | grep -zv '^base$' | tar --null -T - -cf - | tar -xf - -C "$DST"
SHA="$(git submodule status base | awk '{print $1}' | tr -d '+-')"
URL="$(git config -f .gitmodules submodule.base.url)"
cd "$DST"
[ -d .git ] || git init -q -b main
if [ ! -e .gitmodules ]; then git submodule add "$URL" base; fi
git -C base fetch -q origin "$SHA" 2>/dev/null || true
git -C base checkout -q "$SHA"
echo "snapshot in $DST (base at $SHA). Next: git add -A && git commit && git push -u origin main"
