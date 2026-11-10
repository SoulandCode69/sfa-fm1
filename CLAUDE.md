# Working on SFA (rules for people and agents)

Sin Fetish Audio: custom firmware for the M-VAVE FM-1 (Earth Creature Beats in `app/`, Alien Stack in `visitors/`), the SFA
console and tools, and the plan for our own groovebox (`hardware/`).

## Standing rules

1. Build on fm1-base (`base/`, a submodule) and **simulate first**: `tools/sfa sim <unit>`. Nothing is flashed unless it has
   been rendered and measured (`tools/sfa up <unit>`).
2. **Flash one unit at a time**, with mirror tabs closed. Agents never flash. Log every flash in `docs/notes.md`.
3. Audio stays clean. Distortion, clicks and zipper noise are bugs. `report.json` is the judge.
4. Samples are original and CC0, with their source in `docs/SAMPLES.md`.
5. Our own code or permissive libraries. No new dependency without saying why.
6. Do not rename `app/` or `visitors/` (fm1-base conventions and `firmware.json` depend on them).
7. Do not commit build output, renders or WAVs (`tools/repo_check.py` looks for them).

## Map

| Path | What |
|---|---|
| `app/`, `visitors/` | the two firmwares: `engine/`, `tests/`, `app.c` |
| `core/` | portable SFA ONE engine (the hardware track) |
| `sfa/` | console pages: `hub/` (`index.html`, `workshop.js`, `hub.css`), `analyzer/`, `samplelab/`, and `units.json` (the registry) |
| `tools/` | `sfa` (the command), `go.sh`, `audit.py`, `release.py`, `sfa_sample.py`, `repo_check.py` |
| `hardware/prototype/` | `direction.json` (the brief), `spec.json` and `parts-research.json` (research), `compare.json`, `plan.json` (edited in the console) |
| `docs/` | WORKFLOW, CONSOLE, PROTOTYPE, ARCHITECTURE, FORMATS, AGENTS, SAMPLES, notes (flash log) |
| `site/` | the static public page |

## Commands

`tools/sfa doctor | units | sim | render | report | analyze | flash | up | ports | pack | console | publish`.
Start with `tools/sfa console`.

## Before a commit

`python3 tools/repo_check.py` (large files, secrets, build output), and `tools/sfa sim ecb` and `tools/sfa sim as` if
firmware changed. Commit messages say what changed and why.
