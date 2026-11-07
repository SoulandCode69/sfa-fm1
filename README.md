# SFA Suite — Sin Fetish Audio firmware for the M-VAVE FM-1

Three engines in one firmware, playing together. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) (MPL-2.0).

Four animals in the header: OTTER (synth), CAT (drums), FOX (bass + sampler), BEAVER (sequencer).

| Button / knob | Does |
|---|---|
| FX / SEL / ENV | focus OTTER / CAT / FOX; press again for the next page (PRESET knob too) |
| ALGO knob | Otter: 32 presets · Cat: 6 kits · Fox: SAMPLER or 8 bass presets · Beaver: pattern A-D |
| SEQ | the Beaver: TEMPO SWING STEPS GATE, the three lanes; white keys toggle the focused lane's steps; SELECT picks the lane |
| LFO | mix page (PRESET knob flips to FX): OTTER CAT FOX DELAY · DTIME DFEEDB CRUSH DRIVE |
| ARP | SAMPLE: Fox records the groovebox's own output (up to 2 s) until ARP again |
| PLAY / REC | run the pattern / record what you play onto it (hold REC 1 s: clear the focused lane) |
| HOME | stop, back to the engine page |
| SAVE | keep everything: settings, presets, four patterns, the sampled bank |

| Engine | Pages |
|---|---|
| OTTER polysynth, 32 presets: leads (saw, bright, supersaw, trance, square, pulse, chip, sync, screamer), basses (pulse, sub, acid, reese, FM, wobble), pads/strings/brass, plucks, FM bell/piano/marimba, organ, keys, five arps, drone | TONE RES WAVE DETUNE · ATTACK DECAY SUSTAIN RELEASE · FENV FDECAY VEL>F SUB · LFO VIBRATO WOBBLE GLIDE · ARP ARPRATE ARPOCT MONO |
| CAT: 6 kits x 16 drums, nine drum algorithms (808/909-style kick, snare, metal hats, clap, toms, cowbell/bells, clicks, noise, zaps), bus compressor | TUNE DECAY SNAP DRIVE · KIT HATS TONE LEVEL · PENV NDECAY PUNCH COMP |
| FOX BASS (ACID 303, SQR ACID, SUB, REESE, PLUCK, WOBBLE, GROWL, DEEP HOUSE): mono, 303-style filter envelope, accent, legato glide, drive, wobble | CUTOFF RES ENVMOD DECAY · WAVE SUB GLIDE DRIVE · LFO WOBBLE ACCENT RELEASE |
| FOX SAMPLER: a 4 s bank drawn as a waveform; chromatic or chopped, 4 voices | START END CHOPS PITCH · REV LOOP FILTER RES · ATTACK DECAY GATE ROLL |

Dry by default: delay, bitcrusher and drive are off until you turn them up on the FX page. Master: DC block, limiter.
USB MIDI in: ch 1 Otter, ch 2 Cat (notes 36-51), ch 3 Fox; recorded while REC is lit.

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
