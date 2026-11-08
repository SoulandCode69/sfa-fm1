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

Build 3: **GREYS is a synth** now: a wavetable / FM engine (morphing table in a three-voice unison, an FM modulator,
sub, filter with drive, ADSR, LFO to morph / pitch / filter, glide, chorus), 24 presets on ALGO, measured level trims.
**The Mothership**: OCT- / OCT+ on the Ship page the keys to steps 1-16 / 17-32 (the other half dims); hold GEN and
turn K1 pulses / K2 steps / K3 rotation for a live **Euclidean** pattern on the focused being (a tap of GEN is still a
groove). **Mantis LAYER** page: an ABDUCT hit (16 processed alien drums, a new pack) under any voice at its pitch;
DIGITAL, GLITCH and SAUCER come layered. **48 new Nordics presets** (pads, choirs, keys, bells, arps) and **24 new
Reptile basses**, all audited (tools/audit.py --app visitors). ARP toggles the Nordics' arpeggiator.

Build 2: **parameter locks** (on the Ship, hold a step, press the being's button, turn K1-K4: that knob lands there
when the step plays; a magenta mark on the step), **polymeter** (PRESET knob on the Ship: each being's own loop length,
0 = the pattern's; the grid shows each lane's position), a **reverb bus** (FX page: VERB, SIZE; the delay beside it),
segmented 'stack' bars, the ALIEN / STACK header.

Build 1: the look (the five visitors drawn as vector art, a cosmic palette, the mothership HOME with a
starfield), MANTIS complete, the REPTILE bank, the Greys' bank, the SHIP = ECB's sequencer with everything it learned.
Still to come: a mod matrix for the Reptiles, chorus / shimmer on the Nordics, per-being reverb sends, MIDI clock.
