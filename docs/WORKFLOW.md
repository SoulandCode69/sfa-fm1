# The SFA workflow

One command, `tools/sfa`, runs the loop: simulate, render, measure, flash. The simulator comes first, always. A unit is flashed
only after its renders measure clean.

## Where things live

Source of truth is the repo in WSL (`~/sfa-fm1`). Windows gets a tidy copy of what you open or install.

```
WSL  ~/sfa-fm1/
  app/            Earth Creature Beats (ECB)         app/tests/*.txt    simulator scripts
  visitors/       Alien Stack (AS)                   visitors/tests/*.txt
  base/           fm1-base: platform, build, simulator, installer
  core/           portable SFA engine (the hardware track)
  sfa/            units.json (the registry), hub/ (console), analyzer/, samplelab/
  hardware/       prototype research and plan     site/   the public page
  tools/          sfa (the console), go.sh (build + flash), audit.py, release.py, ...
  renders/        WAVs the simulator rendered, per unit and timestamp, with report.md   (git-ignored)
  docs/           notes.md (flash log), SAMPLES.md, this file

Windows  %USERPROFILE%\SFA\                 (written by `tools/sfa publish`, also run by the flash launchers)
  Analyzer\SFA Analyzer.html     double-click it, drop WAVs on it
  Firmware\ecb\ECB-<stamp>.fwsc  every package you built, to keep or reflash
  Firmware\as\...
  Renders\ecb\<stamp>\*.wav      the renders and their report
  Launchers\*.bat                double-click: Doctor, Sim, Flash ECB, Flash Alien Stack, Analyzer
```

## The loop

Run these in WSL from `~/sfa-fm1` (or use the `.bat` launchers).

| Step | Command | Needs | Takes |
|---|---|---|---|
| Check the machine | `tools/sfa doctor` | nothing | 1 s |
| Simulate and check | `tools/sfa sim ecb` | python, a C compiler | ~5 s |
| Render WAVs | `tools/sfa render ecb` (recipes: `song tests kits levels audit`) | same | seconds |
| Measure | `tools/sfa report ecb` | numpy | seconds |
| Look and listen | `tools/sfa analyze ecb` (add `--fresh` to render first) | a browser | opens at localhost:8590 |
| Flash | `tools/sfa flash ecb` | toolchain, Windows Python | ~1 min |
| All of it | `tools/sfa up ecb` (or `as`) | everything | stops if a render is flagged |
| Everything in one page | `tools/sfa console` (or the `SFA Console.bat` launcher) | a browser | opens at localhost:8590 |
| Who is on USB | `tools/sfa ports` | Windows Python with mido + python-rtmidi | 2 s |
| Pack a sample kit | `tools/sfa pack <dir> <unit>` (or send from the Sample Lab) | python | seconds |
| Copy to Windows | `tools/sfa publish` | WSL | 1 s |

`ecb` and `as` can also be written `app` and `visitors`. `tools/sfa units` lists what is registered.

A normal session on one unit:

1. Edit the engine or the sequencer.
2. `tools/sfa sim ecb`. All checks must pass.
3. `tools/sfa analyze ecb --fresh`. Drop in anything else you want to compare (an MPC recording, last week's render).
4. `tools/sfa up ecb`. It re-runs the checks, renders, measures, and flashes only if nothing is flagged
   (`--force` overrides, `--no-flash` stops before the flash).
5. Log the flash in `docs/notes.md`.

One unit on USB at a time, and close any mirror tab first (a browser tab holding the MIDI port is the usual reason a flash
says the FM-1 was not found).

## What the verdict means

The same rules run in the browser analyzer and in `tools/sfa report`, and they agree (checked against each other on 20 files:
loudness within 0.01 LU, true peak within 0.02 dB, same clicks found).

* **flag**: any sample at full scale, a true peak over 0 dBTP, or four or more clicks.
* **watch**: a true peak above -1 dBTP, any click, DC offset over 0.5 %, or stereo correlation dipping below -0.3.
* **clean**: none of the above.

A click is a step of at least about -30 dBFS that is ten times bigger than the signal around it, at least four and a half
times bigger than any neighbouring step, in audio that was already sounding. A snare noise burst or a kick attack is busy
all round and passes. A one-sample step or spike in smooth audio does not. A step that recurs at an even spacing (a saw wrap or a square edge: the same step one period later and two periods later) is the waveform, not a click, and is not reported; one event seen on both channels counts once.

The renders are the simulator's output, so they test the DSP and the sequencer, not the USB or analogue path. For that, record
the unit into the MPC, drop the WAV into the analyzer, and compare it with the render of the same pattern.

## Troubleshooting

* `FM-1 not found` while flashing: close every `localhost:8577` / `8578` tab, unplug the other unit, run again.
* `doctor` says the toolchain is missing: the simulator loop still works. Only the device package needs the toolchain.
* The Windows `.bat` files assume the WSL distro is called `Ubuntu`. If yours differs, edit `BAT` in `tools/sfa.py`.
* `analyze` opens a blank page: the port may be taken. Use `tools/sfa analyze --port 8591`.
