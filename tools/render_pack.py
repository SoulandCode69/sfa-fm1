#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Earth Creature Beats: the factory sample pack, rendered offline (numpy, 44.1 kHz, then written as 22.05 kHz WAVs).
# Original synthesis by Sin Fetish Audio; the rendered samples are CC0 (public domain).
#   python3 tools/render_pack.py            -> assets/pack/<kit>/<nn_name>.wav  and  assets/pack/808/<nn_name>.wav
import math, os, struct, wave
import numpy as np
from pathlib import Path

SR = 44100
OUT = Path(__file__).resolve().parents[1] / "assets" / "pack"
rng = np.random.default_rng(808)

def t(sec): return np.arange(int(sec * SR)) / SR
def env_exp(sec, tau): x = t(sec); return np.exp(-x / tau)
def env_lin_exp(sec, attack, tau):
    x = t(sec); e = np.exp(-(x - attack).clip(0) / tau); e[x < attack] = x[x < attack] / attack; return e
def sine(hz, sec): return np.sin(2 * np.pi * hz * t(sec))
def sine_sweep(f0, f1, tau, sec):
    x = t(sec); f = f1 + (f0 - f1) * np.exp(-x / tau); ph = np.cumsum(f) / SR; return np.sin(2 * np.pi * ph)
def tri_sweep(f0, f1, tau, sec):
    x = t(sec); f = f1 + (f0 - f1) * np.exp(-x / tau); ph = (np.cumsum(f) / SR) % 1.0; return 4 * np.abs(ph - 0.5) - 1
def square(hz, sec): return np.sign(np.sin(2 * np.pi * hz * t(sec)) + 1e-9)
def noise(sec): return rng.standard_normal(int(sec * SR)) * 0.35
def onepole_lp(x, hz):
    a = 1 - math.exp(-2 * math.pi * hz / SR); y = np.empty_like(x); s = 0.0
    for i in range(len(x)): s += (x[i] - s) * a; y[i] = s
    return y
def onepole_hp(x, hz): return x - onepole_lp(x, hz)
def svf_bp(x, hz, q):                       # trapezoidal SVF bandpass
    g = math.tan(math.pi * hz / SR); k = 1 / q; a1 = 1 / (1 + g * (g + k)); a2 = g * a1
    ic1 = ic2 = 0.0; y = np.empty_like(x)
    for i in range(len(x)):
        v3 = x[i] - ic2; v1 = a1 * ic1 + a2 * v3; v2 = ic2 + a2 * ic1 + g * a2 * v3
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; y[i] = v1
    return y
def svf_lp(x, hz, q):
    g = math.tan(math.pi * hz / SR); k = 1 / q; a1 = 1 / (1 + g * (g + k)); a2 = g * a1
    ic1 = ic2 = 0.0; y = np.empty_like(x)
    for i in range(len(x)):
        v3 = x[i] - ic2; v1 = a1 * ic1 + a2 * v3; v2 = ic2 + a2 * ic1 + g * a2 * v3
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; y[i] = v2
    return y
def sat(x, drive): return np.tanh(x * drive) / math.tanh(drive)
def fade_out(x, ms=5):
    n = int(ms * SR / 1000); x[-n:] *= np.linspace(1, 0, n); return x
def norm(x, peak=0.89):                     # -1 dB
    m = np.abs(x).max() or 1.0; return x * (peak / m)
def metal(sec, rate=1.0):                   # the 808's six squares
    hz = [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]
    return sum(square(h * rate, sec) for h in hz) / 6

# ---------------------------------------------------------------- the drums
def kick(f0=190, f1=50, ptau=0.03, atau=0.28, sec=0.7, click=0.3, drive=1.6, tri=False, sub=0.0, beater=0.25, harm=0.18):
    """a kick: the pitched body (saturated), a 2nd-harmonic layer for clarity that dies faster, a beater blip and a
    filtered click for the attack, then a gentle glue so the transient and the tail sit together"""
    body = (tri_sweep if tri else sine_sweep)(f0, f1, ptau, sec)
    body = sat(body, drive) * env_exp(sec, atau)
    body += sine_sweep(f0 * 2, f1 * 2, ptau, sec) * env_exp(sec, atau * 0.35) * harm
    if sub: body += sine(f1 * 0.5, sec) * env_exp(sec, atau * 1.2) * sub
    ck = onepole_lp(noise(sec), 4000) * env_exp(sec, 0.004) * click * 2.5
    bt = sine(1400, sec) * env_exp(sec, 0.0015) * beater
    return fade_out(norm(sat(body + ck + bt, 1.15)))

def snare(tone=190, tone2=330, ttau=0.09, ntau=0.16, nhp=600, nband=2200, snap=0.9, sec=0.45, drive=1.2, crack=0.5):
    """a snare: two body modes with a quick pitch drop, the wires as shaped noise, and a 4 ms crack on the attack"""
    tn = (sine_sweep(tone * 1.35, tone, 0.012, sec) + 0.6 * sine_sweep(tone2 * 1.2, tone2, 0.01, sec)) * env_exp(sec, ttau)
    tn = sat(tn, drive)
    n = noise(sec); n = onepole_hp(onepole_lp(n, 7000), nhp)
    n = (n * 0.7 + svf_bp(n, nband, 1.2) * 1.3) * env_exp(sec, ntau) * snap
    cr = svf_bp(noise(sec), 3500, 1.0) * 3 * env_exp(sec, 0.004) * crack
    return fade_out(norm(sat(tn * 0.75 + n * 0.9 + cr, 1.1)))

def hat(sec=0.12, tau=0.03, bp=9000, hp=7000, nz=0.3, rate=1.0, open_=False):
    m = svf_bp(metal(sec, rate), bp, 1.5) * 2.0
    m = onepole_hp(m, hp)
    m = m + onepole_hp(noise(sec), 8000) * nz
    e = env_exp(sec, tau) if not open_ else env_lin_exp(sec, 0.002, tau)
    return fade_out(norm(m * e), 3)

def clap(bp=1200, q=1.6, tail=0.18, sec=0.45, bursts=3, gap=0.010):
    x = t(sec); n = svf_bp(noise(sec), bp, q) * 2.5
    e = np.zeros_like(x)
    for b in range(bursts):
        e += np.exp(-(x - b * gap).clip(0) / 0.006) * (x >= b * gap)
    e += np.exp(-(x - bursts * gap).clip(0) / tail) * (x >= bursts * gap) * 0.9
    return fade_out(norm(n * e))

def tom(f0=170, f1=95, ptau=0.05, tau=0.3, sec=0.6, nz=0.08, tri=False):
    body = (tri_sweep if tri else sine_sweep)(f0, f1, ptau, sec) * env_exp(sec, tau)
    body = sat(body, 1.3)
    n = onepole_lp(noise(sec), 3000) * env_exp(sec, 0.015) * nz
    return fade_out(norm(body + n))

def cowbell(f1=587, f2=845, bp=2600, tau=0.18, sec=0.5):
    x = (square(f1, sec) + square(f2, sec)) * 0.5
    return fade_out(norm(svf_bp(x, bp, 2.5) * 2 * env_exp(sec, tau)))

def click_perc(hz=2500, tau=0.025, sec=0.2, q=18, nz=0.0):
    imp = np.zeros(int(sec * SR)); imp[:40] = 1.0
    x = svf_bp(imp + noise(sec) * nz * env_exp(sec, 0.01), hz, q) * 6
    return fade_out(norm(x * env_exp(sec, tau)))

def shaker(sec=0.14, tau=0.035, hp=6000):
    n = onepole_hp(onepole_hp(noise(sec), hp), hp)
    return fade_out(norm(n * env_lin_exp(sec, 0.004, tau)))

def crash(sec=1.8, tau=0.5, bp=6500):
    m = svf_bp(metal(sec, 1.6), bp, 1.0) * 2 + onepole_hp(noise(sec), 5000) * 0.6
    return fade_out(norm(m * env_lin_exp(sec, 0.003, tau)), 20)

def zap(f0=3000, f1=60, tau=0.08, atau=0.25, sec=0.5, sq=False):
    x = t(sec); f = f1 + (f0 - f1) * np.exp(-x / tau); ph = np.cumsum(f) / SR
    o = np.sign(np.sin(2 * np.pi * ph)) if sq else np.sin(2 * np.pi * ph)
    return fade_out(norm(o * env_exp(sec, atau)))

def eight08(note_hz=41.2, sec=1.2, ptau=0.02, f0mul=2.5, tau=0.45, drive=1.4, click=0.15):
    body = sine_sweep(note_hz * f0mul, note_hz, ptau, sec) * env_exp(sec, tau)
    body = sat(body, drive)
    ck = onepole_lp(noise(sec), 3000) * env_exp(sec, 0.003) * click * 2
    return fade_out(norm(body + ck), 15)

def sub(note_hz=41.2, sec=1.2, tau=0.5):
    return fade_out(norm(sine(note_hz, sec) * env_lin_exp(sec, 0.004, tau)), 15)

def bass_pluck(note_hz=55, sec=0.8, tau=0.18, cutoff=900):
    x = t(sec); saw = 2 * ((note_hz * x) % 1.0) - 1
    y = svf_lp(saw, cutoff, 1.2) * env_exp(sec, tau)
    return fade_out(norm(sat(y, 1.5) + sine(note_hz * 0.5, sec) * env_exp(sec, tau) * 0.5))

def fm_bass(note_hz=55, sec=0.9, tau=0.25, ratio=1.0, idx=3.0):
    x = t(sec); e = env_exp(sec, tau)
    m = np.sin(2 * np.pi * note_hz * ratio * x) * idx * e
    return fade_out(norm(np.sin(2 * np.pi * note_hz * x + m) * e))

# ---------------------------------------------------------------- the kits: 16 pads each
KITS = {
    "CLASSIC": [
        ("kick", lambda: kick(190, 50, 0.03, 0.30, 0.8, 0.25, 1.6)),
        ("boom", lambda: kick(150, 42, 0.08, 0.5, 1.0, 0.1, 1.3, sub=0.4)),
        ("snare", lambda: snare(180, 330, 0.09, 0.17, 600, 2200, 0.85)),
        ("rim", lambda: click_perc(1750, 0.02, 0.15, 14, 0.3)),
        ("clap", lambda: clap(1100, 1.6, 0.18)),
        ("chat", lambda: hat(0.12, 0.028, 9000, 7000, 0.25)),
        ("ohat", lambda: hat(0.45, 0.14, 9000, 7000, 0.25, open_=True)),
        ("cymbal", lambda: crash(1.1, 0.35, 6500)),
        ("lotom", lambda: tom(160, 88, 0.05, 0.3)),
        ("midtom", lambda: tom(220, 130, 0.05, 0.25)),
        ("hitom", lambda: tom(310, 185, 0.04, 0.2)),
        ("cowbell", lambda: cowbell()),
        ("clave", lambda: click_perc(2500, 0.03, 0.15, 25)),
        ("maracas", lambda: shaker(0.1, 0.025)),
        ("conga", lambda: tom(420, 370, 0.015, 0.14, 0.4, 0.04)),
        ("subdrop", lambda: zap(90, 28, 0.5, 0.5, 1.0)),
    ],
    "PUNCH": [
        ("kick", lambda: kick(300, 56, 0.025, 0.18, 0.6, 0.9, 2.2, tri=True)),
        ("kick-tight", lambda: kick(260, 60, 0.015, 0.11, 0.4, 1.0, 2.8, tri=True)),
        ("snare", lambda: snare(230, 185, 0.07, 0.13, 900, 2800, 1.0, 0.4, 1.5)),
        ("snare-dry", lambda: snare(180, 170, 0.05, 0.08, 1000, 2400, 0.6, 0.3, 1.6)),
        ("clap", lambda: clap(1500, 1.8, 0.14)),
        ("chat", lambda: hat(0.1, 0.022, 10000, 8000, 0.5, 1.25)),
        ("ohat", lambda: hat(0.5, 0.15, 10000, 8000, 0.5, 1.25, open_=True)),
        ("ride", lambda: crash(1.2, 0.4, 7500)),
        ("lotom", lambda: tom(180, 95, 0.045, 0.24, 0.5, 0.15, tri=True)),
        ("midtom", lambda: tom(250, 140, 0.045, 0.2, 0.45, 0.15, tri=True)),
        ("hitom", lambda: tom(340, 200, 0.035, 0.16, 0.4, 0.15, tri=True)),
        ("rim", lambda: click_perc(1800, 0.012, 0.12, 10, 0.5)),
        ("bell", lambda: cowbell(800, 1130, 3000, 0.25)),
        ("shaker", lambda: shaker(0.12, 0.03, 7000)),
        ("zap", lambda: zap(900, 300, 0.05, 0.08, 0.3, True)),
        ("swell", lambda: fade_out(norm(svf_bp(noise(0.7), 500, 0.8) * 3 * env_lin_exp(0.7, 0.25, 0.18)), 30)),
    ],
    "LOFI": [
        ("kick", lambda: kick(140, 46, 0.02, 0.14, 0.4, 0.3, 3.5, tri=True)),
        ("kick-crushed", lambda: kick(100, 42, 0.04, 0.28, 0.6, 0.0, 4.0)),
        ("snare", lambda: snare(170, 160, 0.05, 0.09, 700, 1800, 0.9, 0.3, 2.5)),
        ("snap", lambda: fade_out(norm(svf_bp(noise(0.15), 2500, 2.0) * 3 * env_exp(0.15, 0.03)))),
        ("clap", lambda: clap(1000, 1.5, 0.11, 0.35)),
        ("chat", lambda: shaker(0.08, 0.018, 7000)),
        ("ohat", lambda: shaker(0.4, 0.12, 6500)),
        ("wash", lambda: fade_out(norm(svf_bp(noise(0.8), 5000, 0.6) * 2.5 * env_exp(0.8, 0.25)), 30)),
        ("conga-lo", lambda: tom(230, 200, 0.02, 0.17, 0.45, 0.04)),
        ("conga-hi", lambda: tom(330, 300, 0.02, 0.14, 0.4, 0.04)),
        ("bongo", lambda: tom(520, 470, 0.015, 0.09, 0.3, 0.04)),
        ("cowbell", lambda: cowbell(660, 950, 2200, 0.13)),
        ("woodblock", lambda: click_perc(2200, 0.018, 0.12, 20, 0.3)),
        ("tek", lambda: click_perc(1200, 0.04, 0.2, 12, 0.1)),
        ("tabla", lambda: tom(90, 85, 0.1, 0.3, 0.6, 0.0)),
        ("tick", lambda: fade_out(norm(svf_bp(noise(0.06), 300, 1.0) * 4 * env_exp(0.06, 0.008)))),
    ],
}

KITS["TECHNO"] = [
    ("kick", lambda: kick(240, 48, 0.02, 0.26, 0.6, 0.5, 2.6, tri=True, beater=0.4, harm=0.25)),
    ("kick-rumble", lambda: fade_out(norm(kick(200, 46, 0.03, 0.4, 0.8, 0.2, 2.0) + onepole_lp(noise(0.8), 180) * env_lin_exp(0.8, 0.03, 0.3) * 1.2))),
    ("snare", lambda: snare(210, 180, 0.05, 0.1, 1200, 3000, 1.0, 0.3, 1.8, 0.8)),
    ("clap", lambda: clap(1600, 2.0, 0.1, 0.3, 3, 0.008)),
    ("chat", lambda: hat(0.07, 0.016, 11000, 9000, 0.6, 1.35)),
    ("ohat", lambda: hat(0.35, 0.1, 11000, 9000, 0.6, 1.35, open_=True)),
    ("ride", lambda: crash(1.0, 0.35, 8000)),
    ("zap", lambda: zap(2400, 120, 0.04, 0.06, 0.25, True)),
    ("lotom", lambda: tom(150, 70, 0.03, 0.18, 0.4, 0.1, tri=True)),
    ("midtom", lambda: tom(210, 110, 0.03, 0.15, 0.35, 0.1, tri=True)),
    ("hitom", lambda: tom(300, 170, 0.025, 0.12, 0.3, 0.1, tri=True)),
    ("rim", lambda: click_perc(2100, 0.01, 0.1, 9, 0.6)),
    ("stab", lambda: click_perc(900, 0.05, 0.25, 8, 0.2)),
    ("hit", lambda: fade_out(norm(svf_bp(noise(0.3), 1800, 0.7) * 3 * env_exp(0.3, 0.05)))),
    ("riser", lambda: fade_out(norm((lambda x: np.sin(2 * np.pi * np.cumsum(200 + 2800 * x / 0.5) / SR))(t(0.5)) * env_lin_exp(0.5, 0.4, 0.05)), 20)),
    ("impact", lambda: fade_out(norm(kick(90, 34, 0.1, 0.5, 0.8, 0.3, 1.8, sub=0.5) + onepole_lp(noise(0.8), 900) * env_exp(0.8, 0.12) * 0.8), 20)),
]
KITS["HOUSE"] = [
    ("kick", lambda: kick(175, 52, 0.035, 0.3, 0.6, 0.25, 1.5, beater=0.2, harm=0.15)),
    ("kick-click", lambda: kick(220, 56, 0.02, 0.16, 0.4, 0.8, 1.8, beater=0.5)),
    ("snare", lambda: snare(200, 330, 0.08, 0.2, 500, 2400, 0.9, 0.45, 1.3, 0.6)),
    ("clap", lambda: clap(1100, 1.2, 0.25, 0.5, 4, 0.011)),
    ("chat", lambda: hat(0.09, 0.02, 9500, 7500, 0.3, 1.1)),
    ("ohat", lambda: hat(0.5, 0.16, 9500, 7500, 0.3, 1.1, open_=True)),
    ("ride", lambda: fade_out(norm(svf_bp(metal(1.0, 2.0), 5500, 1.6) * 2 * env_lin_exp(1.0, 0.003, 0.35)), 20)),
    ("crash", lambda: crash(1.0, 0.3, 6000)),
    ("lotom", lambda: tom(165, 90, 0.05, 0.28, 0.5, 0.1)),
    ("midtom", lambda: tom(230, 135, 0.05, 0.22, 0.45, 0.1)),
    ("hitom", lambda: tom(320, 190, 0.04, 0.18, 0.4, 0.1)),
    ("rim", lambda: click_perc(1700, 0.015, 0.12, 12, 0.4)),
    ("cowbell", lambda: cowbell(562, 845, 2400, 0.2)),
    ("shaker", lambda: shaker(0.11, 0.028, 6500)),
    ("conga", lambda: tom(400, 360, 0.015, 0.15, 0.4, 0.04)),
    ("clave", lambda: click_perc(2600, 0.028, 0.15, 24)),
]

# the bass / 808 one-shots: chromatic, root C (recorded at C1 = 32.7 Hz and C2 = 65.4 Hz as marked)
EIGHT08 = [
    ("808-long", lambda: eight08(32.7, 1.3, 0.02, 2.5, 0.5, 1.4, 0.15), 24),
    ("808-short", lambda: eight08(32.7, 0.8, 0.015, 2.5, 0.25, 1.6, 0.25), 24),
    ("808-dist", lambda: eight08(32.7, 1.2, 0.02, 3.0, 0.45, 3.5, 0.3), 24),
    ("808-boom", lambda: eight08(32.7, 1.3, 0.05, 2.0, 0.6, 1.2, 0.05), 24),
    ("808-punch", lambda: eight08(32.7, 1.0, 0.012, 4.0, 0.3, 2.0, 0.6), 24),
    ("808-soft", lambda: eight08(32.7, 1.2, 0.03, 1.8, 0.5, 1.1, 0.0), 24),
    ("sub-sine", lambda: sub(32.7, 1.2, 0.5), 24),
    ("sub-short", lambda: sub(32.7, 0.6, 0.18), 24),
    ("sub-tri", lambda: fade_out(norm(tri_sweep(32.7, 32.7, 1, 1.2) * env_lin_exp(1.2, 0.004, 0.45)), 15), 24),
    ("bass-pluck", lambda: bass_pluck(65.4, 0.8, 0.18, 900), 36),
    ("bass-pluck-dark", lambda: bass_pluck(65.4, 0.9, 0.25, 450), 36),
    ("bass-saw", lambda: bass_pluck(65.4, 1.0, 0.5, 1800), 36),
    ("fm-bass", lambda: fm_bass(65.4, 0.9, 0.25, 1.0, 3.0), 36),
    ("fm-bass-bright", lambda: fm_bass(65.4, 0.8, 0.2, 2.0, 4.0), 36),
    ("fm-bass-wood", lambda: fm_bass(65.4, 0.7, 0.15, 1.41, 2.5), 36),
    ("reese", lambda: fade_out(norm(svf_lp((2 * ((65.4 * t(1.2)) % 1) - 1) + (2 * ((66.1 * t(1.2)) % 1) - 1), 700, 1.0) * env_lin_exp(1.2, 0.01, 0.6)), 20), 36),
]

def write_wav(path, x, sr_out=22050):
    # lowpass at 9.5 kHz then decimate by 2 (SR 44100 -> 22050)
    y = svf_lp(x, 9500, 0.7)
    y = y[::2]
    y = np.clip(y, -1, 1)
    path.parent.mkdir(parents=True, exist_ok=True)
    w = wave.open(str(path), "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr_out)
    w.writeframes(struct.pack("<%dh" % len(y), *(y * 32767).astype(np.int16))); w.close()

def main():
    total = 0
    for kit, pads in KITS.items():
        for i, (name, fn) in enumerate(pads):
            x = fn(); p = OUT / kit / f"{i:02d}_{name}.wav"; write_wav(p, x); total += len(x) // 2
            print(f"{kit:5s} {i:2d} {name:14s} {len(x) / SR:5.2f} s")
    for i, (name, fn, root) in enumerate(EIGHT08):
        x = fn(); p = OUT / "808" / f"{i:02d}_{name}_r{root}.wav"; write_wav(p, x); total += len(x) // 2
        print(f"808   {i:2d} {name:14s} {len(x) / SR:5.2f} s  root {root}")
    print(f"total {total / 22050:.1f} s at 22.05 kHz = {total / 1024:.0f} KB as 8-bit mu-law")

if __name__ == "__main__":
    main()
