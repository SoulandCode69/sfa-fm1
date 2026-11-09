# SFA engine architecture

One engine, written once, runs in the desktop simulator, on the FM-1 and on our own hardware. Presets, kits, samples and
patterns are data that travel as plain files, so a user can change or share them without rebuilding firmware.

This page is a design, not a description of finished code. Each section says what exists today and what is proposed.

## 1. Three layers

```
 DATA      packs (.sfapack): presets, kits, samples, patterns       plain files, no code
 ENGINES   Otter, Fox, Cat, Hopper, Beaver, Mantis, ... SFA ONE     portable C99, no allocation, no I/O
 APP       keys, pages, mixer, sequencer, song                      one per product (ECB, AS, the prototype)
 PORT      audio callback, display, input, storage, MIDI, USB       one per target (host sim, FM-1, prototype)
```

The rule that keeps this working: **an engine includes nothing from a platform.** It receives notes and parameter values and
adds audio into a buffer. Everything that touches hardware lives in the port.

### Today

* The engine contract is `engine_t` in `app/engine/engine.h` (and the same in `visitors/engine/`):
  a name, `init`, `note(note, vel, on)`, `render(mono, n)` which *adds* n frames, `panic`, up to 5 pages of 4 parameters
  (`params[page][4]`, each 0..127), parameter labels, optional text for a value, and page titles. The app owns the keys,
  the pages and the mix.
* The shared DSP helpers (`svf_*`, `fm_soft`, `fm_clip`, `polyblep`, `decay_coef`) live in the same header and are plain C.
* `core/` (SFA ONE) is plain C99 float with no allocation, and already builds on the host.
* The simulator is the real firmware compiled for the host and driven by scripts; the FM-1 build is the same source through
  fm1-base. That is the "same engine in sim and on the FM-1" requirement, already met.
* Samples already travel as data: the user bank (`samples.bin`, format SFA2) is its own flash region, written after the firmware.

### Gaps that block the prototype

| Gap | Why it matters | Proposed fix |
|---|---|---|
| `SR` is a `#define 44100.0f` used by inline helpers | the prototype will run at 48 kHz | make it a per-target compile flag (`-DSR=48000.0f`) first; a runtime rate only if a product needs two |
| Presets are `const` tables compiled into each engine | changing one needs a rebuild and a flash | presets become data (section 3); the tables stay as the factory set |
| Engine output is mono added into one buffer | the box has stereo out and FX sends | add an optional stereo render; the mono one stays for the FM-1 |
| Engines have a name but no stable id or version | packs must say which engine they are for | add `id` (`"otter"`) and `version` |
| Pattern data lives in each app | a pattern cannot move between products | one pattern model (section 3), apps read and write it |

## 2. The engine module

Proposed `engine_t` v2. Everything in v1 stays; additions are marked.

```c
struct engine {
    const char *id;                 /* NEW  "otter": the name packs use */
    uint16_t    version;            /* NEW  bumps when parameter meaning changes */
    const char *name;               /*      "OTTER" */
    void (*init)(void);
    void (*note)(int32_t note, uint32_t vel, int on);
    void (*render)(float *mono, uint32_t n);
    void (*render_st)(float *l, float *r, uint32_t n);   /* NEW  optional: 0 means use render */
    void (*panic)(void);
    uint8_t npages;                 /*      1..5 pages of NPARAM */
    const char *const *names[NPAGES];
    uint8_t *params[NPAGES];        /*      the whole sound: 0..127 each */
    ...
};
```

Why parameters stay as 0..127 bytes: the whole sound is already `params[page][4]`. A preset is therefore just a copy of those
bytes plus a name, which is small, portable, and already how the apps save state. Do not invent a richer parameter system until
an engine genuinely needs one.

An engine module is a folder: `engine/<id>.c`, its header entry, a test script in `tests/`, a render recipe and an audit entry.
The "engine template" on the roadmap is exactly this folder, so a new engine is a copy, not an invention.

## 3. Data: what travels as files

Full format in [FORMATS.md](FORMATS.md). The short version:

| Thing | What it is | Travels as |
|---|---|---|
| Preset | engine id, engine version, name, `params[npages][4]` | `presets/*.json` |
| Kit | a named set of sample references with per-pad settings (tune, decay, level) for one engine | `kits/*.json` |
| Sample | audio at any rate, any length | `samples/*.wav`; the packer converts for the target |
| Pattern | steps, tracks, notes, velocity, per-step parameter locks, length, swing | `patterns/*.json` |
| Pack | any mix of the above plus a manifest | `.sfapack` (a zip) |

A pack contains no code. That is a deliberate limit: it makes sharing safe and makes a bad pack at worst a bad sound, never a
crash or a security problem. New *engines* still need a build.

## 4. One engine, three targets

| | Desktop / simulator | FM-1 | Prototype |
|---|---|---|---|
| Build | `tools/sfa sim` (host C compiler) | `tools/go.sh`, fm1-base toolchain | Daisy Seed first, our STM32H750 board later |
| Rate | 44.1 kHz today | 44.1 kHz | 48 kHz |
| Audio out | WAV files | USB audio + analogue | stereo codec |
| Sample store | files | one flash region, 16 samples, about 4.2 s total | microSD, minutes to hours |
| Presets and patterns | compiled tables + test scripts | compiled tables; a data region is proposed | files on the card, loaded at runtime |
| Change a preset | edit, rebuild | edit, rebuild, flash | copy a file |
| Check it | scripts with `expect` | the same scripts, then the unit | the same scripts, then the unit |

What stays identical across all three: the engine source, the parameter model, the pack format and the measurement loop
(render, analyse, compare). What changes is only the port.

## 5. Customise and share without rebuilding

* **Prototype**: the card is the library. Packs are folders or `.sfapack` files; the box reads them at boot or on demand. No
  flash, no toolchain. Sharing is copying a file.
* **FM-1**: only samples are data today. A second flash region for user presets and patterns, read at boot and merged over the
  factory tables, would give the same behaviour. Whether fm1-base can write a region without a full package is not checked yet;
  treat that as the first spike.
* **Desktop**: the console imports and exports packs now (Presets page) and the simulator can render any pack, so a pack can be
  auditioned and measured before it goes near a unit.
* **Safety**: a pack is validated against the engine id, version and parameter ranges before use; unknown fields are ignored;
  out-of-range values are clamped; sample size and count are checked against the target profile.

## 6. What this means for the prototype's form factor

These follow from the design and should be treated as requirements until someone argues otherwise.

1. **A card slot the user can reach.** If sharing is copying files, the microSD slot must be on the outside, not under the lid.
2. **Four encoders match four parameters per page.** The planner already has four encoders, and `NPARAM = 4`. Keep both.
3. **A screen that can show names and a list.** Presets and packs have names (12 characters today); browsing them needs a
   screen with room for a list, not only a few icons.
4. **USB-C that does more than power.** Class-compliant MIDI at least; a mass-storage mode (card as a drive) makes pack
   exchange work with no app. Whether the chosen MCU does both with audio is an open question.
5. **RAM for sample voices.** Streaming from the card needs buffers; short samples are better preloaded. The Daisy Seed has
   external RAM; the product board needs it priced into the BOM. This is not in the current parts list.
6. **48 kHz, stereo, 16 voices as a design target** sets the processor class, as in the research notes (480 MHz M7 with FPU).
7. **Sixteen pads** map to sixteen steps and sixteen sample voices, so patterns and kits both have a natural size.

## 7. Order of work

1. Add `id` and `version` to `engine_t` in both firmwares; no behaviour change. Re-run every simulator check.
2. Make `SR` a build flag; build `core/` and one engine at 48 kHz on the host and compare the renders.
3. Write `tools/sfa pack` (create, validate, list) and teach the Sample Lab to emit the pack manifest instead of its own `kit.json`.
4. Simulator: load a pack (presets override the tables, samples fill the bank) and render it.
5. FM-1 spike: can a data region be written alone? If yes, ship presets as data on the FM-1.
6. Port one engine to the Daisy Seed with the same test script and compare its render with the host render.
7. Pattern model and a shared sequencer module.

Step 6 is the proof of the whole idea: the same engine source and the same script producing matching audio on the host and on a board.

## Open questions

* Do we want one `render_st` or a fixed stereo bus per engine?
* How are factory presets and user presets numbered so a pack does not clobber a slot unexpectedly?
* Is a pattern shared across engines (a drum pattern played by another kit), or tied to one engine?
* Pack signing: not needed for data-only packs, but the question returns if scripts are ever allowed. They are not planned.
