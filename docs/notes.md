# FM1 firmware log + quirks — Sin Fetish Audio / Earth Creature Beats

Device: M-VAVE FM-1 (pocket FM synth). SoC: JieLi AC791N / WL82, pi32v2 CPU, two cores, single flash bank, XIP flash at 0x02000000, ROM bootloader (cannot be overwritten by firmware).
Started: 2026-11-07. Two units. Unit A = dev unit (gets flashed). Unit B = stays stock as control/reference until A is proven.

---

## 0. Stock state (fill in before first flash)

Unit A — stock, never flashed.

| Field | Value (fill in) | How to read it |
|---|---|---|
| Stock version | V__ (V15 or V16?) | Boot screen / settings page on the unit, or M-VAVE's M-UPGRADE app. Units shipped since ~20 Oct 2026 may be V16. |
| Package identity | FM-1_____ | thefirmwarehub.com → Connect FM-1 (Chrome/Edge) shows what the connected firmware reports. |
| Flash chip ID | 0x856014 (old) / 0xB36014 (new, autumn-2026 units) | Hub rescue page / fm1-base installer reports it. Matters: old-chip-only firmwares don't save on the new chip. |
| USB name as seen by computer | | System USB/MIDI device list while on stock. |
| Serial / batch sticker | | Underside of unit. |
| Rebranded unit? | yes/no | Some rebranded FM-1s are locked and refuse community firmware. Ours are M-VAVE-branded → expected OK. |
| Stock behaviour notes | | Boot time, default preset, key feel, knob detents, any crackle at full volume, battery level behaviour. |

Backup before anything: export stock projects/voices/samples with M-VAVE's Workbench (open.m-vave.com/fm1-workbench) or M-UPGRADE. Keep the stock `FM-1.fwsc` file(s) locally:
- V15: published in m-vave/fm1_open repo under Apache-2.0 (the hub mirrors it — the universal "way back").
- V16: only from m-vave.com/download via M-UPGRADE; not open-licensed, so hubs don't host it. If Unit A is on V16, download it now and keep a copy.

Unit B — stock, untouched. Record the same table once, then leave it alone.

---

## 1. Base survey (2026-11-07)

Lineage: almost everything descends from **Felucca** (Leo Kuroshita / Hügelton). Felucca's platform was split out as **felucca-bsp** (MPL-2.0) and Charles Vestal's **fm1-base** sits on it. zvenson's **Dinghy** is an independent minimal base; **fm1-dock** is a module host built on Dinghy. M-VAVE's own **FM1 Open** is GPL app + closed binary sound SDK.

| Project | Link | Last activity | Licence | Verdict |
|---|---|---|---|---|
| fm1-base (Charles Vestal) | github.com/charlesvestal/fm1-base | active, Nov 2026 (CREDITS updated ~1 Nov) | MPL-2.0 | **Primary dev base.** Platform layer (screen, panel, LEDs, audio, USB/TRS MIDI, storage, screen reader), host simulator + browser emulator running the same app, build/package/install tools, template app, BLE MIDI/audio, WiFi, USB audio. Used by LOOM-8, Polly, Boardwalk, Berlinan, Bass Something 2, ACID FM1, FIELD… Knows the new flash chip. MPL-2.0 lets us keep our app under whatever licence we want. |
| Felucca (Hügelton) | github.com/hugelton/Felucca | 1.6.x, Oct 2026 | GPL-3.0 | **First flash target.** The reference firmware: multi-engine synth, sequencer, drum kits, sample upload/record via web editor, web installer, return-to-V15, safe mode. Proven on hardware by thousands of installs. As a *code* base it's a full instrument (hard to carve down) — use fm1-base instead for our code. |
| SLOOP (isod89) | github.com/isod89/sloop-fm1 | very active, days ago | GPL-3.0 | Closest existing thing to our suite: 3 synth tracks + drum machine, 12 engines, user samples, song mode. Great reference for how multi-track + samples fit in RAM/CPU. Too big to fork cleanly; steal patterns, not code. |
| Dinghy (zvenson) | github.com/zvenson/dinghy | 0.4, 23 Oct 2026 | GPL-3.0 | Smallest base: 4-voice sine + everything under it done. Good docs (API.md, RULES.md, MAKE-IT-YOURS.md), host mode, CI. 0.4 itself untested on hardware. Runner-up base; fm1-base has more (emulator, screen reader, samples/storage patterns from X0X/LOOM). |
| fm1-dock (Joakim Eriksson) | github.com/joakimeriksson/fm1-dock | 0.7, 31 Oct 2026 | GPL-3.0 | Module host: one firmware, synths as modules in flash slots, deployed over MIDI. Conceptually our "suite", BUT modules run one at a time, 272 KiB RAM each, big modules crowd out the rest. Our goal is Cat+Fox+Otter *together* → one multi-track app, not dock modules. Keep as a later distribution option. |
| FM1 Open (M-VAVE official) | github.com/m-vave/fm1_open | V14, 19 Oct 2026 | App GPL-3.0 + SDK linking exception; sound engine binary | Official, 8 tracks, DX7/303/808/PSG/pluck/sampler, Workbench. Binary SDK = can't read/modify the engine, and our own hardware plan needs code we fully own. Not our base. Use its Workbench for backups + force-flash recovery. |
| zp12 (zvenson) | via thefirmwarehub.com | active | GPL-3.0 | Standalone 12-bit sampler/drum machine. Reference for Fox Cut & Sample (slicing, sample storage layout). |
| X0X / LOOM-8 (Charles Vestal) | github.com/charlesvestal/fm1-x0x, /fm1-loom | Oct 2026 | GPL-3.0 | X0X: drums + 303s + breakbeat player (sample playback). LOOM-8: 5 tracks, 14 engines incl. slicer/granular/tape scrub, user samples via web page. Both on the fm1-base lineage → best code reference for sample playback + kits on our base. |
| Groove OS, Baud Girl FM-1+VA | groove-os.com, baudgirl.com | active | closed | Not open source. Ignore as base; useful UX reference. |
| fm1-emu (Joakim Eriksson) | github.com/joakimeriksson/fm1-emu | daily | — | Emulator that runs .fwsc unchanged in browser/native. Test every build here before flashing. |
| AL-255/FM-1-RE | github.com/AL-255/FM-1-RE | Oct 2026 | — | Reverse-engineering notes (chip, memory map, Dexed-derived FM engine). Read when planning our own hardware. |
| Directories | github.com/cicloid/awesome-fm-1, github.com/aroum/fm1-custom-fw, fm1-editor.com/firmware, thefirmwarehub.com | daily | — | Catalogues. Re-check monthly; ~150 firmwares and counting. |

### Decision
- **Flash first:** Felucca 1.6.x (via thefirmwarehub.com). Proves the update path, the flash chip, saves, USB audio/MIDI on Unit A, and its web editor gives us sample upload today.
- **Build on:** fm1-base + felucca-bsp. Start from its `template/`, run in host sim and fm1-emu, flash only tested builds.
- **Not:** FM1 Open SDK (binary engine), fm1-dock (one module at a time), forking a full instrument (SLOOP/Felucca).

---

## 2. Safe flashing order (Unit A only)

0. Fill in section 0. Back up stock data. Save stock .fwsc locally (V15 from fm1_open repo; V16 from m-vave.com if present).
1. Read the Firmware Hub KB + rescue page once: thefirmwarehub.com/kb/ and thefirmwarehub.com/tools/rescue/. Know the two rescues before you need them:
   - **Soft rescue:** hold OCT− while powering on → USB rescue mode; the hub can install again.
   - **Hard rescue:** ROM bootloader. Computer sees a USB disk "UBOOT1.00" / "WL80UBOOT" (USB ID 4C4A:8057). Tools: fm1_rescue.py (Linux), M-Flasher (Mac), MvaveFM1Unbricker by Quixotic7 (Windows, backs up whole flash first). Only the firmware area is written, never the bootloader.
2. Chrome or Edge (Web MIDI). thefirmwarehub.com → Connect FM-1 → note identity → Install **Felucca** (latest). ~1 minute. Don't unplug. Update goes over USB MIDI: running firmware stages the new package's update loader, loader writes the firmware.
3. Power-cycle. Check: boots, keys/knobs/LEDs, audio out, USB audio + MIDI seen by computer, FELUCCA CRASH screen does NOT appear.
4. **Save test:** change a setting / save a preset, power off, power on. If it's gone → new flash chip + firmware that doesn't know it. (Felucca current should be fine; if not, use the hub's "Saves on newer FM-1s" filter.)
5. Open Felucca's web editor; upload a sample; play it. This is the sampling path we'll reuse.
6. Optional but recommended once: hub → "Back to official V15" → confirm it boots → reinstall Felucca. Now the round trip is proven on this unit.
7. Only after 1–6: first flash of our own fm1-base template build (identity of our own, see section 4).
8. Unit B stays stock until our firmware has survived a week on Unit A.

Rules: one unit at a time; never flash a build that hasn't run in fm1-emu/host sim; always know the current identity before installing; never install repeatedly into a failing update (go to rescue instead).

---

## 3. Firmware log

| Date | Unit | From → To | Identity | Result / notes |
|---|---|---|---|---|
| 2026-11-07 | A | stock → Felucca (hub) | FM-1_918 | Felucca 0 reported by installer. OK. |
| 2026-11-07 22:33 | A | Felucca → SFA v1 | FM-1_5700 | first own build. 3 engines. OK |
| 2026-11-07 | A | SFA v2..v6 | FM-1_5700 | groovebox, sound overhauls, Beaver. OK. v5 fixed a NaN-silence bug (Chamberlin SVF) |
| 2026-11-08 | A | ECB Build 1 (v1.0 step 1) | FM-1_5700 | Earth Creature Beats: sample Cat, Fox 808, Hopper, Beaver 4 lanes, mixer |
| 2026-11-08 | A | ECB Build 2 | FM-1_5700 | workflow: hold=mute, REC arms, step edit (hold+knobs), copy/paste/chain, mute/solo, new palette |
| 2026-11-08 | A | ECB Build 10 | FM-1_5700 | from Alien Stack: reverb bus + per-creature sends (FX page VERB SIZE DELAY DTIME, crusher gone; SENDS page; magic ECB6), Beaver parameter locks / lane lengths / transpose / Euclid / step pages / hold-BEAT fills; Otter ensemble + fm_clip stage, Fox wavefolder + 2nd saw + keytrack, Cat velocity-to-filter and punchier kick defaults; USB audio at full level; Otter re-trimmed (-9.3..-4.8), 0 harsh; 131 sim checks |
| 2026-11-08 | A | ECB Build 9 | FM-1_5700 | headroom pass: fm_clip (linear to 0.7, knee to 1.0) replaces the tanh output stage that squashed everything above -6 dB; engines at unity (not 1.27x), defaults 92 / Cat 100; glue gentler (thr 0.5, slope 0.45, makeup 0.25, 10 ms / 250 ms), limiter 0.95 with 190 ms release, output -1.4 dB; Cat bus comp 25 / makeup 0.35. Full beat: crest 7.0 -> 10.7 dB, peak -1.5 dBFS |
| 2026-11-08 | A | ECB Build 8 | FM-1_5700 | the big one: 8 patterns (compact lanes, patterns E-H + 8 kits' pads in a 2nd store object OBJ_USER at 0xDE000), 16-slot song, 8 scenes on mixer keys 5-8/13-16, 5 sample kits (TECHNO + HOUSE new; kick/snare DSP: harmonic layer, beater, crack; LF sounds at 11.025 kHz) + 3 built kits (TRAP DNB HIPHOP via tools/vkits.py) = 96 samples 366 KB, Hopper sources on ALGO (BUILT-IN / OWN / user bank SFA2 up to 16 named samples via tools/sfa_sample.py, demo bank break/chord/vox), hold-to-mute removed; settings magic ECB5; 118 sim checks |
| 2026-11-08 | A | ECB Build 7 | FM-1_5700 | TRAP kit (built from the pool: 808 kicks/toms, snap rim), Hopper sample -> transient chop -> slice page, Beaver ratchets x1-x4 (len byte bits 5-6) + 8-slot song chain, 64 Otter presets / 24 Fox basses (trim table re-measured: peaks -7.3..-2.7), settings magic ECB4 (resets saves); 104 sim checks |
| 2026-11-08 | A | ECB Build 6 | FM-1_5700 | refinement: tools/audit.py sound audit (no harshness found: HF share <8% everywhere); Otter per-preset trim table (peak spread 24 -> 4.6 dB), Fox synth basses +3 dB, SNAP = highpassed tick, 808 CLICK = lowpassed knock, 808 default decay 105, master defaults eased (30/20/14), ARMED/REC/MUTED/STEP badge on engine pages |
| 2026-11-08 | A | ECB Build 5 | FM-1_5700 | Cat PAD+ page (SAMPLE any of 64 per pad, PUNCH, SNAP, CRUSH; pads blob 512 B, settings magic ECB3 resets saves), Fox 808 CLICK+PUNCH, MASTER mixer page (GLUE, LOWEND, AIR, DRIVE), 48 Otter presets, 16 Fox basses, hold creature + K1/K2 = level/pan anywhere (mute now at 600 ms); 86 sim checks; beat peaks -1.5 dBFS, 0 clipped |
| 2026-11-08 | A | ECB Build 4 | FM-1_5700 | vector cartoon creatures (AA, cached), brighter palette + gradient panels, step entry (REC stopped), GEN/BEAT groove generators, second core renders Otter+Fox (cpu2_fallback), fm_soft replaces per-sample tanh, Cat comp gain every 8 samples, CPU/xrun readout in mixer; 80 sim checks pass, generated beat peaks -2.4 dBFS no clipping |
| 2026-11-08 | A | ECB Build 3 | FM-1_5700 | HOME screen (5 tiles, keys 1-5 jump, K1-K4 levels), page titles, outlined icons, Hopper slice grid, header pattern strip; 69 sim checks pass |

---

## 4. Suite structure: Cat Kit + Fox Cut & Sample + Otter Synth in one firmware

One fm1-base app, three *tracks*, each track bound to one engine. All three run at once, share one sequencer, one FX bus, one storage layout. (Same shape as SLOOP: 3 synths + drum machine, which proves it fits.)

```
sfa-fm1/                      # our repo, fm1-base template layout
  base/   (submodule: fm1-base, which brings felucca-bsp)
  app/
    app.c / app.h             # fm1-base entry points: init, tick, audio render, UI
    seq/                      # one shared step sequencer, 3 lanes, swing, p-locks later
    engine/
      engine.h                # tiny interface: init, note_on/off, param_set, render(buf, n)
      cat_kit.c               # CAT KIT: 8–12 drum voices (synth + sample hybrid), 16 pads on white keys
      fox_sample.c            # FOX CUT & SAMPLE: sample slots, chop/slice (equal, transient, manual), pitch, reverse, mono/poly
      otter_synth.c           # OTTER SYNTH: 1 engine to start (2-op FM or sub+filter), 4–6 voices, 2 envelopes, 1 LFO
    fx/                       # master: drive/bitcrush, delay, small reverb (steal budget numbers from X0X/LOOM)
    store/                    # projects, kits, sample bank; use fm1-base storage API (slots + CRC)
    ui/                       # one screen per track + a mixer page; Dinghy/X0X style: PRESET/ALGO knobs = page nav
    assets/                   # Cat Kit factory samples (ours, CC0 or own recordings), icons, palette
  web/                        # installer page + sample upload page (reuse fm1-base's)
  fm1.toml                    # name = "SFA Suite", identity FM-1_SFxxxxxx (pick our own prefix; hub rejects collisions)
```

Engine contract (keep it dumb so each animal can be developed alone):
- `engine_init(ctx)`, `engine_note(ctx, note, vel, on)`, `engine_param(ctx, id, val)`, `engine_render(ctx, int16 *out, int frames)`.
- Each engine gets a fixed RAM budget (RAM is tight: ~272 KiB usable app RAM on fm1-dock's numbers; check fm1-base's map). Samples live in flash, streamed/XIP, not copied to RAM.
- Audio budget: aim ≤ 35 % of one core per engine so all three + FX fit (LUOGU measured ~35 % in its busiest block; LOOM runs 5 tracks).

Milestones:
1. Template builds, boots on Unit A, our identity, our splash. (Day 1–2)
2. Otter: one voice, one page, playable. (small synth engine is the easiest to verify)
3. Cat Kit: 8 pads, 4 synth drums + 4 sample drums, one page.
4. Fox: load sample via web page, play chromatically, then slicing.
5. Shared sequencer + mixer page. Then FX. Then save/load.
Ship each milestone as a hub-style .fwsc with GitHub Actions (zvenson/fm1-build workflow gives signed builds for free).

---

## 5. Quirks (grows over time)

- **Two stock branches:** stock V16 (closed, m-vave.com, 20–21 Oct 2026) vs FM1 Open V14 (open app, separate line). V14 < V16 does NOT mean older. V15 is the "way back" every community installer knows.
- **New flash chip 0xB36014** on units sold since ~autumn 2026. Old-chip firmwares keep saves in RAM (gone at power-off) and updates from them can fail. Check chip ID; prefer bases that know it (fm1-base, Dinghy ≥0.4, Felucca current).
- **Single flash bank, no rollback.** Interrupted write = unit won't boot normally, but ROM bootloader survives. Rescue tools exist for Win/Mac/Linux. Not a brick.
- **Identities collide.** Every firmware reports an FM-1_xxx identity; the hub refuses collisions. Pick our own prefix early and keep it.
- **Project/sample formats are not interchangeable** between firmwares. Flashing usually erases saved projects/settings. Export first.
- **Brightness freeze** bit X0X early on (low BRIGHTNESS froze the unit at boot). Don't expose a brightness setting until tested.
- **Divide-by-zero trap** crashed X0X on hardware but not in sim (limiter). The chip traps on div/0 — clamp every divisor; sim passes don't prove hardware passes.
- **Locked rebranded units** exist; don't buy rebrands for the fleet.
- **Chrome/Edge only** for installers (Web MIDI). Safari/Firefox won't see it.
- **Second core:** stock runs both cores; most community firmware uses one. Dual-core (GammaWave, FM-106) is possible but adds a "safe hold" dance (Melodee's power-up code). Later.
- **Hardware notes for our own unit (longer term):** FM-1 = AC791N + small screen + 25 keys/16 white-key steps + encoders + USB-C + TRS MIDI in + battery. A cheaper, more focused clone is plausible: same chip family keeps all this firmware portable (SMK-37 Pro proves the port). Watch VIBE FM Mk1 (thefirmwarehub.com/vibe) as the first community-hardware competitor.

---

## 6. Today's on-brand sound: "Mrrp Bass" (Felucca, no code)

After flashing Felucca on Unit A:
1. Record a short "mrrp" / tongue-click into your phone or laptop mic (~200 ms). Normalise, trim tight.
2. Felucca web editor → Samples → upload, trim.
3. Put it on a track, play it chromatically on the keys, 2 octaves down → it becomes a growly bass. Add Felucca's delay, short.
4. On a second track, a 4-op FM pluck with fast decay for the "claw" hat.
That's Cat Kit voice #1 and Fox's first sample in the same session. Note what sounded good; the Fox engine should make exactly that workflow one knob.

Distortion report (Build 2/3 on device, sequencer playing): the simulator shows no clipping, so the likely cause is
CPU overrun (xruns) with four engines at once. Build 4: cheaper saturation, the synths on the second core, and a CPU %
/ XRUN readout on the mixer page so the next report can say which it is.

2026-11-08 — why no ECB build ever reached the device: the rename SFA → ECB moved the package to app/build/ECB.fwsc,
but tools/go.sh kept flashing app/build/sfa.fwsc, the stale pre-rename package. Every run rebuilt ECB and reflashed old
SFA. go.sh now flashes the newest .fwsc the build wrote, deletes the old one and prints which package it is sending.

Sound audit (tools/audit.py): every Otter preset played as a 3-note chord for 1 s, every Fox sound as two notes;
peak / RMS / crest / share of energy above 5 kHz. Rerun after any engine change: `python3 tools/audit.py gen`,
the two scripts in the simulator, `python3 tools/audit.py report`. Build 6 numbers: Otter peaks -7.2..-2.6 dBFS,
Fox RMS spread 7.4 dB, HF share under 8% on all 80 sounds.

Build 8 image size: not measured here (no toolchain in the cloud container); Build 7 was 511748 B of 581564 with
349 KB of samples, Build 8 has 366 KB plus ~6 KB of code, so about 535 KB expected. The build's own check refuses an
image over the slot.

2026-11-08 — web look and distribution: tools/ecb_site.py (the creatures as SVG from icons.c's shapes, the theme,
landing + quick start, a skin for fm1-base's mirror / backup / installer pages), tools/make_pages.py uses it for the
local pages, tools/release.py builds dist/ecb-<version>/ + a zip from a finished build; go.sh runs it after the build.
The resource bank fix: 64 bytes of room for the 32-byte commit record.

## VISITORS

| date | unit | build | identity | notes |
|---|---|---|---|---|
| 2026-11-08 | Alien unit | ALIEN Build 4 | FM-1_5800 | row layout (rowbar/rowpan, context strip), FX sends per being (settings send[4], magic ALI2), hold-SIGNAL fill (seq_fill, restored on release), transpose page (settings transpose[4]), Greys now a melodic lane (rest 0, release, generator), Nordics ensemble (ENS 2048 int16 in pool) + fm_clip stage, Reptiles wavefolder + 2nd saw + keytrack, Mantis choke/velocity/2nd harmonic, Greys velocity; re-audited (Nordics -9.2..-3.4, Reptiles -9.3..-1.7, Greys -4.5..-3.1); 144 sim checks |
| 2026-11-08 | Alien unit | ALIEN Build 3 | FM-1_5800 | GREYS wavetable/FM synth replaces the slicer (greys.c also answers the hop_* API as stubs), Ship: OCT step pages + GEN-held Euclid, Mantis LAYER (PP 9, ABDUCT pack 16 hits, samples 142 KB), 48 Nordics + 24 Reptile presets, audit --app visitors (peaks: Nordics -9.1..-2.8, Reptiles -8.5..-1.7, Greys -4.8..-3.2); 134 sim checks |
| 2026-11-08 | Alien unit | ALIEN Build 2 | FM-1_5800 | renamed Alien Stack (name ALIEN, USB Alien Stack, magic ALI1/ALIU); parameter locks in the melodic lanes' spare byte (0x80|knob<<5|val>>2, applied in lane_hit), polymeter (pattern llen[4], seq.count, per-lane steps), half-rate Schroeder reverb in the pool (4 combs + 2 allpass, int16, ~11 KB) on the FX page (VERB SIZE DELAY DTIME; crusher removed), stack bars, brand-aware site (port 8578); 126 sim checks |
| 2026-11-08 | (the new unit) | VIS Build 1 | FM-1_5800 | scaffold from ECB: cosmic palette, five alien icons (Nordic, Mantis, Reptilian, Grey, Ship), starfield HOME; MANTIS synthesized drum engine (16 voices, 8 kits); REPTILE bank 16 one-shots (82 KB); Greys default bank (drone, transmission, sweep, choir); tools: go.sh [app|visitors], release.py --app, render_visitors.py; 118 sim checks pass |

## SFA core / SFA PROTO

| date | unit | build | identity | notes |
|---|---|---|---|---|
| 2026-11-08 | (any) | PROTO Build 1 | FM-1_5900 | core/sfa_one.c: our own engine (exciter -> 3 waveguide combs -> folder -> ladder, 2 env, 2 LFO, 4 macros, 16 presets), portable C99 with host-provided sin/exp2; host renderer + bench (4 voices ~1.8% host = ~2x ECB's render); proto/ = Alien Stack body with SFA ONE in slot 4 via one_adapter.c; the full AS test script passes |
