#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sin Fetish Audio
"""Make Fox's sample bank (app/samples.bin) from a WAV: mono 16-bit 22.05 kHz, at most 1.27 s, header SFA1.
    python3 tools/sfa_sample.py IN.wav [--out app/samples.bin] [--gain 1.0]
Then: python3 base/tools/build.py app  and  python3 base/tools/fm1_install.py app/build/sfa.fwsc
(the installer writes the bank after the firmware). Any WAV (mono/stereo, 8/16/24/32-bit, any rate) goes in;
it is mixed to mono, resampled, trimmed to the limit and normalised to -1 dB."""
import argparse, struct, sys, wave
from pathlib import Path
RATE, MAXN = 22050, 28000

def read_wav(path):
    w = wave.open(str(path))
    ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    out = []
    for i in range(n):
        acc = 0.0
        for c in range(ch):
            b = raw[(i * ch + c) * sw:(i * ch + c + 1) * sw]
            if sw == 1:
                v = (b[0] - 128) / 128.0
            else:
                v = int.from_bytes(b, "little", signed=True) / float(1 << (8 * sw - 1))
            acc += v
        out.append(acc / ch)
    return out, sr

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav", type=Path)
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parents[1] / "app" / "samples.bin")
    ap.add_argument("--gain", type=float, default=1.0)
    a = ap.parse_args()
    s, sr = read_wav(a.wav)
    if not s:
        sys.exit("empty WAV")
    step = sr / RATE
    n = min(MAXN, int(len(s) / step))
    res = []
    for i in range(n):                                   # linear resample
        p = i * step
        j = int(p); f = p - j
        b = s[j + 1] if j + 1 < len(s) else s[j]
        res.append(s[j] + (b - s[j]) * f)
    peak = max(abs(x) for x in res) or 1.0
    g = a.gain * 0.891 / peak                            # -1 dB
    pcm = struct.pack("<%dh" % n, *[max(-32768, min(32767, int(x * g * 32767))) for x in res])
    a.out.write_bytes(b"SFA1" + struct.pack("<I", n)[:3] + b"\0" + pcm)
    print(f"{a.out}: {n} samples, {n / RATE:.2f} s at {RATE} Hz" + ("" if len(s) / step <= MAXN else " (trimmed)"))

if __name__ == "__main__":
    main()
