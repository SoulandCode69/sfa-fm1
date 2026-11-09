# SFA pack format (draft 1)

Plain files, no code. A pack is a folder, or the same folder zipped as `name.sfapack`. Everything is optional except the manifest.

```
mypack.sfapack
  manifest.json
  presets/otter-glass-keys.json
  kits/trap-tight.json
  samples/01_KICK.wav
  patterns/four-on-floor.json
```

## manifest.json

```json
{ "schema": "sfa-pack/1", "name": "Tight Trap", "author": "Johnny", "created": "2026-11-09",
  "licence": "CC0", "notes": "kit + two basses",
  "requires": { "otter": 1, "cat": 1 } }
```

`requires` lists engine ids and the version each item was made for. A loader that does not know an engine skips that item and
says so; it never guesses.

## Preset (`presets/*.json`)

```json
{ "schema": "sfa-preset/1", "engine": "otter", "version": 1, "name": "GLASS KEYS",
  "params": [[70,45,80,15],[0,55,20,35],[100,20,60,0],[30,10,0,0],[40,0,0,100]], "tags": ["keeper"] }
```

`params` is the engine's `params[page][4]`: the number of rows is `npages`, every value is 0..127. Names are upper case, 12 characters.

## Kit (`kits/*.json`)

```json
{ "schema": "sfa-kit/1", "engine": "cat", "version": 1, "name": "TRAP TIGHT",
  "pads": [ { "pad": 0, "sample": "01_KICK.wav", "tune": 64, "decay": 50, "level": 100 } ] }
```

A pad refers to a sample by file name inside the pack, or to a built-in voice by name. Missing samples leave the pad on its
built-in voice.

## Sample (`samples/*.wav`)

Any WAV: 8/16/24/32-bit, mono or stereo, any rate. The packer converts per target profile:

| Profile | Rate | Channels | Limit |
|---|---|---|---|
| `fm1` | 22.05 kHz | mono | 16 samples, each up to 4.17 s, about 4.2 s in all (`samples.bin`, format SFA2) |
| `proto` | 48 kHz | stereo | microSD; count and length set by the card |

Names are `NN_NAME.wav`, `NN` the slot, `NAME` up to 9 characters on the FM-1.

## Pattern (`patterns/*.json`)

```json
{ "schema": "sfa-pattern/1", "name": "FOUR ON FLOOR", "steps": 16, "swing": 0, "bpm": 120,
  "tracks": [ { "engine": "cat", "pad": 0,
    "steps": [ { "i": 0, "vel": 110 }, { "i": 4, "vel": 100 }, { "i": 8, "vel": 110 }, { "i": 12, "vel": 100 } ],
    "locks": [ { "i": 12, "page": 0, "param": 2, "value": 90 } ] } ] }
```

`locks` are per-step parameter changes. This is the part of the model that does not exist in either firmware yet.

## Rules for any loader

1. Check `schema`; refuse an unknown major version.
2. Check `engine` and `version`; skip, do not guess.
3. Clamp every parameter to 0..127; truncate names to the target length.
4. Check sample count and size against the target profile before writing anything.
5. Ignore unknown fields so later versions can add them.
6. Never run anything from a pack.

## Status

Draft. The console's Presets page exports and imports tags and notes as `sfa-presets/1` (a flat file, not this format) as a
stand-in; the real `tools/sfa pack` and the engine-side loaders are step 3 and 4 in `ARCHITECTURE.md`.
