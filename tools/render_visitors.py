#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""VISITORS' factory audio, synthesised here (original, CC0), on tools/render_pack.py's primitives:
  assets/vpack/REPTILE/   16 chromatic bass / lead one-shots for the Reptilians (root C1 = 24 or C2 = 36)
  assets/vuser/           the Greys' default user bank: a drone, a transmission, a sweep, a choir (tools/sfa_sample.py)
    python3 tools/render_visitors.py && python3 tools/pack_samples.py --pack assets/vpack --out visitors/samples_data.c
    python3 tools/sfa_sample.py assets/vuser --out visitors/samples.bin"""
import math, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
import render_pack as R   # noqa: E402
SR = R.SR
ROOT = Path(__file__).resolve().parents[1]
VP, VU = ROOT / "assets" / "vpack", ROOT / "assets" / "vuser"

def fm(carrier, ratio, index, sec, itau, atau=0.4):
    x = R.t(sec); idx = index * np.exp(-x / itau)
    mod = np.sin(2 * np.pi * carrier * ratio * x)
    return np.sin(2 * np.pi * carrier * x + idx * mod) * np.exp(-x / atau)

def saw(hz, sec): return 2 * ((hz * R.t(sec)) % 1.0) - 1
def square(hz, sec, pw=0.5): return np.where((hz * R.t(sec)) % 1.0 < pw, 1.0, -1.0)
def fold(x, amt): return np.sin(x * amt * math.pi / 2)
def lfo_lp(x, hz_lo, hz_hi, rate, q=1.0):
    """an SVF lowpass whose cutoff follows an LFO, sample by sample"""
    g_arr = np.tan(np.pi * np.clip(hz_lo + (hz_hi - hz_lo) * (0.5 + 0.5 * np.sin(2 * np.pi * rate * R.t(len(x) / SR))), 20, 18000) / SR)
    ic1 = ic2 = 0.0; k = 1 / q; out = np.empty_like(x)
    for i in range(len(x)):
        g = g_arr[i]; a1 = 1 / (1 + g * (g + k)); a2 = g * a1
        v3 = x[i] - ic2; v1 = a1 * ic1 + a2 * v3; v2 = ic2 + a2 * ic1 + g * a2 * v3
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; out[i] = v2
    return out
def formant(x, pairs):
    out = np.zeros_like(x)
    for hz, q, g in pairs: out += R.svf_bp(x, hz, q) * g
    return out
C1, C2 = 32.7, 65.4
REPTILE = [
    ("growl",      lambda: R.fade_out(R.norm(R.sat(fm(C1, 2.0, 4.0, 1.3, 0.5, 0.5) + 0.5 * R.sine(C1, 1.3) * R.env_exp(1.3, 0.5), 2.2)), 20), 24),
    ("acid-hit",   lambda: R.fade_out(R.norm(R.sat(R.svf_lp(saw(C2, 0.6), 900, 3.0) * R.env_exp(0.6, 0.18), 2.0))), 36),
    ("sub-fold",   lambda: R.fade_out(R.norm(fold(R.sine(C1, 1.2), 1.0 + 2.5 * R.env_exp(1.2, 0.25)) * R.env_lin_exp(1.2, 0.004, 0.5)), 20), 24),
    ("laser-bass", lambda: R.fade_out(R.norm(R.sat(R.sine_sweep(C2 * 8, C2, 0.06, 0.8) * R.env_exp(0.8, 0.3), 1.8))), 36),
    ("metal-bass", lambda: R.fade_out(R.norm(R.sat(fm(C2, 3.01, 3.0, 0.9, 0.12, 0.3) + fm(C2 * 0.5, 1.0, 1.0, 0.9, 0.3, 0.4) * 0.8, 1.6))), 36),
    ("vowel-bass", lambda: R.fade_out(R.norm(formant(saw(C2, 0.9), ((400, 6, 1.0), (900, 8, 0.7), (2400, 9, 0.3))) * R.env_exp(0.9, 0.3) + R.sine(C2 * 0.5, 0.9) * R.env_exp(0.9, 0.3) * 0.5)), 36),
    ("reese-hit",  lambda: R.fade_out(R.norm(R.svf_lp(saw(C2, 1.0) + saw(C2 * 1.007, 1.0) + saw(C2 * 0.993, 1.0), 700, 1.0) * R.env_exp(1.0, 0.35)), 20), 36),
    ("pluck-fm",   lambda: R.fade_out(R.norm(fm(C2 * 2, 1.0, 6.0, 0.6, 0.05, 0.18))), 48),
    ("drop-sub",   lambda: R.fade_out(R.norm(R.sine_sweep(C1 * 4, C1, 0.25, 1.4) * R.env_lin_exp(1.4, 0.01, 0.6)), 20), 24),
    ("snarl",      lambda: R.fade_out(R.norm(R.sat(R.svf_lp(square(C2, 0.8, 0.3) * (1 + 0.4 * R.noise(0.8)), 1400, 2.0) * R.env_exp(0.8, 0.22), 2.5))), 36),
    ("wobble",     lambda: R.fade_out(R.norm(lfo_lp(saw(C2, 1.2) + saw(C2 * 0.5, 1.2) * 0.6, 150, 2500, 4.0, 2.5) * R.env_lin_exp(1.2, 0.01, 0.6)), 20), 36),
    ("hiss-bass",  lambda: R.fade_out(R.norm(R.sine(C1, 1.0) * R.env_exp(1.0, 0.4) + R.svf_bp(R.noise(1.0), 3000, 1.5) * R.env_exp(1.0, 0.06) * 1.5), 20), 24),
    ("saw-stab",   lambda: R.fade_out(R.norm(R.sat(R.svf_lp(saw(C2, 0.5) + saw(C2 * 2.004, 0.5) * 0.5, 2500, 1.4) * R.env_exp(0.5, 0.12), 1.4))), 36),
    ("cold-sub",   lambda: R.fade_out(R.norm(R.sine(C1, 1.4) * R.env_lin_exp(1.4, 0.02, 0.7) + R.sine(C1 * 2, 1.4) * R.env_exp(1.4, 0.15) * 0.3), 20), 24),
    ("ring-bass",  lambda: R.fade_out(R.norm(R.sine(C2, 0.9) * R.sine(C2 * 1.5, 0.9) * 1.4 * R.env_exp(0.9, 0.3) + R.sine(C2 * 0.5, 0.9) * R.env_exp(0.9, 0.35) * 0.6), 20), 36),
    ("glass-lead", lambda: R.fade_out(R.norm(fm(C2 * 4, 2.0, 2.5, 0.8, 0.3, 0.3) + fm(C2 * 4 * 1.002, 2.0, 2.5, 0.8, 0.3, 0.3)), 20), 60),
]

def drone(sec=3.4):
    x = R.t(sec); out = np.zeros_like(x)
    for i, (hz, g) in enumerate(((55, 1.0), (55 * 1.5, 0.5), (110 * 1.26, 0.35), (220, 0.2))):
        out += (saw(hz, sec) * 0.5 + saw(hz * 1.004, sec) * 0.5) * g * (1 + 0.1 * np.sin(2 * np.pi * (0.13 + 0.07 * i) * x))
    out = lfo_lp(out, 300, 1800, 0.09, 1.2)
    return R.fade_out(R.norm(out * np.minimum(1, x / 0.4) * np.minimum(1, (sec - x) / 0.6)), 40)
def transmission(sec=2.2):
    x = R.t(sec)
    tone = np.sin(2 * np.pi * (900 + 400 * np.sign(np.sin(2 * np.pi * 7.3 * x)) + 200 * np.sin(2 * np.pi * 0.7 * x)) * x)
    gate = (np.sin(2 * np.pi * 11 * x) > 0.2).astype(float) * (np.sin(2 * np.pi * 2.3 * x) > -0.4)
    static = R.svf_bp(R.noise(sec), 2500, 0.7) * 0.6
    return R.fade_out(R.norm(tone * gate * 0.8 + static * (0.3 + 0.3 * gate)), 20)
def sweep(sec=1.8):
    x = R.t(sec)
    ph = np.cumsum(60 * np.power(60.0, x / sec)) / SR
    return R.fade_out(R.norm(fold(np.sin(2 * np.pi * ph), 1.0 + 1.5 * x / sec) * np.minimum(1, x / 0.05) * np.minimum(1, (sec - x) / 0.3)), 20)
def choir(sec=2.6):
    x = R.t(sec); out = np.zeros_like(x)
    for hz in (130.8, 164.8, 196.0, 261.6):
        src = saw(hz, sec) * 0.5 + saw(hz * 1.003, sec) * 0.5
        out += formant(src, ((600, 7, 1.0), (1040, 9, 0.6), (2250, 10, 0.25)))
    return R.fade_out(R.norm(out * np.minimum(1, x / 0.5) * np.minimum(1, (sec - x) / 0.7)), 40)

def main():
    total = 0
    for i, (name, fn, root) in enumerate(REPTILE):
        x = fn(); p = VP / "REPTILE" / f"{i:02d}_{name}_r{root}.wav"; R.write_wav(p, x); total += len(x) // 2
        print(f"REPTILE {i:2d} {name:12s} {len(x) / SR:5.2f} s  root {root}")
    for i, (name, fn) in enumerate((("drone", drone), ("transmission", transmission), ("sweep", sweep), ("choir", choir))):
        x = fn(); p = VU / f"{i + 1:02d}_{name}.wav"; R.write_wav(p, x)
        print(f"vuser   {i:2d} {name:12s} {len(x) / SR:5.2f} s")
    print(f"REPTILE bank: about {total // 1024} KB at 11.025 kHz ADPCM")

if __name__ == "__main__":
    main()
