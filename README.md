# Sin Fetish Audio

Custom firmware for the M-VAVE FM-1, the tools to measure and shape it, and the plan for a groovebox of our own.

| | |
|---|---|
| **Earth Creature Beats** (`app/`) | five engines: Otter, Cat, Fox, Hopper, Beaver. [Read more](app/README.md) |
| **Alien Stack** (`visitors/`) | Mantis, Reptiles, Nordics, Greys, Ship. [Read more](visitors/README.md) |
| **SFA Console** (`tools/sfa console`) | simulate, render, measure and flash, plus the analyzer, sample lab and prototype planning pages |
| **Prototype** (`hardware/`, `core/`) | specs, parts, decisions and the engine architecture for our own box |

## Start

```
git clone --recurse-submodules git@github.com:SoulandCode69/sfa.git
cd sfa
tools/sfa doctor        # what this machine can do
tools/sfa console       # the console, at http://localhost:8590
```

Needs Python 3 and a C compiler for the simulator. Flashing needs the fm1-base toolchain and, on Windows, Windows Python with
`pip install mido python-rtmidi` (WSL has no USB MIDI). The simulator loop works without any of that.

## The loop

`tools/sfa sim ecb` then `render`, `report`, `analyze`, and `up ecb` to flash only when the renders are clean. Same for `as`.
One unit on USB at a time. Full detail: [docs/WORKFLOW.md](docs/WORKFLOW.md).

## Layout

```
app/  visitors/   the two firmwares (engine/, tests/, app.c)
core/             portable SFA ONE engine
base/             fm1-base (submodule)
sfa/              console pages and the unit registry
tools/            sfa, go.sh, audit.py, release.py, sfa_sample.py, repo_check.py
hardware/         prototype research, comparison, plan
docs/             workflow, console, architecture, formats, agents, samples, flash log
site/             the static public page
```

## Docs

[WORKFLOW](docs/WORKFLOW.md) · [CONSOLE](docs/CONSOLE.md) · [ARCHITECTURE](docs/ARCHITECTURE.md) · [FORMATS](docs/FORMATS.md) ·
[AGENTS](docs/AGENTS.md) · [SAMPLES](docs/SAMPLES.md) · [flash log](docs/notes.md) · [CHANGELOG](CHANGELOG.md) · [rules](CLAUDE.md)

## Licence

GPL-3.0-only for our code (see [LICENSE](LICENSE)); fm1-base is MPL-2.0; samples are CC0. Details in [NOTICE](NOTICE).
