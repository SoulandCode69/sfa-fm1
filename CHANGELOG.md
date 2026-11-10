# Changelog

Newest first. Firmware builds are also logged, with the flash, in `docs/notes.md`.

## 2026-11-10: the workshop build

* **Look**: a bright, high-contrast theme on every page (light by default, dark by switch), new navigation in three groups: Workshop, Prototype, System.
* **Units**: a four-step strip per firmware (simulate, measure, on USB, flashed), a flash checklist that will not go ahead with zero or two units, and a Log this flash button that writes to `docs/notes.md`.
* **Analyzer**: a 30 ms crossfade on A/B switches, verdict filters and a next-problem jump (N), a what-to-do line under each flag.
* **Sample Lab**: undo and redo, name guessing, slice nudge and join from the keyboard, loop audition, play the whole kit, tail fade choice, a pack manifest in the zip, clearer send-to-unit feedback.
* **Presets**: engine chips with counts, a favourite tag, bulk tag and clear.
* **Patterns** (new): a step sketchbook with a click preview, an arrangement lane, export and import as `sfa-pattern/1`.
* **Toolbox** (new): tempo and delay times with tap, note to pitch, the FM-1 bank budget, level matching, card space, battery life.
* **Prototype**: Direction (brief, controls, screen, sound, I/O, parts search, pipeline, open questions) and Layouts (Step Row, Butterfly, Terrace, drawn to scale with a 1:1 print). Decisions, Compare, Planner defaults and the Roadmap updated. See `docs/PROTOTYPE.md`.

## 2026-11-09 (evening): DSP pass on ECB and Alien Stack

Found with the audit renders (every Otter, Fox and Greys preset, played twice each) and fixed in both firmwares:

* **Fox bass retrigger**: a new note set the amplitude envelope to zero while the previous note's tail was still sounding, a step to silence. It now ramps up from where it is.
* **Fox bass filter envelope** rises over about 0.5 ms instead of jumping open in one sample.
* **Preset, octave and mute changes** stopped the engine on the spot (a step if anything was ringing). They now fade the engine's tail over about 9 ms (`soft_panic` in `app.c`). Stop and flash-save still cut hard.
* **Click finder** (analyzer and `tools/sfa report` agree): saw wraps and square edges that recur every period are no longer reported as clicks, one event on both channels counts once, and the isolation window is wider.

Audit clicks before and after: ECB Fox 35 to 2, AS Fox 141 to 1, AS Otter 3 to 0, AS Greys 2 to 0. Simulator checks: 127 of 127 in both. Needs a flash to hear it.

## 2026-11-09: the SFA software layer

* **Console** (`tools/sfa console`): Units, Analyzer, Sample Lab, Planner, Decisions, Engine map, Architecture, Presets,
  Compare, Research, Docs, Doctor, Flash log, Roadmap. A USB check allows one FM-1 on the bus when flashing.
* **Analyzer**: A/B mode with loudness matching and a difference plot; render history and favourites.
* **Sample Lab**: chop by transients, equal parts, beat grid or silence; preview as the unit hears it; export a kit or send it to a unit's bank.
* **Prototype planning**: specs, costed parts list, decision log, engine and feature map, comparison board, architecture and pack format drafts.
* **Presets**: tag and note every ECB and Alien Stack preset; export and import tags.
* **Site**: a static landing page structure in `site/`.
* **Alien Stack**: Mantis drum onset ramp (needs a flash to take effect).
* **Repo**: first private push to `SoulandCode69/sfa` as a clean snapshot, with `base/` as the fm1-base submodule.
