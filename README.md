# Earth Creature Beats — Sin Fetish Audio firmware for the M-VAVE FM-1

A groovebox of five creatures that play together. Built on [fm1-base](https://github.com/charlesvestal/fm1-base) (MPL-2.0).

| Creature | Button | What |
|---|---|---|
| OTTER Synth | FX | 4-voice polysynth, 64 presets (ALGO), ADSR, filter env, LFO, arpeggiator |
| CAT Kit | SEL | sample drum machine: 8 kits (CLASSIC, HOUSE, LOFI, PUNCH, TECHNO + the built TRAP, DNB, HIPHOP) x 16 pads; every pad its own PITCH DECAY FILTER LEVEL and SAMPLE (any of the 64 sounds: build your own kit) PUNCH SNAP CRUSH (pages PAD / PAD+ follow the pad you hit); KIT page; BUS compressor |
| FOX Bass & 808 | ENV | 16 factory 808 / sub / bass one-shots played chromatically (mono, glide, filter, decay, drive, CLICK, PUNCH) + 24 synth basses |
| HOPPER Cut | LFO | slicer: ALGO picks the source (BUILT-IN sounds, OWN = what you sampled, then your USER bank: tools/sfa_sample.py your WAVs, up to 16), SAMPLE the output (ARP: stop = chopped by transients and on the slice page, ready to play), chop EQUAL / TRANSIENT / MANUAL (hold a slice key + K1 moves its start), per-slice pitch / reverse / decay / level |
| BEAVER | SEQ | 4 tracks x up to 32 steps (OCT-/+ page the keys to 1-16 / 17-32), per step NOTE VEL LENGTH PROB (hold a step + K1-K4), RATCHET x1-x4 (hold a step + GEN), **parameter locks** (hold a step, press the creature's button, turn K1-K4), **polymeter** (PRESET knob: each track's own length) and **live transpose** (next PRESET page), **Euclid** (hold GEN, turn K1 pulses / K2 steps / K3 rotation), **fills** (hold BEAT: busier for as long as you hold, then back), a **song builder** (the 4th Beaver page: 16 slots, each a pattern x 1-8 repeats with an optional scene recalled on entry; keys pick a slot, K1-K4 edit it; tap CHAIN to run it), swing, 8 patterns A-H (ALGO), a 16-slot song chain (hold CHAIN + keys 1-8 to write it, CHAIN + CLR to clear, tap CHAIN to run it), copy / paste, GEN / BEAT groove generators, step entry, live record |

**One way of working everywhere**: a creature's button = its track (again: next page; **hold + K1 / K2: its level /
pan, from any page**; a plain hold does nothing, so diving into a page never mutes anything). K1-K4 are always
the four things on screen; PRESET knob = page; ALGO knob = preset / kit / 808 / pattern; SELECT knob = track.
REC arms the focused track (hold: clear it); PLAY runs; GLO taps tempo; EDIT = mixer (levels + meters, pans, FX: a **reverb bus** (VERB, SIZE) and the delay, MASTER: GLUE compressor, LOWEND, AIR, DRIVE,
**SENDS: each creature's send into the reverb and delay**;
keys 1-4 mute, 9-12 solo; **keys 5-8 and 13-16 are 8 scenes**: hold one to store the pattern + presets + kit + mix,
tap to recall). **HOME = the home screen**: the five creatures as tiles (preset / kit / level / meter /
REC / MUTE), white keys 1-5 jump straight to one, K1-K4 are the four track levels. The device boots there.
Every engine page shows its title (OTTER · ENVELOPE, HOPPER · SLICE) and the header shows patterns A-D with the
live one lit. SAVE keeps everything.

**Making a beat fast**: HOME key 7 (or BEAT on the Beaver's black keys) writes a fresh groove on all four tracks and
plays it; GEN rewrites the focused track only, a new one every press. **REC while stopped = step entry**: play the
keys and each one lands on the next step of the focused track (OCT+ = a rest, OCT- = back); PLAY to hear it. While
playing, REC arms the track for live record. The mixer shows the audio load (CPU %) and flags xruns.

**Sampling into an MPC or a computer**: each FM-1 is a class-compliant USB audio interface as well as a MIDI device, and
both firmwares send that stream at full level whatever MASTER does; on the MPC, Preferences → Audio Device (MPC 2.10+
takes class-compliant interfaces), or the headphone out into the line inputs.

**Two FM-1s**: the installer flashes whichever FM-1 answers, so plug in Unit A alone when running tools/go.sh
(or pass `--port` to base/tools/fm1_install.py). Unit B stays stock. MIDI in ch 1-4 = Otter, Cat, Fox, Hopper.
**Your own samples**: `python3 tools/sfa_sample.py mydir/` (or WAV files) writes app/samples.bin, the user bank the
installer flashes with the firmware; Hopper's ALGO reaches them by name. The factory pack: tools/render_pack.py →
tools/pack_samples.py → tools/vkits.py (the built kits, by sample name).
Sound audit: tools/audit.py (every preset measured in the simulator). Samples: docs/SAMPLES.md (original, CC0). Clean by default: no drive, no delay until you add them.

## Distribution

`tools/go.sh` (or `python3 tools/release.py` after any build) writes `dist/ecb-<version>/`, a static site in ECB's
look: the landing page, **install/** (the web installer: Chrome or Edge over Web MIDI, with the FM-1 on USB),
**firmware/** (the .fwsc, the user sample bank, firmware.json, licences), **mirror/**, **backup/** and the quick
start; and `dist/EarthCreatureBeats-<version>.zip` with the site, README, docs and tools/sfa_sample.py. Serve the
folder from anywhere static (GitHub Pages, or `python3 -m http.server` beside it); Web MIDI needs https or localhost.
The laptop pages at http://localhost:8577/ share the same look.

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
python3 base/tools/build.py app             # device: app/build/ECB.fwsc (identity FM-1_5700)
python3 base/tools/build.py app --web       # browser emulator (needs emcc)
```

## Flash (Unit A only)
1. Felucca via https://thefirmwarehub.com first (proves the unit, the chip, the way back).
2. `python3 base/tools/fm1_install.py app/build/ECB.fwsc` with the FM-1 on USB, or the hub's Install from file.
3. Back: hub → Back to official V15. Stuck: hold OCT− at power-on.
4. Log it in `FM1s/notes.md`.

GPL-3.0-only. Platform MPL-2.0 (fm1-base, felucca-bsp): see base/LICENSING.md.
