# ALIEN STACK

A second firmware for the M-VAVE FM-1 by Sin Fetish Audio, on the Earth Creature Beats platform. Same hardware, another
world: **NORDICS** (poly synth), **MANTIS** (a synthesized drum machine: 16 voices, 8 kits, no samples), **REPTILES**
(16 chromatic alien one-shots + 24 synth basses), **GREYS** (the sampler: sample the output, load your own, chop, play;
a drone / transmission / sweep / choir bank on board), and the **SHIP** (8 patterns, scenes, a 16-slot song).

Build and flash (the Alien Stack unit alone on USB): `tools/go.sh visitors`. The package: `visitors/build/alien.fwsc`
(identity FM-1_5800, USB name Alien Stack); the laptop pages at http://localhost:8578/; the distributable site and zip
land in `dist/alien-<version>/` and `dist/AlienStack-<version>.zip`.
Simulator: `python3 base/tools/build.py visitors --host && ./visitors/build/host/alien_host visitors/tests/basic.txt visitors/build/host/out`.

Audio: `python3 tools/render_visitors.py` (the REPTILE bank and the Greys' default bank, original and CC0), then
`python3 tools/pack_samples.py --pack assets/vpack --out visitors/samples_data.c` and
`python3 tools/sfa_sample.py assets/vuser --out visitors/samples.bin`.

Build 2: **parameter locks** (on the Ship, hold a step, press the being's button, turn K1-K4: that knob lands there
when the step plays; a magenta mark on the step), **polymeter** (PRESET knob on the Ship: each being's own loop length,
0 = the pattern's; the grid shows each lane's position), a **reverb bus** (FX page: VERB, SIZE; the delay beside it),
segmented 'stack' bars, the ALIEN / STACK header.

Build 1: the look (the five visitors drawn as vector art, a cosmic palette, the mothership HOME with a
starfield), MANTIS complete, the REPTILE bank, the Greys' bank, the SHIP = ECB's sequencer with everything it learned.
NORDICS and the REPTILES' synth half carry ECB's engines for now; their own deeper engines (mod matrix, chorus /
shimmer, granular Greys, parameter locks, polymeter, MIDI clock, a reverb bus) are the next builds.
