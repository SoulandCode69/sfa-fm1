#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sin Fetish Audio
"""Hopper's user sample bank (app/samples.bin) from your WAVs: up to 16 samples, mono 16-bit 22.05 kHz, each at most
4.1 s, named after the files. ALGO on the Hopper page browses BUILT-IN, OWN (what you sampled) and these.
    python3 tools/sfa_sample.py kick.wav vocal.wav break.wav [--out app/samples.bin] [--gain 1.0]
    python3 tools/sfa_sample.py mydir/            (every WAV in the folder, sorted)
Then tools/go.sh (the installer writes the bank after the firmware). The region holds 184 KB = 4.2 s of audio in
all, shared by the samples in proportion to their lengths (one sample: up to 4.1 s, Hopper's RAM bank).
Any WAV (mono/stereo, 8/16/24/32-bit, any rate) goes in; mixed to mono, resampled, trimmed, normalised to -1 dB.
Format "SFA2": count, 3 pad, count x { offset u32, length u32 (samples), name[12] }, then the PCM.
(A single file may also be written in the old "SFA1" format with --single.)"""
import argparse, struct, sys, wave
from pathlib import Path
RATE, MAXN, REGION = 22050, 92000, 46 * 4096

def read_wav(path):
    w = wave.open(str(path))
    ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    out = []
    for i in range(n):
        acc = 0.0
        for c in range(ch):
            b = raw[(i * ch + c) * sw:(i * ch + c + 1) * sw]
            v = (b[0] - 128) / 128.0 if sw == 1 else int.from_bytes(b, "little", signed=True) / float(1 << (8 * sw - 1))
            acc += v
        out.append(acc / ch)
    return out, sr

def convert(path, gain, maxn):
    s, sr = read_wav(path)
    if not s: sys.exit(f"{path}: empty WAV")
    step = sr / RATE
    n = min(maxn, int(len(s) / step))
    res = []
    for i in range(n):                                   # linear resample
        p = i * step; j = int(p); f = p - j
        b = s[j + 1] if j + 1 < len(s) else s[j]
        res.append(s[j] + (b - s[j]) * f)
    peak = max(abs(x) for x in res) or 1.0
    g = gain * 0.891 / peak                              # -1 dB
    return [max(-32768, min(32767, int(x * g * 32767))) for x in res], len(s) / step > maxn

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav", type=Path, nargs="+", help="WAV files, or one folder of them")
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parents[1] / "app" / "samples.bin")
    ap.add_argument("--gain", type=float, default=1.0)
    ap.add_argument("--single", action="store_true", help="the old one-sample SFA1 format")
    a = ap.parse_args()
    files = a.wav
    if len(files) == 1 and files[0].is_dir(): files = sorted(p for p in files[0].iterdir() if p.suffix.lower() == ".wav")
    files = files[:16]
    if not files: sys.exit("no WAV files")
    if a.single or (len(files) == 1 and a.single):
        pcm, trimmed = convert(files[0], a.gain, MAXN)
        a.out.write_bytes(b"SFA1" + struct.pack("<I", len(pcm))[:3] + b"\0" + struct.pack("<%dh" % len(pcm), *pcm))
        print(f"{a.out}: {len(pcm)} samples, {len(pcm) / RATE:.2f} s" + (" (trimmed)" if trimmed else "")); return
    budget = (REGION - 8 - 20 * len(files)) // 2          # samples of PCM for all of them together
    want = []
    for f in files:                                         # each one's natural length (capped), then share the region
        w = wave.open(str(f)); want.append(min(MAXN, int(w.getnframes() * RATE / w.getframerate())))
    scale = min(1.0, budget / max(1, sum(want)))
    table, pcm_all, off = [], b"", 8 + 20 * len(files)
    for f, n in zip(files, want):
        pcm, trimmed = convert(f, a.gain, max(1, int(n * scale)))
        name = f.stem.upper()[:12].encode("ascii", "replace")
        table.append(struct.pack("<II12s", off, len(pcm), name.ljust(12, b"\0")))
        pcm_all += struct.pack("<%dh" % len(pcm), *pcm); off += len(pcm) * 2
        print(f"  {f.stem.upper()[:12]:12s} {len(pcm) / RATE:5.2f} s" + (" (trimmed)" if trimmed else ""))
    a.out.write_bytes(b"SFA2" + bytes([len(files), 0, 0, 0]) + b"".join(table) + pcm_all)
    print(f"{a.out}: {len(files)} samples, {off / 1024:.0f} KB of {REGION / 1024:.0f}")

if __name__ == "__main__":
    main()
