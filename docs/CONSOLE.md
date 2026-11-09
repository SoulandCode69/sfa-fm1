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

* **SFA Analyzer** (`sfa/analyzer/index.html`): one file, no install, our own code. Spectrum with peak hold, smoothing and
  slope; spectrogram with a time axis shared with the waveform; peak, true peak, RMS, crest; BS.1770 loudness (integrated,
  momentary, short-term, range); stereo correlation, side energy and a goniometer; a click finder that marks the waveform;
  A/B overlay of up to five files; a compare table with CSV export; playback of a selection.
* **`tools/sfa`**: doctor, sim, render, report, analyze, flash, up, publish, console.
* **SFA Console** (`tools/sfa console`, a page at localhost:8590): one card per unit with its simulator and package status, latest render verdicts and
  the latest build from the flash log; buttons for Sim, Render, Report, Check all and Flash (with a confirm step) that stream their output into
  the page; the Analyzer embedded and fed with the latest renders; the doctor; the flash log; the roadmap. It listens on 127.0.0.1 only, checks the
  Host header, and accepts actions only with a custom header, from a fixed list of commands.
* **The simulator**: the real firmware compiled for the host, driven by scripts, with WAV capture and screenshots.

## Next, in order

1. **Preset browser**: the audit already renders every Otter, Fox and Greys preset. Show them as a grid with their spectra, so
   a harsh or quiet preset is visible before it is chosen.
2. **Engine template**: one folder to copy for a new engine (`engine.h` API, a test script, a render recipe, an audit entry).
3. **Sample lab**: drop a WAV in, slice it, audition it against the engines, pack it for the unit (`sfa_sample.py` is the packer).
4. **Prototype target**: `core/` (the portable SFA ONE engine) built for a second platform behind the same console.

## Principles

* The simulator comes first. Nothing is flashed that has not been rendered and measured.
* Own code, or permissive libraries. The analyzer has no dependencies.
* Measurements are the contract: `report.json` is what a person, a script or an agent reads to decide.
