# Where agents help

An agent is a second Claude working on one bounded job while you or the main session do something else. They pay off when the
job has a clear input, a clear output, and a measurement that says whether it worked. `tools/sfa` makes that true for most
SFA work, because every change can be simulated, rendered and measured without hardware.

## Good jobs for agents

| Job | Input | Output | How it is checked |
|---|---|---|---|
| **New engine or voice** (code generation) | the engine API in `app/engine/engine.h`, one existing engine as a pattern, a sound brief | a new `engine/*.c`, a test script, a render recipe | `tools/sfa sim`, then `tools/sfa report` shows no flag |
| **Regression tests** | a feature description | new `expect` lines and a script in `tests/` | the suite passes, and fails when the feature is removed |
| **Preset ideas** | an engine and a mood ("dub chords", "alien arps") | 24 parameter sets in the preset table format | `tools/audit.py` and the analyzer: level, harshness, clicks |
| **Preset triage** | `renders/<unit>/<stamp>/report.json` after an audit render | a ranked list of presets that are too hot, too quiet, too bright, or click | rerun after the trim table is edited |
| **DSP review** (a fresh pair of eyes) | one engine file and the click rules in `docs/WORKFLOW.md` | a list of places a voice can start, stop or retrigger with a step | each finding reproduced in a render before it is fixed |
| **Sample sourcing and licensing** | the sample list in `docs/SAMPLES.md` | licence and provenance notes, gaps | a human reads it before anything ships |
| **Hardware research** | a question ("which SoCs run two engines at 48 kHz with USB audio class 2?") | a short comparison with sources | the numbers are checked against datasheets |
| **Docs** | a diff | README, notes and flash-log entries | read once |

## Rules that keep it safe

* Agents work in the simulator. They never flash a unit. A person runs `tools/sfa up` or `tools/sfa flash`.
* One agent per area, in its own branch or worktree, so two agents never edit the same engine file.
* The brief names the files, the command that proves success, and what is out of scope. An agent with a vague brief returns a
  confident answer to a different question.
* The main session reads `report.json` and the diff, not the agent's summary alone.
* Anything that makes sound gets listened to on the unit before it ships. The analyzer finds problems; it does not judge taste.

## A brief that works

> Add a CHOIR voice to the Greys engine in `visitors/engine/greys.c`, 8 presets, same parameter pages as the others.
> Read `visitors/engine/greys.c` and `app/engine/engine.h` first. Success: `tools/sfa sim as` passes, a new test in
> `visitors/tests/basic.txt` plays each preset, and `tools/sfa render as audit` then `tools/sfa report as` shows every new
> preset between -9 and -3 dBFS peak with no clicks. Do not touch the sequencer or the mixer. Report the numbers.
