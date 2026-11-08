# SFA core: our own engines

Portable synthesis code for everything Sin Fetish Audio builds: the FM-1 firmwares today, plugins and our own
hardware next. Plain C99, single-precision float, no allocation, no libm (the host hands in `sin` and `exp2` once), no
globals: one `one_t` per instance, so a plugin runs one per track and a firmware one per slot.

## SFA ONE (`sfa_one.h` / `sfa_one.c`)

An **exciter → resonator → folder → ladder** voice, four of them. The exciter is a phase-distortion oscillator (sine
to buzz) plus band-limited noise and a burst on the attack; the resonator is three tuned waveguide combs with damping
and feedback (SPREAD: unison, a fifth and an octave, inharmonic), so one engine covers plucks, bowed strings, bars,
bells, pipes, basses and pads; then a wavefolder and a four-pole ladder with resonance, two envelopes, two LFOs, glide,
unison and a sub. Four **macros** (TONE BODY COLOR MOTION) each bend several targets, so the four knobs of a small
instrument play the whole thing; six pages sit behind them. 16 presets to start.

- `one_render.c`: renders every preset to WAV with peak / RMS / crest (`cc -O2 -o build/one_render one_render.c sfa_one.c -lm && ./build/one_render build`)
- `one_bench.c`: the CPU cost of four voices (`cc -O2 -o build/one_bench one_bench.c sfa_one.c -lm && ./build/one_bench`)
- On the FM-1: `proto/` (SFA Proto) carries it in the fourth slot through `proto/engine/one_adapter.c`.

Measured (host): every preset tunes to its note; peaks -2..-8 dBFS; four voices cost about twice ECB's whole render,
which sizes the prototype's processor (the FM-1 runs it with three voices on its second core).
