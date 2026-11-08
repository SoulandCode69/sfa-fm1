# Earth Creature Beats — Sin Fetish Audio firmware for the M-VAVE FM-1

A groovebox of five creatures that play together. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) (MPL-2.0).

| Creature | Button | What |
|---|---|---|
| OTTER Synth | FX | 4-voice polysynth, 32 presets (ALGO), ADSR, filter env, LFO, arpeggiator |
| CAT Kit | SEL | sample drum machine: 3 factory kits x 16 pads, per-pad PITCH DECAY FILTER LEVEL, bus compressor |
| FOX Bass & 808 | ENV | 16 factory 808 / sub / bass one-shots played chromatically (mono, glide, filter, decay, drive) + 8 synth basses |
| HOPPER Cut | LFO | slicer: SAMPLE the output (ARP), chop EQUAL or at TRANSIENTS, per-slice pitch / reverse / decay / level, chromatic play |
| BEAVER | SEQ | sequencer: 4 lanes, up to 32 steps, swing, patterns A-D (ALGO), live record (REC) and step entry (keys), tap tempo (GLO) |

EDIT = the mixer (levels, pans, FX: delay, bitcrusher). Press a creature's button again for its next page; PRESET knob
turns pages; SELECT knob changes focus. SAVE keeps everything. MIDI in ch 1-4 = Otter, Cat, Fox, Hopper.
Samples: see docs/SAMPLES.md (original, CC0). Clean by default: no drive, no delay until you add them.

## Everyday loop (WSL/Ubuntu)
```
tools/go.sh            # build -> flash Unit A -> write the sample bank -> open the laptop UI in Chrome
tools/go.sh --pull     # same, after pulling the latest code
tools/go.sh --no-flash # just rebuild and reopen the laptop UI
```
Laptop UI = fm1-base's screen mirror at http://localhost:8577/mirror/ : the FM-1's screen live, its buttons, knobs
and keys clickable. Press Connect; nothing else may have the FM-1's MIDI port open (close the hub tab).

Your own sample: `python3 tools/sfa_sample.py /mnt/c/Users/<you>/Downloads/x.wav` then `tools/go.sh`.

## Build (by hand)
```
git clone --recursive <this repo>           # base/ = fm1-base + felucca-bsp
python3 base/tools/build.py app --host      # host simulator, needs only cc
app/build/host/ecb_host app/tests/basic.txt app/build/host/out   # 11 checks, screenshots + WAVs in out/
sh base/tools/get_toolchain.sh              # once: JieLi pi32v2 toolchain -> ~/.jieli
python3 base/tools/build.py app             # device: app/build/sfa-dev+<commit>.fwsc (identity FM-1_5700)
python3 base/tools/build.py app --web       # browser emulator (needs emcc)
```

## Flash (Unit A only)
1. Felucca via https://thefirmwarehub.com first (proves the unit, the chip, the way back).
2. `python3 base/tools/fm1_install.py app/build/sfa-*.fwsc` with the FM-1 on USB, or the hub's Install from file.
3. Back: hub → Back to official V15. Stuck: hold OCT− at power-on.
4. Log it in `FM1s/notes.md`.

GPL-3.0-only. Platform MPL-2.0 (fm1-base, felucca-bsp): see base/LICENSING.md.
