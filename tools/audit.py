#!/usr/bin/env python3
"""Sound audit in the simulator: every Otter preset and every Fox sound played the same way, one second each,
then peak / RMS / crest / high-frequency share per sound, so the presets can be balanced and harshness found.
  python3 tools/audit.py gen        writes app/tests/audit_otter.txt and audit_fox.txt
  python3 tools/audit.py report     reads app/build/host/out/audit_*.wav and prints the table
Run from the repo root after `python3 base/tools/build.py app --host`:
  ./app/build/host/ecb_host app/tests/audit_otter.txt app/build/host/out && ./app/build/host/ecb_host app/tests/audit_fox.txt app/build/host/out
"""
import sys, wave, struct, math
import numpy as np

APP = "app"
if "--app" in sys.argv: APP = sys.argv[sys.argv.index("--app") + 1]; del sys.argv[sys.argv.index("--app"):sys.argv.index("--app") + 2]
N_OTTER, N_FOX, N_GREYS = (76, 40, 0) if APP == "app" else (48, 40, 24)
SEG = 1.0                                   # seconds per sound (500 ms held, 500 ms release)

def gen():
    def body(n, keys):
        out = ["wait 600", "master 4096"]
        return out

    o = ["wait 600", "master 4096", "tap HOME", "wait 50", "tap FX", "wait 100", "wav audit_otter.wav"]
    for i in range(N_OTTER):
        o += ["key 7 down", "key 11 down", "key 14 down", "wait 500", "key 7 up", "key 11 up", "key 14 up", "wait 500", "spin ALGO 1", "wait 20"]
    o += ["wavstop"]
    open(f"{APP}/tests/audit_otter.txt", "w").write("\n".join(o) + "\n")
    f = ["wait 600", "master 4096", "tap HOME", "wait 50", "tap ENV", "wait 100", "wav audit_fox.wav"]
    for i in range(N_FOX):
        f += ["key 7 down", "wait 300", "key 7 up", "wait 200", "key 12 down", "wait 300", "key 12 up", "wait 200", "spin ALGO 1", "wait 20"]
    f += ["wavstop"]
    open(f"{APP}/tests/audit_fox.txt", "w").write("\n".join(f) + "\n")
    if N_GREYS:
        g = ["wait 600", "master 4096", "tap HOME", "wait 50", "tap LFO", "wait 100", "wav audit_greys.wav"]
        for i in range(N_GREYS):
            g += ["key 7 down", "key 11 down", "key 14 down", "wait 500", "key 7 up", "key 11 up", "key 14 up", "wait 500", "spin ALGO 1", "wait 20"]
        g += ["wavstop"]
        open(f"{APP}/tests/audit_greys.txt", "w").write("\n".join(g) + "\n")

def load(path):
    w = wave.open(path)
    d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float) / 32768.0
    if w.getnchannels() == 2: d = d.reshape(-1, 2).mean(axis=1)
    return d, w.getframerate()

def metrics(x, sr):
    pk = 20 * math.log10(abs(x).max() + 1e-9)
    rms = 20 * math.log10(math.sqrt((x ** 2).mean()) + 1e-9)
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    fr = np.fft.rfftfreq(len(x), 1 / sr)
    hf = spec[fr > 5000].sum() / (spec.sum() + 1e-12)
    cen = (spec * fr).sum() / (spec.sum() + 1e-12)
    return pk, rms, pk - rms, hf, cen

def report():
    for name, n, per in (("otter", N_OTTER, 1.02), ("fox", N_FOX, 1.02), ("greys", N_GREYS, 1.02)):
        if not n: continue
        try: d, sr = load(f"{APP}/build/host/out/audit_{name}.wav")
        except FileNotFoundError: continue
        print(f"== {name}: {len(d)/sr:.1f} s")
        print(f"{'#':>3} {'peak':>6} {'rms':>6} {'crest':>5} {'hf%':>5} {'cen':>6}")
        rows = []
        for i in range(n):
            a, b = int(i * per * sr), int((i * per + 1.0) * sr)
            if b > len(d): break
            pk, rms, cr, hf, cen = metrics(d[a:b], sr)
            rows.append((i, pk, rms, cr, hf, cen))
            flag = " LOUD" if pk > -1.0 else (" quiet" if rms < -24 else "")
            flag += " HARSH" if hf > 0.35 else ""
            print(f"{i:3} {pk:6.1f} {rms:6.1f} {cr:5.1f} {hf*100:5.1f} {cen:6.0f}{flag}")
        if rows:
            r = np.array([x[2] for x in rows]); print(f"rms mean {r.mean():.1f} dB, spread {r.max()-r.min():.1f} dB")

if __name__ == "__main__":
    (gen if sys.argv[1:2] == ["gen"] else report)()
