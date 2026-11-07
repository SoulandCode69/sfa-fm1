# SFA Suite — Sin Fetish Audio firmware for the M-VAVE FM-1

Three engines in one firmware, playing together. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) (MPL-2.0).

| Button | Does |
|---|---|
| FX / SEL / ENV | focus OTTER / CAT / FOX; press again for page 2 (PRESET knob too) |
| SEQ | sequencer page: TEMPO SWING STEPS GATE; the three lanes shown |
| LFO | mix page: OTTER CAT FOX DELAY |
| PLAY / REC | run the 16-step pattern / record what you play onto it (hold REC 1 s: clear the focused lane) |
| HOME | stop, back to the engine page |
| SAVE | keep settings + pattern |

| Engine | Keys | Page 1 | Page 2 |
|---|---|---|---|
| OTTER 4-voice polysynth (PolyBLEP saw/pulse/square or 2-op FM, unison detune, sub, driven SVF, ADSR) | chromatic, OCT-/OCT+ | TONE RES WAVE DETUNE | ATTACK DECAY SUSTAIN RELEASE |
| CAT kit (808/909-style kick snare hats clap toms rim + 8 sample slices) | 16 white keys = pads | TUNE DECAY SNAP DRIVE | HATS SNARE TONE SMPLVL |
| FOX cut & sample | chromatic (C4 = as recorded) or slices on the white keys | START END CHOPS PITCH | REV LOOP GAIN |

USB MIDI in: ch 1 Otter, ch 2 Cat (notes 36-51), ch 3 Fox; recorded while REC is lit. Master delay on the mix page.
Fox's bank: `tools/sfa_sample.py x.wav` (any WAV; up to 2 s at 22.05 kHz), written by the installer after the firmware.

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
app/build/host/sfa_host app/tests/basic.txt app/build/host/out   # 11 checks, screenshots + WAVs in out/
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
