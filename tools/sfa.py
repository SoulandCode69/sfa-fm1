#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sin Fetish Audio
"""The SFA console: one command for the whole loop. Run in WSL/Ubuntu (or any Linux) from the repository.

  tools/sfa doctor                 what is installed, what is missing, what each step needs
  tools/sfa sim ecb                build the simulator, run the checks (no hardware, no toolchain)
  tools/sfa render ecb [song tests kits levels audit]
                                   render WAVs from the simulator into renders/<unit>/<stamp>/
  tools/sfa report ecb             numbers for the latest renders (peak, true peak, LUFS, clicks, stereo) -> report.md
  tools/sfa analyze [ecb as]       render if needed, report, then open the SFA Analyzer on the renders
  tools/sfa flash ecb              build the package and flash the unit on USB (tools/go.sh)
  tools/sfa up ecb [--no-flash]    sim -> render -> report -> flash, stopping at the first failure
  tools/sfa publish                copy the analyzer, firmware and renders to Windows: %USERPROFILE%\\SFA\\ + launchers
  tools/sfa units                  the registered units (sfa/units.json)

A unit is a line in sfa/units.json: ecb (Earth Creature Beats), as (Alien Stack). A new firmware or engine is one more line."""
import argparse, json, os, re, shutil, subprocess, sys, time, shlex
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONF = json.loads((ROOT / "sfa" / "units.json").read_text())
UNITS = CONF["units"]
RENDERS = ROOT / "renders"
ANALYZER = ROOT / "sfa" / "analyzer" / "index.html"

def c(code, s): return f"\033[{code}m{s}\033[0m" if sys.stdout.isatty() else s
ok = lambda s: c("32", s); bad = lambda s: c("31", s); dim = lambda s: c("2", s); bold = lambda s: c("1", s); warn = lambda s: c("33", s)

def unit(key):
    k = key.lower()
    for uid, u in UNITS.items():
        if k == uid or k in u.get("aliases", []) or k == u["dir"]: return uid, u
    sys.exit(f"unknown unit '{key}'. Registered: {', '.join(UNITS)}")

def sh(cmd, **kw):
    return subprocess.run(cmd, cwd=kw.pop("cwd", ROOT), text=True, capture_output=kw.pop("capture", True), **kw)

def wsl_win_home():
    """The Windows user profile as a WSL path, or None when not under WSL."""
    try:
        r = subprocess.run(["cmd.exe", "/c", "echo %USERPROFILE%"], capture_output=True, text=True, timeout=10)
        w = r.stdout.strip().replace("\r", "")
        if not w or "%" in w: return None
        p = subprocess.run(["wslpath", "-u", w], capture_output=True, text=True).stdout.strip()
        return Path(p) if p and Path(p).is_dir() else None
    except Exception:
        return None

# ---------- doctor ----------
def cmd_doctor(a):
    rows = []
    def add(name, good, detail, need=""): rows.append((name, good, detail, need))
    add("python3", sys.version_info >= (3, 8), sys.version.split()[0])
    for mod in ("numpy", "scipy"):
        try: m = __import__(mod); add(f"python: {mod}", True, getattr(m, "__version__", ""))
        except Exception: add(f"python: {mod}", mod == "scipy", "missing" + (" (optional, makes reports faster)" if mod == "scipy" else ""), "pip install " + mod + " --break-system-packages")
    add("git", shutil.which("git") is not None, "")
    add("C compiler (simulator)", shutil.which("cc") is not None or shutil.which("gcc") is not None, "", "sudo apt install build-essential")
    tc = os.environ.get("JIELI_TOOLCHAIN") or str(Path.home() / ".jieli" / "toolchain"); sdk = os.environ.get("AC79_SDK") or str(Path.home() / "fw-AC79_AIoT_SDK")
    add("JieLi toolchain (builds .fwsc)", Path(tc).is_dir(), tc, "see README: the device package is built only where the toolchain is")
    add("AC79 SDK", Path(sdk).is_dir(), sdk, "see README")
    wpy = shutil.which("python.exe"); add("Windows python.exe (flashing over USB)", bool(wpy), wpy or "not found", "winget install Python.Python.3.12, then pip install mido python-rtmidi on Windows")
    if wpy:                                                              # flashing runs on Windows Python, so mido and rtmidi are checked there
        r = subprocess.run([wpy, "-c", "import mido, rtmidi; print(mido.__version__)"], capture_output=True, text=True)
        add("Windows python: mido + python-rtmidi", r.returncode == 0, r.stdout.strip() if r.returncode == 0 else "missing", "on Windows: pip install mido python-rtmidi")
    wh = wsl_win_home(); add("Windows home (for SFA folder)", bool(wh), str(wh or "not under WSL"))
    for uid, u in UNITS.items():
        host = ROOT / u["dir"] / "build" / "host" / u["host"]; pk = sorted((ROOT / u["dir"] / "build").glob("*.fwsc"), key=lambda p: p.stat().st_mtime) if (ROOT / u["dir"] / "build").is_dir() else []
        add(f"{uid}: simulator binary", host.exists(), time.strftime("built %b %d %H:%M", time.localtime(host.stat().st_mtime)) if host.exists() else "run: tools/sfa sim " + uid)
        add(f"{uid}: device package", bool(pk), (pk[-1].name + " " + time.strftime("%b %d %H:%M", time.localtime(pk[-1].stat().st_mtime))) if pk else "run: tools/sfa flash " + uid + " (needs the toolchain)")
    print(bold("SFA doctor"))
    for n, g, d, need in rows:
        print(f"  {ok('ok  ') if g else warn('miss')}  {n:<40} {dim(d)}" + ("" if g or not need else "\n        " + dim("-> " + need)))
    print()
    print("Simulator, render and analyze need only python, numpy and a C compiler. Flashing needs the toolchain and Windows Python.")

def cmd_units(a):
    for uid, u in UNITS.items(): print(f"{uid:<4} {u['name']:<22} dir={u['dir']:<9} hw={u.get('hardware','')}  engines: {', '.join(u.get('engines', []))}")

# ---------- sim ----------
def build_host(u):
    r = sh(["python3", "base/tools/build.py", u["dir"], "--host"])
    if r.returncode: print(r.stdout[-3000:], r.stderr[-3000:]); sys.exit(bad(f"host build failed for {u['name']}"))
    return ROOT / u["dir"] / "build" / "host" / u["host"]

def run_script(u, script, outdir):
    outdir.mkdir(parents=True, exist_ok=True)
    host = ROOT / u["dir"] / "build" / "host" / u["host"]
    return sh([str(host), str(ROOT / u["dir"] / script), str(outdir)])

def cmd_sim(a):
    uid, u = unit(a.unit); print(bold(f"{u['name']}: build + simulator checks"))
    t = time.time(); build_host(u); print(f"  built the host simulator in {time.time()-t:.0f}s")
    out = ROOT / u["dir"] / "build" / "host" / "out"; t = time.time()
    r = run_script(u, "tests/basic.txt", out); text = r.stdout + r.stderr
    fails = [l for l in text.splitlines() if re.search(r"FAIL|mismatch|assert|error", l, re.I) and "host:" not in l]
    n = len(re.findall(r"^expect ", (ROOT / u["dir"] / "tests" / "basic.txt").read_text(), re.M))
    if r.returncode or fails:
        print(bad(f"  FAILED ({len(fails)} problem lines):")); [print("   ", l) for l in fails[:15]]; sys.exit(1)
    print(ok(f"  {n} checks passed") + dim(f" in {time.time()-t:.0f}s; screenshots in {out.relative_to(ROOT)}"))
    return 0

# ---------- render ----------
def collect_wavs(outdir, since):
    return sorted(p for p in outdir.glob("*.wav") if p.stat().st_mtime >= since - 1)

def cmd_render(a, quiet=False):
    uid, u = unit(a.unit); recipes = a.recipes or CONF["default_recipes"]
    build_host(u) if (not (ROOT / u["dir"] / "build" / "host" / u["host"]).exists() or a.rebuild) else None
    stamp = time.strftime("%Y%m%d-%H%M%S"); dest = RENDERS / uid / stamp; dest.mkdir(parents=True)
    files = []
    for rec in recipes:
        if rec not in CONF["recipes"]: sys.exit(f"unknown recipe '{rec}'. Choose from: {', '.join(CONF['recipes'])}")
        R = CONF["recipes"][rec]; tmp = RENDERS / ".tmp" / uid / rec
        if tmp.exists(): shutil.rmtree(tmp)
        tmp.mkdir(parents=True); t0 = time.time()
        if R.get("gen"):
            sh(["python3", "tools/audit.py", "--app", u["dir"], "gen"])
            for scr in sorted((ROOT / u["dir"] / "tests").glob("audit_*.txt")): run_script(u, f"tests/{scr.name}", tmp)
        else:
            r = run_script(u, R["script"], tmp)
            if r.returncode: print(bad(f"  {rec}: the simulator exited with {r.returncode}")); print((r.stdout + r.stderr)[-1200:]); sys.exit(1)
        got = collect_wavs(tmp, t0)
        for w in got:
            name = w.name if rec in ("tests", "song") else f"{rec}-{w.name}"
            shutil.copy(w, dest / name); files.append({"name": f"{u['short']} {name}", "path": f"{uid}/{stamp}/{name}", "group": f"{u['name']} · {rec}"})
        if not quiet: print(f"  {rec:<7} {len(got)} WAV(s)  {dim(R['about'])}")
    (dest / "manifest.json").write_text(json.dumps({"unit": u["name"], "stamp": stamp, "files": files}, indent=1))
    latest = RENDERS / uid / "latest"
    try:
        if latest.is_symlink() or latest.exists(): latest.unlink()
        latest.symlink_to(stamp)
    except OSError: pass
    (RENDERS / uid / "latest.txt").write_text(stamp)
    if not quiet: print(ok(f"  rendered to renders/{uid}/{stamp}/"))
    return dest

def latest_dir(uid):
    p = RENDERS / uid / "latest.txt"
    if not p.exists(): return None
    d = RENDERS / uid / p.read_text().strip(); return d if d.is_dir() else None

# ---------- report: the same measurements as the analyzer, in numpy, so the two cross-check ----------
def load_wav(path):
    import wave, numpy as np
    w = wave.open(str(path)); sr, ch, sw = w.getframerate(), w.getnchannels(), w.getsampwidth(); raw = w.readframes(w.getnframes())
    if sw == 2: d = np.frombuffer(raw, "<i2").astype(np.float64) / 32768
    elif sw == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32); v = b[:, 0] | b[:, 1] << 8 | b[:, 2] << 16; v = np.where(v & 0x800000, v - 0x1000000, v); d = v / 8388608.0
    elif sw == 4: d = np.frombuffer(raw, "<i4").astype(np.float64) / 2147483648
    else: raise ValueError(f"{path.name}: {sw * 8}-bit not supported")
    d = d.reshape(-1, ch); L = d[:, 0]; R = d[:, 1] if ch > 1 else d[:, 0]
    return L, R, sr, ch == 1

def biquad(x, b, a):
    try:
        from scipy.signal import lfilter; return lfilter(b, a, x)
    except ImportError:
        import numpy as np; y = np.zeros_like(x); z1 = z2 = 0.0
        for i, v in enumerate(x): o = b[0] * v + z1; z1 = b[1] * v - a[1] * o + z2; z2 = b[2] * v - a[2] * o; y[i] = o
        return y

def kweight(x, sr):
    import math
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196; K = math.tan(math.pi * f0 / sr); Vh = 10 ** (G / 20); Vb = Vh ** 0.4996667741545416; a0 = 1 + K / Q + K * K
    y = biquad(x, [(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0], [1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0])
    f0, Q = 38.13547087602444, 0.5003270373238773; K = math.tan(math.pi * f0 / sr); d = 1 + K / Q + K * K
    return biquad(y, [1, -2, 1], [1, 2 * (K * K - 1) / d, (1 - K / Q + K * K) / d])

def lufs(L, R, sr):
    import numpy as np
    sb = max(1, round(0.1 * sr)); nsb = len(L) // sb
    if nsb < 4: return None, None, None, None
    E = np.zeros(nsb)
    for x in (L, R): k = kweight(x, sr)[:nsb * sb]; E += (k * k).reshape(nsb, sb).mean(axis=1)
    lu = lambda e: -0.691 + 10 * np.log10(e)
    mom = (E[3:] + E[2:-1] + E[1:-2] + E[:-3]) / 4
    cs = np.concatenate([[0], np.cumsum(E)]); st = (cs[30:] - cs[:-30]) / 30 if nsb >= 30 else np.array([])
    I = M = S = LRA = None
    M = float(lu(mom.max()))
    g1 = mom[lu(mom) > -70]
    if len(g1): rel = lu(g1.mean()) - 10; g2 = g1[lu(g1) > rel]; I = float(lu(g2.mean())) if len(g2) else None
    if len(st):
        S = float(lu(st.max())); s1 = st[lu(st) > -70]
        if len(s1) > 2:
            rel = lu(s1.mean()) - 20; v = np.sort(lu(s1[lu(s1) > rel]))
            if len(v) > 2: LRA = float(v[int(0.95 * (len(v) - 1))] - v[int(0.10 * (len(v) - 1))])
    return I, M, S, LRA

def true_peak(x):
    import numpy as np
    try:
        from scipy.signal import resample_poly; return float(np.abs(resample_poly(x, 4, 1)).max())
    except ImportError: return float(np.abs(x).max())

def clicks(x, sr, k=10.0, min_abs=0.03, isolate=4.5):
    import numpy as np
    d = np.abs(np.diff(x)); W = 64; S = np.concatenate([[0], np.cumsum(d)]); out = []
    for j in np.nonzero(d >= min_abs)[0]:
        i = j + 1                                                        # d[j] = |x[i]-x[i-1]|
        if i < W + 2 or i > len(x) - W - 30: continue
        ref = (S[j + W] - S[j - W] - d[j - 1:j + 2].sum()) / (2 * W - 3)  # the JS uses sums over [i-W, i+W) of |x[n]-x[n-1]|
        if d[j] < k * ref: continue
        sg = x[i] - x[i - 1]; nb = 0.0
        for q in range(i - 40, i + 41):
            if q == i: continue
            dd = x[q] - x[q - 1]
            if q == i + 1 and dd * sg < 0 and abs(dd) > 0.5 * d[j]: continue
            nb = max(nb, abs(dd))
        if d[j] < isolate * nb: continue
        pw = round(sr * 0.002); pre = np.abs(x[max(0, i - 3 - pw):i - 3]).sum() / pw
        if pre < 0.0005: continue
        if out and i - out[-1][0] < sr * 0.005:
            if d[j] > out[-1][1]: out[-1] = (i, d[j])
        else: out.append((i, d[j]))
    return out

def measure(path):
    import numpy as np
    L, R, sr, mono = load_wav(path); chs = [L] if mono else [L, R]; db = lambda v: 20 * np.log10(max(v, 1e-9))
    pk = max(np.abs(x).max() for x in chs); tp = max(true_peak(x) for x in chs); rms = np.sqrt(sum((x * x).sum() for x in chs) / (len(L) * len(chs)))
    I, M, S, LRA = lufs(L, R, sr); ck = sorted({round(i / sr, 4) for x in chs for i, _ in clicks(x, sr)})
    clips = int(sum((np.abs(x) >= 0.9995).sum() for x in chs)); corr = float((L * R).sum() / np.sqrt((L * L).sum() * (R * R).sum() + 1e-30)) if not mono else 1.0
    m = {"file": path.name, "dur": len(L) / sr, "sr": sr, "peak": db(pk), "tp": db(tp), "rms": db(rms), "crest": db(pk) - db(rms), "lufs": I, "mom": M, "st": S, "lra": LRA, "corr": corr, "clicks": ck, "clips": clips}
    why = []; lvl = 0
    if clips: lvl = 2; why.append(f"{clips} samples at full scale")
    if m["tp"] > 0: lvl = 2; why.append(f"true peak {m['tp']:.1f} dBTP over 0")
    elif m["tp"] > -1: lvl = max(lvl, 1); why.append(f"true peak {m['tp']:.1f} dBTP, under 1 dB of headroom")
    if len(ck) >= 4: lvl = 2; why.append(f"{len(ck)} clicks")
    elif ck: lvl = max(lvl, 1); why.append(f"{len(ck)} possible click(s)")
    m["verdict"] = ["clean", "watch", "flag"][lvl]; m["why"] = why or ["no clipping, no clicks, headroom ok"]
    return m

def cmd_report(a, uids=None):
    import numpy as np
    uids = uids or [unit(x)[0] for x in (a.units or UNITS)]
    for uid in uids:
        u = UNITS[uid]; d = latest_dir(uid)
        if not d: print(warn(f"{u['name']}: nothing rendered yet (tools/sfa render {uid})")); continue
        fs = sorted(w for w in d.glob("*.wav") if not (a.only and a.only not in w.name))
        if not fs: continue
        print(bold(f"{u['name']}  {dim(d.name)}")); print(dim(f"  {'file':<28}{'len':>6}{'peak':>7}{'TP':>7}{'RMS':>7}{'LUFS-I':>8}{'LRA':>6}{'clk':>5}  verdict"))
        rows = []
        for w in fs:
            try: m = measure(w)
            except Exception as e: print(warn(f"  {w.name}: {e}")); continue
            rows.append(m); f = lambda v, p=1: "-" if v is None else f"{v:.{p}f}"
            col = {"clean": ok, "watch": warn, "flag": bad}[m["verdict"]]
            print(f"  {m['file'][:28]:<28}{m['dur']:>6.1f}{m['peak']:>7.1f}{m['tp']:>7.1f}{m['rms']:>7.1f}{f(m['lufs']):>8}{f(m['lra']):>6}{len(m['clicks']):>5}  {col(m['verdict'])}" + (dim("  " + "; ".join(m["why"])) if m["verdict"] != "clean" else ""))
        md = [f"# {u['name']} render report", f"Renders: `{d.relative_to(ROOT)}`", "", "| file | len s | peak | true peak | RMS | LUFS-I | LRA | clicks | clips | verdict |", "|---|---:|---:|---:|---:|---:|---:|---:|---:|---|"]
        for m in rows: md.append(f"| {m['file']} | {m['dur']:.1f} | {m['peak']:.1f} | {m['tp']:.1f} | {m['rms']:.1f} | {'-' if m['lufs'] is None else round(m['lufs'],1)} | {'-' if m['lra'] is None else round(m['lra'],1)} | {len(m['clicks'])} | {m['clips']} | {m['verdict']}: {'; '.join(m['why'])} |")
        (d / "report.md").write_text("\n".join(md) + "\n"); (d / "report.json").write_text(json.dumps(rows, indent=1, default=float))
        print(dim(f"  saved {d.relative_to(ROOT)}/report.md"))

# ---------- analyze / serve ----------
def sync_renders_site(uids):
    """renders/index.html (the analyzer) + renders/manifest.json (every unit's latest renders), ready for any static server."""
    RENDERS.mkdir(exist_ok=True); shutil.copy(ANALYZER, RENDERS / "index.html")
    files = []
    for uid in uids:
        d = latest_dir(uid)
        if d and (d / "manifest.json").exists(): files += json.loads((d / "manifest.json").read_text())["files"]
    (RENDERS / "manifest.json").write_text(json.dumps({"title": "SFA renders", "files": files}, indent=1))
    return len(files)

def cmd_analyze(a):
    uids = [unit(x)[0] for x in (a.units or UNITS)]
    for uid in uids:
        if a.fresh or not latest_dir(uid):
            print(bold(f"rendering {UNITS[uid]['name']}")); build_host(UNITS[uid]) if not (ROOT / UNITS[uid]["dir"] / "build" / "host" / UNITS[uid]["host"]).exists() else None
            cmd_render(argparse.Namespace(unit=uid, recipes=a.recipes, rebuild=False))
    cmd_report(argparse.Namespace(units=uids, only=None), uids)
    n = sync_renders_site(uids); port = a.port
    print(bold(f"\nAnalyzer: {n} file(s) at http://localhost:{port}/   (Ctrl-C to stop)"))
    if not a.no_open: open_url(f"http://localhost:{port}/")
    serve(RENDERS, port)

def serve(directory, port):
    import http.server, socketserver
    class H(http.server.SimpleHTTPRequestHandler):
        def __init__(s, *args, **kw): super().__init__(*args, directory=str(directory), **kw)
        def log_message(s, *args): pass
        def end_headers(s): s.send_header("Cache-Control", "no-store"); super().end_headers()
    socketserver.TCPServer.allow_reuse_address = True
    try:
        with socketserver.ThreadingTCPServer(("127.0.0.1", port), H) as srv: srv.serve_forever()
    except KeyboardInterrupt: pass
    except OSError as e: print(warn(f"port {port} busy ({e}); the analyzer may already be running there"))

def open_url(url):
    for cmd in (["cmd.exe", "/c", "start", "", url], ["powershell.exe", "-c", f"Start-Process '{url}'"], ["xdg-open", url]):
        try:
            if subprocess.run(cmd, capture_output=True, timeout=15).returncode == 0: return
        except Exception: continue
    print("open", url)

# ---------- flash / up ----------
def cmd_flash(a):
    uid, u = unit(a.unit); app = u["dir"]
    print(bold(f"flashing {u['name']}: one unit on USB, mirror tabs closed"))
    r = subprocess.run(["sh", str(ROOT / "tools" / "go.sh"), app] + (["--no-flash"] if a.build_only else []), cwd=ROOT)
    if r.returncode: sys.exit(r.returncode)
    return 0

def cmd_up(a):
    uid, u = unit(a.unit)
    cmd_sim(argparse.Namespace(unit=uid))
    print(bold("\nrender")); cmd_render(argparse.Namespace(unit=uid, recipes=a.recipes, rebuild=False))
    print(); cmd_report(argparse.Namespace(units=[uid], only=None), [uid])
    d = latest_dir(uid); rep = json.loads((d / "report.json").read_text()) if d and (d / "report.json").exists() else []
    flagged = [m["file"] for m in rep if m["verdict"] == "flag"]
    if flagged and not a.force: sys.exit(bad(f"\nstopping: flagged renders ({', '.join(flagged[:5])}). Open them with tools/sfa analyze {uid}, fix, or use --force."))
    if a.no_flash: print(ok("\nready to flash (skipped: --no-flash)")); return
    cmd_flash(argparse.Namespace(unit=uid, build_only=False))
    sync_renders_site(list(UNITS))

# ---------- publish to Windows ----------
BAT = {
    "SFA Analyzer.bat": 'wsl -d Ubuntu -e bash -lc "cd ~/sfa-fm1 && tools/sfa analyze"',
    "SFA Doctor.bat": 'wsl -d Ubuntu -e bash -lc "cd ~/sfa-fm1 && tools/sfa doctor" & pause',
    "SFA Sim ECB.bat": 'wsl -d Ubuntu -e bash -lc "cd ~/sfa-fm1 && tools/sfa sim ecb" & pause',
    "SFA Sim Alien Stack.bat": 'wsl -d Ubuntu -e bash -lc "cd ~/sfa-fm1 && tools/sfa sim as" & pause',
    "SFA Flash ECB.bat": 'wsl -d Ubuntu -e bash -lc "cd ~/sfa-fm1 && git pull && tools/sfa up ecb" & pause',
    "SFA Flash Alien Stack.bat": 'wsl -d Ubuntu -e bash -lc "cd ~/sfa-fm1 && git pull && tools/sfa up as" & pause',
}
def cmd_publish(a):
    wh = wsl_win_home()
    if not wh: sys.exit("not under WSL, so there is no Windows folder to publish to. (renders/ and sfa/analyzer/index.html are already here.)")
    base = wh / CONF.get("windows_home_dir", "SFA")
    for sub in ("Analyzer", "Firmware", "Renders", "Launchers"): (base / sub).mkdir(parents=True, exist_ok=True)
    shutil.copy(ANALYZER, base / "Analyzer" / "SFA Analyzer.html")
    for uid, u in UNITS.items():
        b = ROOT / u["dir"] / "build"; pk = sorted(b.glob("*.fwsc"), key=lambda p: p.stat().st_mtime) if b.is_dir() else []
        if pk:
            (base / "Firmware" / uid).mkdir(exist_ok=True); dst = base / "Firmware" / uid / f"{pk[-1].stem}-{time.strftime('%Y%m%d-%H%M', time.localtime(pk[-1].stat().st_mtime))}.fwsc"; shutil.copy(pk[-1], dst); print("  firmware", dst.name)
        d = latest_dir(uid)
        if d: t = base / "Renders" / uid / d.name; shutil.copytree(d, t, dirs_exist_ok=True); print("  renders ", f"{uid}/{d.name}")
    for name, line in BAT.items(): (base / "Launchers" / name).write_text("@echo off\r\n" + line + "\r\n")
    print(ok(f"published to {base}")); print(dim("  Analyzer\\SFA Analyzer.html opens by double-click (drop WAVs onto it); Launchers\\ run the loop through WSL."))

def main():
    ap = argparse.ArgumentParser(prog="sfa", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    sp.add_parser("doctor").set_defaults(f=cmd_doctor); sp.add_parser("units").set_defaults(f=cmd_units)
    p = sp.add_parser("sim"); p.add_argument("unit"); p.set_defaults(f=cmd_sim)
    p = sp.add_parser("render"); p.add_argument("unit"); p.add_argument("recipes", nargs="*"); p.add_argument("--rebuild", action="store_true"); p.set_defaults(f=cmd_render)
    p = sp.add_parser("report"); p.add_argument("units", nargs="*"); p.add_argument("--only", help="only files whose name contains this"); p.set_defaults(f=cmd_report)
    p = sp.add_parser("analyze"); p.add_argument("units", nargs="*"); p.add_argument("--fresh", action="store_true", help="render again first"); p.add_argument("--recipes", nargs="*"); p.add_argument("--port", type=int, default=8590); p.add_argument("--no-open", action="store_true"); p.set_defaults(f=cmd_analyze)
    p = sp.add_parser("flash"); p.add_argument("unit"); p.add_argument("--build-only", action="store_true"); p.set_defaults(f=cmd_flash)
    p = sp.add_parser("up"); p.add_argument("unit"); p.add_argument("recipes", nargs="*"); p.add_argument("--no-flash", action="store_true"); p.add_argument("--force", action="store_true"); p.set_defaults(f=cmd_up)
    sp.add_parser("publish").set_defaults(f=cmd_publish)
    a = ap.parse_args(); a.f(a)

if __name__ == "__main__": main()
