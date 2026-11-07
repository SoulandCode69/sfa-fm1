# SFA Suite — Sin Fetish Audio firmware for the M-VAVE FM-1

Three engines in one firmware, playing together. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) (MPL-2.0).

| Button | Does |
|---|---|
| FX / SEL / ENV | focus OTTER / CAT / FOX; press again for the next page (PRESET knob too) |
| SEQ | sequencer page: TEMPO SWING STEPS GATE; the three lanes shown |
| LFO | mix page: OTTER CAT FOX DELAY |
| ARP | SAMPLE: Fox records the groovebox's own output (up to 2 s) until ARP again |
| PLAY / REC | run the 16-step pattern / record what you play onto it (hold REC 1 s: clear the focused lane) |
| HOME | stop, back to the engine page |
| SAVE | keep settings, pattern and the sampled bank |

| Engine | Keys | Pages |
|---|---|---|
| OTTER 4-voice polysynth: PolyBLEP saw/pulse/square or 2-op FM, unison detune, sub, driven SVF, ADSR, LFO | chromatic, OCT-/OCT+ | TONE RES WAVE DETUNE · ATTACK DECAY SUSTAIN RELEASE · LFO VIBRATO WOBBLE GLIDE |
| CAT kit: 4 kits x 16 parametric drums (808, 909, LOFI, FX) | 16 white keys = pads | TUNE DECAY SNAP DRIVE · KIT HATS TONE LEVEL |
| FOX cut & sample: a 4 s bank, chromatic or chopped, 4 voices | chromatic (C4 = as recorded) or slices on the white keys | START END CHOPS PITCH · REV LOOP FILTER DECAY |

USB MIDI in: ch 1 Otter, ch 2 Cat (notes 36-51), ch 3 Fox; recorded while REC is lit. Master: delay, DC block, limiter.
Fox's bank, in order: the sample you SAMPLEd and SAVEd (flash), else `tools/sfa_sample.py x.wav` (the resource region),
else four built-in sounds (growl, vowel, noise hit, pluck), a second each.

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
