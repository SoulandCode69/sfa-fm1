# SFA as a console

SFA is becoming one place that holds several engines and several tools, so a new engine or a new box plugs in instead of
being bolted on. This is the shape, what exists today, and what comes next.

## The layers

```
 units       sfa/units.json     what exists: ECB, Alien Stack, (later) a prototype engine, a hardware target
 recipes     sfa/units.json     what can be rendered from a unit: song, tests, kits, levels, audit
 tools       tools/, sfa/       the analyzer, the audit, the sample packers, the flasher, the mirror pages
 console     tools/sfa          one command that walks a unit through sim -> render -> measure -> flash
 hub         sfa/hub/index.html  the SFA Console: units as modules, the analyzer, doctor, flash log, roadmap
```

A unit is a firmware (a directory with an `fm1.toml`, an `engine/` folder, `tests/` scripts and a simulator binary name). Adding
one is a few lines in `units.json`. Everything in the console reads that file, so it works for the next firmware without code changes.

## What exists

The console (`tools/sfa console`, localhost:8590) has three groups.

**Workshop**
* **Units**: per firmware, a strip of four steps (simulate, measure, on USB, flashed), Check all, Sim, Render, Report, and a
  Flash button that checks USB first, shows a checklist, and refuses with zero or two FM-1s. After a flash, Log this flash adds
  a row to `docs/notes.md`.
* **SFA Analyzer** (`sfa/analyzer/index.html`, also works on its own): spectrum, spectrogram, true peak, BS.1770 loudness,
  stereo, click finder, A/B with loudness matching and a crossfade, a difference plot, render history, favourites, verdict
  filters, a what-to-do line under each flag.
* **SFA Sample Lab** (`sfa/samplelab/index.html`, also works on its own): chop by transients, equal parts, beat grid or
  silence; nudge, join, undo; guess names; preview as the unit hears it; play the kit; export a zip (with a pack manifest) or
  send it to a unit's bank.
* **Presets**: every preset and kit name from both firmwares, with tags, notes, bulk tagging, export and import.
* **Patterns**: a step sketchbook and arrangement lane; exports `sfa-pattern/1`.
* **Toolbox**: tempo, notes, the FM-1 bank, level matching, card space, battery.

**Prototype**: Direction, Layouts, Planner, Decisions, Engine map, Architecture, Compare, Research. Edits save to
`hardware/prototype/plan.json`.

**System**: Docs and changelog, Doctor, Flash log, Roadmap.

The server listens on 127.0.0.1 only, checks the Host header, and accepts actions only with a custom header, from a fixed
list of commands. Page code is in `sfa/hub/` (`index.html`, `workshop.js`, `hub.css`).

## Next, in order

1. **Engine v2 and packs** (see ARCHITECTURE.md): engine `id` and `version`, `SR` as a build flag, `tools/sfa pack`, simulator loads a pack.
2. **FM-1 data region spike**: can presets live as data on the FM-1?
3. **Engine template**: one folder to copy for a new engine.
4. **Prototype target**: `core/` and one engine on a development board, compared render for render with the host.

## Principles

* The simulator comes first. Nothing is flashed that has not been rendered and measured.
* Own code, or permissive libraries. The analyzer has no dependencies.
* Measurements are the contract: `report.json` is what a person, a script or an agent reads to decide.
