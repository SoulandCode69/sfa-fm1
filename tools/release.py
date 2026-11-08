#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sin Fetish Audio
"""Package Earth Creature Beats for distribution, from a finished build (tools/go.sh or base/tools/build.py app):
    python3 tools/release.py [--version 1.0]
  -> dist/ecb-<version>/            a static site: install/ (the web installer, Chrome/Edge over Web MIDI), firmware/
                                    (the .fwsc, the user sample bank, firmware.json), mirror/, backup/, quickstart.html
  -> dist/EarthCreatureBeats-<version>.zip   the site plus README, docs/, tools/sfa_sample.py, LICENSES
Serve the folder anywhere static (GitHub Pages, a USB stick + `python3 -m http.server`); the installer needs https or
localhost for Web MIDI."""
import argparse, hashlib, json, re, shutil, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "base" / "tools")); sys.path.insert(0, str(ROOT / "tools"))
import make_site  # noqa: E402
import ecb_site   # noqa: E402

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default=None)
    ap.add_argument("--app", default="app", help="app (ECB) or visitors (VISITORS)")
    a = ap.parse_args()
    app, build = ROOT / a.app, ROOT / a.app / "build"
    ecb_site.set_brand(a.app)
    slug = "ecb" if a.app == "app" else "alien"
    conf = make_site.read_conf(app / "fm1.toml")
    pkgs = sorted(build.glob("*.fwsc"), key=lambda p: p.stat().st_mtime)
    if not pkgs: sys.exit("no app/build/*.fwsc: run tools/go.sh (or base/tools/build.py app) first")
    pkg = pkgs[-1]
    fw = json.loads((build / "firmware.json").read_text()) if (build / "firmware.json").exists() else {}
    version = a.version or fw.get("version") or "dev"
    raw = pkg.read_bytes()
    product = make_site.product_of(raw)
    out = ROOT / "dist" / f"{slug}-{version}"
    if out.exists(): shutil.rmtree(out)
    (out / "firmware").mkdir(parents=True); (out / "install").mkdir()
    name = f"{slug}-{version}.fwsc"
    shutil.copy(pkg, out / "firmware" / name)
    res_src = build / f"{pkg.stem}-res.bin"
    res_name = f"{slug}-{version}-res.bin"
    if res_src.exists(): shutil.copy(res_src, out / "firmware" / res_name)
    if (build / "firmware.json").exists(): shutil.copy(build / "firmware.json", out / "firmware" / "firmware.json")
    # m-bootloader, offered by the installer, with its licence; the other licences beside the package
    SRC = make_site.SRC
    boot = SRC / "web" / "m-bootloader" / f"fm1-uboot-{make_site.MBOOT_VERSION}.fwsc"
    boot_name = f"m-bootloader-{make_site.MBOOT_VERSION}.fwsc"
    shutil.copy(boot, out / "firmware" / boot_name)
    shutil.copy(SRC / "web" / "m-bootloader" / "LICENSE", out / "firmware" / "m-bootloader-LICENSE.txt")
    for lic in sorted((SRC / "LICENSES").glob("*.txt")): shutil.copy(lic, out / "firmware" / lic.name)
    # the web installer
    html = (SRC / "web" / "installer.html").read_text(encoding="utf-8")
    lib = "\n".join(make_site.strip_module((SRC / "web" / f).read_text(encoding="utf-8")) for f in ("fm1ports.js", "fm1pkg.js", "fm1ota.js"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name, "backup": bool(conf.get("backup")),
                       "res": "../firmware/" + res_name if res_src.exists() else None,
                       "boot": {"pkg": "../firmware/" + boot_name, "version": make_site.MBOOT_VERSION, "sha256": hashlib.sha256(boot.read_bytes()).hexdigest()}})
    repo = conf.get("repo", "https://github.com/SoulandCode69/sfa-fm1")
    for key, val in (("RES", conf.get("resources_what", "Hopper's user sample bank")), ("RES_JA", conf.get("resources_what_ja", "Hopper のサンプル")),
                     ("NAME", conf["name"]), ("REPO", repo), ("REPO_SHORT", re.sub(r"^https?://", "", repo)),
                     ("LICENSE", conf.get("licence", "GPL-3.0")), ("CREDITS", " " + conf.get("credits", "Sin Fetish Audio, on fm1-base."))):
        html = html.replace("{{" + key + "}}", val)
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta).replace("/*APP*/", "null")
    (out / "install" / "index.html").write_text(ecb_site.skin(html, "install", 1), encoding="utf-8")
    # mirror, backup, landing, quick start
    for rel, h in make_site.tool_pages(app, conf).items():
        (out / rel).parent.mkdir(parents=True, exist_ok=True)
        (out / rel).write_text(ecb_site.skin(h, rel.split("/")[0], 1), encoding="utf-8")
    if (app / "web" / "words").is_dir(): shutil.copytree(app / "web" / "words", out / "words")
    (out / "index.html").write_text(ecb_site.landing(version, name) if a.app == "app" else ecb_site.landing(version, name, title="VISITORS"), encoding="utf-8")
    (out / "quickstart.html").write_text(ecb_site.quickstart(), encoding="utf-8")
    # the zip: the site plus the docs and the sample tool
    z = ROOT / "dist" / f"{"EarthCreatureBeats" if a.app == "app" else "AlienStack"}-{version}.zip"
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        for p in out.rglob("*"):
            if p.is_file(): zf.write(p, f"{z.stem}/site/{p.relative_to(out)}")
        for p in [ROOT / "README.md", ROOT / "tools" / "sfa_sample.py", *sorted((ROOT / "docs").glob("*.md"))]:
            zf.write(p, f"{z.stem}/{p.relative_to(ROOT)}")
    print(f"{out}  ({product}, {len(raw)} B)\n{z}  ({z.stat().st_size // 1024} KB)")

if __name__ == "__main__":
    main()
