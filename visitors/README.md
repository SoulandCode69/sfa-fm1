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

Build 5: **the declick pass**: a Mantis voice that retriggers (and the open hat when the closed hat chokes it) fades
out in a tail instead of cutting; a Reptiles 808 retrigger fades the old note over 2 ms; mixer levels, pans, mutes and
sends ramp across the block (no zipper). **The song builder** (fourth Ship page on PRESET): 16 slots in two rows;
keys pick a slot, SLOT / PATTERN / REPEAT (1-8 bars, below 1 ends the song there) / SCENE (a mixer scene recalled on
entering the slot). CHAIN plays the song honouring repeats and scenes. Settings magic ALI3.

Build 4: **the Alien Stack layout**: every parameter page is four horizontal stack rows (name, 12 segments, value) with
a context strip under them (a synth: the presets either side of the one you are on; Mantis: the sixteen voices, the one
you hit lit). **The Mothership**: per-being **FX sends** (mixer page 5: SENDS; the reverb and delay are fed from them,
so drums stay dry while pads swim), **hold SIGNAL** = a fill on every being that snaps back when you let go (a tap is
still a new signal), and a third Ship page (PRESET knob): **live transpose** per being, -24..+24. **DSP**: the Nordics
gain an ensemble (DETUNE past a third brings in three modulated taps) and a transparent voice stage; the Reptiles'
DRIVE past half becomes a wavefolder, with a second detuned saw for width and a keytracked filter; Mantis: the closed
hat chokes the open one, velocity brightens the tone, kicks carry a 2nd harmonic that dies first; the Greys' filter and
level follow velocity. Every synth re-measured (tools/audit.py --app visitors).

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
