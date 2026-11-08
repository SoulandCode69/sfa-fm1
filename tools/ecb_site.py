#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sin Fetish Audio
"""The Earth Creature Beats web look: the creatures as SVG (straight from app/icons.c's shapes), the theme, the
landing page, and the ECB skin for fm1-base's mirror / backup / installer pages. Used by tools/make_pages.py (the
local laptop UI) and tools/release.py (the distributable site)."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / "app" / "icons.c"
NAMES = ["OTTER", "CAT", "FOX", "HOPPER", "BEAVER"]
COLORS = {"OTTER": "#54ccff", "CAT": "#ffc428", "FOX": "#ff7650", "HOPPER": "#8ee864", "BEAVER": "#e2a46c"}
ROLES = {"OTTER": "Synth", "CAT": "Kit", "FOX": "Bass & 808", "HOPPER": "Cut", "BEAVER": "Sequencer"}
BLURB = {
    "OTTER": "A 4-voice polysynth with 64 presets: leads, pads, keys, plucks, basses and arps. ADSR, filter envelope, LFO, glide.",
    "CAT": "A sample drum machine: 8 kits x 16 pads, every pad its own pitch, decay, filter, level, sample, punch, snap and crush. Build your own kits.",
    "FOX": "The low end: 16 chromatic 808s and subs with glide, filter, click and punch, plus 24 synth basses from acid to reese.",
    "HOPPER": "A slicer and sampler: record the output, chop by transients, play the slices on the keys. Load your own WAVs.",
    "BEAVER": "8 patterns of 4 tracks, step entry and live record, per-step velocity, length, probability and ratchets, groove generators, a 16-slot song.",
}

def shade(hexcol, amt):
    r, g, b = (int(hexcol[i:i + 2], 16) for i in (1, 3, 5))
    t = 255 if amt > 0 else 0
    a = abs(amt) / 256
    return "#%02x%02x%02x" % tuple(int(c + (t - c) * a) for c in (r, g, b))

def shapes():
    """{"OTTER": [(kind, paint, outline, a, b, c, d, e, f), ...], ...} from app/icons.c"""
    s = ICONS.read_text()
    out = {}
    for name in NAMES:
        m = re.search(r"static const shape_t SH_%s\[\] = \{(.*?)\n\};" % name, s, re.S)
        body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
        rows = re.findall(r"\{(ELL|TRI),\s*P_(\w+),\s*(\d),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\}", body)
        out[name] = [(k, p, int(o), *map(int, v)) for k, p, o, *v in rows]
    return out

def svg(name, size=96, bg=None):
    col = COLORS[name]
    pal = {"BODY": col, "DARK": shade(col, -90), "LITE": shade(col, 70), "CREAM": "#fff3e2", "WHITE": "#ffffff",
           "BLACK": "#18121c", "PINK": "#ff96aa", "NOSE": "#301e2c", "IRIS": "#96e66e"}
    parts = []
    if bg: parts.append(f'<rect width="64" height="64" rx="14" fill="{bg}"/>')
    sh = shapes()[name]
    for pas in (1, 0):                                        # the outline pass (grown shapes in the outline colour), then the shapes
        for k, p, o, a, b, c, d, e, f in sh:
            if pas and not o: continue
            fill = "#16101a" if pas else pal[p]
            if k == "ELL":
                g = 2.2 if pas else 0
                parts.append(f'<ellipse cx="{a}" cy="{b}" rx="{c + g}" ry="{d + g}" fill="{fill}"/>')
            else:
                pts = [(a, b), (c, d), (e, f)]
                if pas:
                    cx, cy = sum(x for x, _ in pts) / 3, sum(y for _, y in pts) / 3
                    kk = 1 + 2.2 / 9
                    pts = [(cx + (x - cx) * kk, cy + (y - cy) * kk) for x, y in pts]
                parts.append('<polygon points="%s" fill="%s"/>' % (" ".join(f"{x:.1f},{y:.1f}" for x, y in pts), fill))
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="{size}" height="{size}" '
            f'role="img" aria-label="{name.title()}">' + "".join(parts) + "</svg>")

THEME_CSS = """
:root, :root[data-theme="dark"], :root:not([data-theme="light"]) {
  --ground: #141824; --ink: #f8faff; --muted: #bac4d8; --rule: #424a62; --accent: #ff5caa; --teal: #54ccff; --card: #282e40;
  --otter: #54ccff; --cat: #ffc428; --fox: #ff7650; --hopper: #8ee864; --beaver: #e2a46c; }
body { background: radial-gradient(1200px 600px at 20% -10%, #243052 0%, var(--ground) 55%) fixed, var(--ground); color: var(--ink);
  font-family: "Barlow Semi Condensed", "Segoe UI", system-ui, sans-serif; }
a { color: var(--accent); }
.ecb-nav { display: flex; gap: 18px; align-items: center; flex-wrap: wrap; margin: 0 0 22px; padding: 10px 0 14px; border-bottom: 1px solid var(--rule); }
.ecb-nav .brand { font-weight: 800; letter-spacing: .08em; color: var(--accent); text-decoration: none; font-size: 18px; display: flex; gap: 10px; align-items: center; }
.ecb-nav .brand svg { width: 28px; height: 28px; }
.ecb-nav a.link { color: var(--muted); text-decoration: none; font-weight: 600; letter-spacing: .04em; text-transform: uppercase; font-size: 13px; }
.ecb-nav a.link:hover, .ecb-nav a.link[aria-current] { color: var(--ink); }
.ecb-nav .creatures { margin-left: auto; display: flex; gap: 4px; }
.ecb-nav .creatures svg { width: 30px; height: 30px; }
h1, h2, h3 { font-weight: 800; letter-spacing: .02em; }
button, .btn { font-family: inherit; }
"""

BRAND = {"name": "EARTH CREATURE BEATS", "icon": "CAT", "beings": None}
def set_brand(app):
    """the ECB look (app) or Alien Stack's (visitors): the nav's name and colours"""
    global BRAND
    BRAND = {"name": "EARTH CREATURE BEATS", "icon": "CAT", "alien": False} if app == "app" else {"name": "SFA PROTO" if app == "proto" else "ALIEN STACK", "icon": None, "alien": True}

ALIEN_CSS = """
:root, :root[data-theme="dark"], :root:not([data-theme="light"]) { --ground: #07071a; --accent: #ff3ef0; --card: #1c1838; --rule: #3e366e; --muted: #bebae4; }
body { background: radial-gradient(900px 500px at 70% -10%, #2a1a5a 0%, #07071a 60%) fixed, #07071a; }
"""
def nav(active, depth=1):
    up = "../" * depth
    links = [("mirror/", "Mirror"), ("backup/", "Backup"), ("install/", "Install"), ("quickstart.html", "Quick start")]
    row = "".join(f'<a class="link" href="{up}{h}"{" aria-current=page" if a == active else ""}>{t}</a>' for h, t, a in
                  [(h, t, h.split("/")[0].split(".")[0]) for h, t in links])
    crit = "" if BRAND.get("alien") else "".join(svg(n, 30) for n in NAMES)
    icon = svg(BRAND["icon"], 28) if BRAND.get("icon") else '<span style="font-size:22px">&#x1F6F8;</span>'
    return (f'<nav class="ecb-nav"><a class="brand" href="{up}index.html">{icon}{BRAND["name"]}</a>{row}'
            f'<span class="creatures">{crit}</span></nav>')

def skin(html, active, depth=1):
    """fm1-base's page in ECB's clothes: the theme forced dark, the nav above its content"""
    html = html.replace("</style>", THEME_CSS + (ALIEN_CSS if BRAND.get("alien") else "") + "</style>", 1)
    if BRAND.get("alien"): html = html.replace("ALIEN Mirror", "Alien Stack Mirror").replace("running ALIEN", "running Alien Stack").replace("ALIEN Backup", "Alien Stack Backup")
    html = re.sub(r"<html([^>]*)>", lambda m: "<html" + m.group(1).replace('data-theme="light"', "") + ' data-theme="dark">' if "data-theme" not in m.group(1) else m.group(0), html, count=1)
    # the nav as the first thing in the body (after the opening tag and any <main> / <header>)
    m = re.search(r"<body[^>]*>", html)
    if m:
        html = html[:m.end()] + '<div style="max-width:1100px;margin:0 auto;padding:0 16px">' + nav(active, depth) + "</div>" + html[m.end():]
    return html

def landing(version, pkg_name, has_install=True, title=None):
    if title == "VISITORS": return landing_visitors(version, pkg_name, has_install)
    cards = "".join(f'''
      <article class="card" style="--c:{COLORS[n]}">
        <div class="art">{svg(n, 120)}</div>
        <h3><span>{n}</span> {ROLES[n]}</h3>
        <p>{BLURB[n]}</p>
      </article>''' for n in NAMES)
    install = (f'<a class="btn primary" href="install/">Install on your FM-1</a>' if has_install else "") + \
              f'<a class="btn" href="firmware/{pkg_name}">Download {pkg_name}</a>'
    return f'''<!doctype html>
<html lang="en" data-theme="dark"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Earth Creature Beats</title>
<meta name="description" content="Earth Creature Beats: a groovebox firmware for the M-VAVE FM-1 by Sin Fetish Audio.">
<link rel="icon" href="data:image/svg+xml,{svg('CAT', 32).replace('#', '%23').replace('"', "'")}">
<style>
{THEME_CSS}
* {{ box-sizing: border-box; }}
body {{ margin: 0; padding: 0 16px 60px; }}
.wrap {{ max-width: 1100px; margin: 0 auto; }}
.hero {{ display: grid; grid-template-columns: 1.2fr 1fr; gap: 32px; align-items: center; padding: 30px 0 10px; }}
.hero h1 {{ font-size: clamp(38px, 6vw, 68px); line-height: .95; margin: 0 0 14px; }}
.hero h1 em {{ font-style: normal; color: var(--accent); }}
.hero p.lead {{ font-size: 19px; color: var(--muted); max-width: 54ch; }}
.hero .shelf {{ display: grid; grid-template-columns: repeat(5, 1fr); gap: 10px; }}
.hero .shelf div {{ background: linear-gradient(180deg, #2e3650, #222838); border: 1px solid var(--rule); border-radius: 16px; padding: 10px 6px 8px; text-align: center; }}
.hero .shelf svg {{ width: 100%; height: auto; }}
.hero .shelf b {{ display: block; font-size: 11px; letter-spacing: .1em; margin-top: 6px; }}
.actions {{ display: flex; gap: 12px; flex-wrap: wrap; margin: 22px 0 0; }}
.btn {{ display: inline-block; padding: 12px 18px; border-radius: 12px; border: 1px solid var(--rule); color: var(--ink); text-decoration: none; font-weight: 700; letter-spacing: .04em; background: var(--card); }}
.btn.primary {{ background: var(--accent); border-color: var(--accent); color: #1a0a12; }}
.btn:hover {{ filter: brightness(1.1); }}
.grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin: 36px 0; }}
.card {{ background: linear-gradient(180deg, #2b3246, #20263a); border: 1px solid var(--rule); border-top: 3px solid var(--c); border-radius: 18px; padding: 18px; }}
.card .art {{ display: flex; justify-content: center; margin-bottom: 8px; }}
.card h3 {{ margin: 6px 0; font-size: 16px; letter-spacing: .04em; }}
.card h3 span {{ color: var(--c); margin-right: 6px; }}
.card p {{ color: var(--muted); font-size: 14px; line-height: 1.45; margin: 0; }}
.strip {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 16px; }}
.strip section {{ background: var(--card); border: 1px solid var(--rule); border-radius: 16px; padding: 16px 18px; }}
.strip h2 {{ font-size: 15px; letter-spacing: .1em; text-transform: uppercase; color: var(--accent); margin: 0 0 10px; }}
.strip li {{ color: var(--muted); margin: 4px 0; }}
.strip li b {{ color: var(--ink); }}
kbd {{ background: #11151f; border: 1px solid var(--rule); border-radius: 6px; padding: 1px 6px; font-family: inherit; font-weight: 700; color: var(--ink); }}
footer {{ color: var(--muted); font-size: 13px; margin-top: 40px; border-top: 1px solid var(--rule); padding-top: 14px; }}
@media (max-width: 800px) {{ .hero {{ grid-template-columns: 1fr; }} }}
</style></head>
<body><div class="wrap">
{nav("index", 0)}
<section class="hero">
  <div>
    <h1>Five creatures.<br><em>One groovebox.</em></h1>
    <p class="lead">Earth Creature Beats turns the M-VAVE FM-1 into a four-track groovebox: a polysynth, a sample drum
    machine, 808s and basses, a slicer that samples itself, and a sequencer with scenes and songs. Clean sound,
    a bright cartoon UI, and a laptop mirror so you can see it big.</p>
    <div class="actions">{install}<a class="btn" href="quickstart.html">Quick start</a><a class="btn" href="mirror/">Open the mirror</a></div>
    <p style="color:var(--muted);font-size:13px;margin-top:14px">Version {version} · for the FM-1 · built on fm1-base · samples original, CC0</p>
  </div>
  <div class="shelf">{"".join(f'<div style="border-top:3px solid {COLORS[n]}">{svg(n, 120)}<b style="color:{COLORS[n]}">{n}</b></div>' for n in NAMES)}</div>
</section>
<section class="grid">{cards}</section>
<section class="strip">
  <section><h2>Make a beat in a minute</h2><ul>
    <li><kbd>HOME</kbd> key <b>7</b>: a fresh beat on all four tracks, playing.</li>
    <li>Tap a creature's button: play it on the keys. <b>ALGO</b> browses its presets, kits, sounds or samples.</li>
    <li><kbd>REC</kbd> while stopped: step entry. While playing: arm the track and play along.</li>
    <li><kbd>SEQ</kbd>: the Beaver. Hold a step + K1-K4 for velocity, length, probability; + <b>GEN</b> for ratchets.</li>
  </ul></section>
  <section><h2>Scenes and songs</h2><ul>
    <li><kbd>EDIT</kbd> mixer keys <b>5-8, 13-16</b>: hold to store a scene (pattern, presets, kit, mix), tap to recall.</li>
    <li>8 patterns A-H on the ALGO knob; <b>CHAIN</b> + keys writes a 16-slot song; tap CHAIN to run it.</li>
    <li>Hold a creature's button and turn K1 / K2: its level / pan, from anywhere.</li>
  </ul></section>
  <section><h2>Your own sounds</h2><ul>
    <li>Cat's <b>PAD+</b> page: any of 96 samples on any pad, with punch, snap and crush. Build kits.</li>
    <li>Hopper: <kbd>ARP</kbd> samples the output; the ALGO knob loads your own WAVs (tools/sfa_sample.py).</li>
    <li>The mixer's MASTER page: glue, low end, air, drive.</li>
  </ul></section>
</section>
<footer>Earth Creature Beats by Sin Fetish Audio · GPL-3.0 (the app) on fm1-base (MPL-2.0) · not affiliated with M-VAVE.
Unit A only: install on the FM-1 you set aside for custom firmware.</footer>
</div></body></html>'''

def quickstart():
    rows = [
        ("FX / SEL / ENV / LFO", "Otter / Cat / Fox / Hopper: that track on the keys; again: next page; hold + K1/K2: level / pan"),
        ("SEQ", "the Beaver (patterns, steps, song)"), ("EDIT", "the mixer: levels, pans, FX, MASTER; keys 1-4 mute, 9-12 solo, 5-8 & 13-16 scenes"),
        ("HOME", "the home screen: five tiles, keys 1-6 jump, key 7 = NEW BEAT"), ("PLAY", "run / stop"),
        ("REC", "stopped: step entry on the focused track (OCT+ rest, OCT- back); playing: arm it for live record; hold 1 s: clear it"),
        ("GLO", "tap tempo"), ("ARP", "Hopper: sample the output until ARP again; it lands chopped"), ("SAVE", "keep everything"),
        ("PRESET knob", "page"), ("ALGO knob", "preset / kit / 808 & bass / sample source / pattern"), ("SELECT knob", "track"),
        ("K1-K4", "always the four things on the screen"),
        ("Beaver white keys", "toggle the focused track's steps; hold one + K1-K4 = NOTE VEL LENGTH PROB, + GEN = ratchet x1-x4"),
        ("Beaver black keys", "COPY PASTE CLR CHAIN BEAT M1 M2 M3 M4 GEN SOLO (CHAIN held + keys 1-8 writes the song, + CLR clears it)"),
    ]
    table = "".join(f"<tr><th>{a}</th><td>{b}</td></tr>" for a, b in rows)
    return f'''<!doctype html><html lang="en" data-theme="dark"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ECB Quick start</title><style>{THEME_CSS}
body {{ margin: 0; padding: 0 16px 60px; }} .wrap {{ max-width: 1000px; margin: 0 auto; }}
table {{ border-collapse: collapse; width: 100%; }} th, td {{ text-align: left; padding: 9px 10px; border-bottom: 1px solid var(--rule); vertical-align: top; }}
th {{ color: var(--accent); white-space: nowrap; width: 200px; letter-spacing: .04em; }} td {{ color: var(--muted); }}
h1 {{ margin: 10px 0 6px; }} p {{ color: var(--muted); }}
</style></head><body><div class="wrap">{nav("quickstart", 0)}
<h1>Quick start</h1><p>Everything is on the device; this is the map. Unit A only.</p>
<table>{table}</table>
<h2>Flashing</h2><p>In the Claude session's workflow: <code>wsl -d Ubuntu -e bash -lc "cd ~/sfa-fm1 &amp;&amp; git pull &amp;&amp; tools/go.sh"</code> with the FM-1 alone on USB.
From this site: <a href="install/">Install</a> works in Chrome or Edge over Web MIDI. Your own samples: <code>python3 tools/sfa_sample.py mydir/</code>, then flash.</p>
</div></body></html>'''

def landing_visitors(version, pkg_name, has_install=True):
    """VISITORS' front page: the same bones, a cosmic skin, the beings named (their art comes with the visitors site pass)"""
    beings = [("NORDICS", "Poly synth", "#ffd86a", "A 4-voice polysynth of pads, choirs, leads and arps."),
              ("MANTIS", "Drum synth", "#38e0c0", "Sixteen synthesized drum voices, eight kits, every voice shaped per pad. No samples."),
              ("REPTILES", "Bass & lead", "#58ff7a", "Sixteen chromatic alien one-shots: growls, folded subs, lasers, vowels; plus 24 synth basses."),
              ("GREYS", "Sampler", "#c8d0e8", "Sample the output or load your own; chop by transients; play the slices. Drone, transmission, sweep and choir on board."),
              ("SHIP", "Sequencer", "#c05cff", "8 patterns, scenes, a 16-slot song, ratchets, probability, groove generators, step entry and live record.")]
    cards = "".join(f'<article class="card" style="--c:{c}"><h3><span>{n}</span> {r}</h3><p>{b}</p></article>' for n, r, c, b in beings)
    install = (f'<a class="btn primary" href="install/">Install on your FM-1</a>' if has_install else "") + f'<a class="btn" href="firmware/{pkg_name}">Download {pkg_name}</a>'
    return f"""<!doctype html><html lang="en" data-theme="dark"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Alien Stack</title><style>{THEME_CSS}
:root {{ --ground: #07071a; --accent: #ff3ef0; --card: #1c1838; --rule: #3e366e; }}
body {{ margin: 0; padding: 0 16px 60px; background: radial-gradient(900px 500px at 70% -10%, #2a1a5a 0%, #07071a 60%) fixed, #07071a; }}
.wrap {{ max-width: 1100px; margin: 0 auto; }} h1 {{ font-size: clamp(40px, 7vw, 76px); line-height: .95; margin: 30px 0 12px; }} h1 em {{ font-style: normal; color: var(--accent); }}
p.lead {{ font-size: 19px; color: var(--muted); max-width: 60ch; }}
.actions {{ display: flex; gap: 12px; flex-wrap: wrap; margin: 22px 0; }}
.btn {{ display: inline-block; padding: 12px 18px; border-radius: 12px; border: 1px solid var(--rule); color: var(--ink); text-decoration: none; font-weight: 700; background: var(--card); }}
.btn.primary {{ background: var(--accent); border-color: var(--accent); color: #1a0a18; }}
.grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin: 30px 0; }}
.card {{ background: linear-gradient(180deg, #241e48, #171334); border: 1px solid var(--rule); border-top: 3px solid var(--c); border-radius: 18px; padding: 18px; }}
.card h3 {{ margin: 0 0 8px; font-size: 16px; }} .card h3 span {{ color: var(--c); margin-right: 6px; }} .card p {{ color: var(--muted); font-size: 14px; line-height: 1.45; margin: 0; }}
footer {{ color: var(--muted); font-size: 13px; margin-top: 40px; border-top: 1px solid var(--rule); padding-top: 14px; }}
</style></head><body><div class="wrap">
<h1>ALIEN <em>STACK</em></h1>
<p class="lead">Alien Stack is a second firmware for the M-VAVE FM-1 from the people behind Earth Creature Beats: a synthesized
drum machine, an alien bass bank, a polysynth, a sampler that samples itself, and a ship that sequences them all, with
scenes and a song, parameter locks on every step, polymeter, and a reverb bus. Same hardware, another world.</p>
<div class="actions">{install}<a class="btn" href="quickstart.html">Quick start</a><a class="btn" href="mirror/">Open the mirror</a></div>
<p style="color:var(--muted);font-size:13px">Version {version} · for the FM-1 · built on fm1-base · all audio original, CC0</p>
<section class="grid">{cards}</section>
<footer>Alien Stack by Sin Fetish Audio · GPL-3.0 (the app) on fm1-base (MPL-2.0) · not affiliated with M-VAVE. One firmware per unit.</footer>
</div></body></html>"""
