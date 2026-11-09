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

* **SFA Analyzer** (`sfa/analyzer/index.html`): one file, no install, our own code. Spectrum, spectrogram, peak / true peak / RMS,
  BS.1770 loudness, stereo, click finder, A/B with loudness matching and a difference plot, a compare table with CSV export,
  render history and favourites (from the console).
* **SFA Sample Lab** (`sfa/samplelab/index.html`): chop by transients, equal parts, beat grid or silence; preview as the unit
  hears it; export a kit zip or send it to a unit's bank (`tools/sfa pack`, with a backup of the old bank).
* **`tools/sfa`**: doctor, units, sim, render, report, analyze, flash, up, ports, pack, publish, console.
* **SFA Console** (`tools/sfa console`, localhost:8590): Units (sim, render, report, check, flash with a one-FM-1-on-USB check),
  Analyzer and Sample Lab embedded, and the hardware pages: Planner, Decisions, Engine map, Architecture, Presets, Compare, Research.
  Docs and changelog, the doctor, the flash log and the roadmap sit under System. It listens on 127.0.0.1 only, checks the Host
  header, and accepts actions only with a custom header, from a fixed list of commands. Plan edits save to `hardware/prototype/plan.json`.
* **Site** (`site/`): a static public page, previewed at `/site/`.
* **The simulator**: the real firmware compiled for the host, driven by scripts, with WAV capture and screenshots.

## Next, in order

1. **Engine v2 and packs** (see ARCHITECTURE.md): engine `id` and `version`, `SR` as a build flag, `tools/sfa pack`, simulator loads a pack.
2. **FM-1 data region spike**: can presets live as data on the FM-1?
3. **Engine template**: one folder to copy for a new engine.
4. **Prototype target**: `core/` and one engine on a development board, compared render for render with the host.

## Principles

* The simulator comes first. Nothing is flashed that has not been rendered and measured.
* Own code, or permissive libraries. The analyzer has no dependencies.
* Measurements are the contract: `report.json` is what a person, a script or an agent reads to decide.
