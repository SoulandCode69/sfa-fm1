#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Earth Creature Beats: the demo user bank for Hopper (assets/user/*.wav -> app/samples.bin via tools/sfa_sample.py):
# a one-bar drum break built from the factory pack, a chord stab and a vowel sweep. Original, CC0. Replace with yours.
import wave, struct, math
import numpy as np
from pathlib import Path
SR = 22050
ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "assets" / "pack"
OUT = ROOT / "assets" / "user"

def rd(p):
    w = wave.open(str(p)); d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float) / 32768
    return d
def wr(name, x):
    x = np.clip(x / (np.abs(x).max() or 1) * 0.89, -1, 1)
    OUT.mkdir(parents=True, exist_ok=True)
    w = wave.open(str(OUT / name), "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(struct.pack("<%dh" % len(x), *(x * 32767).astype(np.int16))); w.close()
def place(buf, s, at, g=1.0):
    i = int(at * SR); n = min(len(s), len(buf) - i); buf[i:i + n] += s[:n] * g

def main():
    kit = PACK / "HOUSE"
    k, sn, ch, oh, cl = (rd(sorted(kit.glob(f"*_{n}.wav"))[0]) for n in ("kick", "snare", "chat", "ohat", "clap"))
    bpm, bar = 110, 60 / 110 * 4
    b = np.zeros(int(bar * SR) + 2000)
    for beat in (0, 1, 2, 3): place(b, k, beat * bar / 4, 1.0)
    place(b, k, 2.5 * bar / 4, 0.8)
    for beat in (1, 3): place(b, sn, beat * bar / 4, 0.9); place(b, cl, beat * bar / 4, 0.4)
    for e in range(8): place(b, ch if e != 7 else oh, e * bar / 8, 0.5 if e % 2 == 0 else 0.35)
    wr("01_break.wav", b[: int(bar * SR)])
    t = np.arange(int(1.4 * SR)) / SR
    chord = sum(np.sign(np.sin(2 * math.pi * f * t) + 1e-9) * 0.5 + np.sin(2 * math.pi * f * 1.003 * t) for f in (130.8, 164.8, 196.0, 246.9))
    env = np.exp(-t / 0.35); lp = np.zeros_like(chord); s = 0.0; a = 1 - math.exp(-2 * math.pi * 1800 / SR)
    for i in range(len(chord)): s += (chord[i] - s) * a; lp[i] = s
    wr("02_chord.wav", lp * env)
    t = np.arange(int(0.9 * SR)) / SR
    src = np.sign(np.sin(2 * math.pi * 110 * t) + 1e-9) * (1 - 0.3 * np.sin(2 * math.pi * 5 * t))
    out = np.zeros_like(src)
    for f0, f1, g in ((300, 800, 1.0), (2200, 1200, 0.6), (3000, 2600, 0.3)):   # formants gliding "oo" -> "ah"
        ic1 = ic2 = 0.0
        for i in range(len(src)):
            hz = f0 + (f1 - f0) * i / len(src); gg = math.tan(math.pi * hz / SR); kq = 1 / 6.0
            a1 = 1 / (1 + gg * (gg + kq)); a2 = gg * a1
            v3 = src[i] - ic2; v1 = a1 * ic1 + a2 * v3; v2 = ic2 + a2 * ic1 + gg * a2 * v3
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; out[i] += v1 * g
    wr("03_vox.wav", out * np.minimum(1, t / 0.05) * np.exp(-np.maximum(0, t - 0.5) / 0.15))
    print("assets/user: break, chord, vox")

if __name__ == "__main__":
    main()
