# Changelog

Newest first. Firmware builds are also logged, with the flash, in `docs/notes.md`.

## 2026-11-10 (late night): violet identity

* **Colour**: the main accent is now electric violet (actions, selection, the active page), on the same dark base. Magenta is the pop, kept for Flash and the pattern steps; cyan for links, info and the current page in the header. Lime is gone.
* **Logo**: a rounded white S (one cycle of a wave) on a violet tile with a magenta signal dot; used in the header and as the tab icon on the console, Analyzer, Sample Lab and site.
* **Wordmark**: SFA in bold display type, Console as a spaced secondary label.

## 2026-11-10 (night): brand

* **Dark base** by default (a Light switch remains): near-black indigo surfaces, hot magenta for the action, acid lime for selection and tools, cyan for links and info.
* **Emblem**: a square-wave S in ink on a magenta tile with a lime signal dot (`sfa/hub/emblem.svg`, `site/emblem.svg`). It is the header mark and the browser-tab icon on the console, the Analyzer, the Sample Lab and the site.
* **Header**: SFA wordmark with Console as the secondary label, a section / page crumb, and the page name in the tab title.

## 2026-11-10 (evening): polish pass

* One consolidated stylesheet (`sfa/hub/hub.css`): a single button height with four roles (action, tool, plain, ghost), consistent fields, card and table styles, state pills with a dot, a joined four-step strip, a full-height ink rail.
* Units cards lead with an overall status (Not checked, Checked, Ready to flash, Flagged); secondary actions are quiet, Check all and Flash stand out.

## 2026-11-10 (later): designer, agents, colour

* **Colour**: a new identity across the console and both tools: an ink top bar and rail, a paper work area, magenta for action, violet for tools, lime as the signal on ink, a three-colour stripe.
* **Prototype Designer** (replaces Layouts): drag keys, pads, screen, encoders, buttons, knob, touch strip and speaker on a to-scale top view; front and side views; four starting concepts (Step Row, Butterfly, Terrace, Rails); four styles (clean, assembled / industrial, recycled junk, wood ends) with colours, materials and vibe notes; live size, pitch, counts and warnings; undo; saved designs and a side-by-side compare; 1:1 print, SVG and JSON export.
* **Agents** (new): Prototype, Sound, Hardware and Workflow. Each has a built-in review of your own data that needs no network, and a chat that works when the console is started with `ANTHROPIC_API_KEY`. Without a key, Copy a brief for Claude gives the same context as text.
* **Online mode**: `tools/sfa console --online` behind `SFA_PASSWORD`, with run, flash and USB turned off; `railway.json`, `Procfile`, `docs/ONLINE.md`. Not deployed yet.

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
