# SFA Suite — Sin Fetish Audio firmware for the M-VAVE FM-1

Three engines in one firmware, playing together. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) (MPL-2.0).

| Engine | Focus button | Keys | K1 K2 K3 K4 |
|---|---|---|---|
| OTTER synth (2-op FM, 1 voice) | FX | chromatic, OCT-/OCT+ | TONE DECAY RATIO FM |
| CAT kit (8 pads: kick snare chat ohat clap tom + 2 sample pads) | SEL | first 8 white keys | TUNE DECAY SNAP DRIVE |
| FOX cut & sample | ENV | chromatic (C4 = as recorded) or, with CHOPS > 1, slices on the white keys | START END CHOPS PITCH |

SELECT knob also cycles focus. SAVE keeps settings. USB MIDI in: ch 1 Otter, ch 2 Cat (notes 36–43), ch 3 Fox — all three at once from a DAW.
Fox ships with a built-in "mrrp" growl; a sample bank at flash `PLAT_XDATA1` with header `SFA1` + 24-bit length + 16-bit mono 22.05 kHz replaces it (loader tool: todo).

## Build
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
