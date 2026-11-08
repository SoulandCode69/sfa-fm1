/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * SFA ONE: our own synthesis engine. Portable C99, single-precision float, no allocation, no platform calls: the
 * same file runs in a firmware, a plugin, a web build or a hardware prototype. The host passes its math (sin, exp2)
 * once at init so a fast implementation can be used on small chips.
 *
 * The voice: an EXCITER (a phase-distortion oscillator, sine to buzz, plus a shaped noise / impulse burst) drives a
 * RESONATOR (three tuned waveguide combs with damping and feedback: pluck, bow, bar, pipe or a sustained tone), then
 * a FOLDER, a four-pole FILTER and the AMP. Two envelopes, two LFOs, and four MACROS that each move several targets.
 *
 * Parameters are 0..127 bytes grouped in pages of four, so a firmware's four knobs and a plugin's panel share them:
 *   page 0 MACROS:    TONE BODY COLOR MOTION         (the performance page: each macro is a curated set of targets)
 *   page 1 EXCITER:   SHAPE NOISE BURST PITCHENV
 *   page 2 RESONATOR: DECAY DAMP SPREAD MIX           (SPREAD detunes the three combs: unison .. chord .. inharmonic)
 *   page 3 FILTER:    CUTOFF RES FOLD ENVAMT
 *   page 4 ENVELOPE:  ATTACK DECAY SUSTAIN RELEASE
 *   page 5 MOTION:    LFO1RATE LFO1>PITCH LFO2RATE LFO2>FILT
 *   page 6 VOICE:     GLIDE UNISON SUB LEVEL */
#ifndef SFA_ONE_H
#define SFA_ONE_H
#include <stdint.h>

#define ONE_PAGES 7
#define ONE_NPARAM 4
#define ONE_VOICES 4
#define ONE_COMBS 3
#define ONE_COMB_LEN 1024            /* samples, int16: down to ~43 Hz at 44.1 kHz (the sub covers below); 6 KB a voice */

typedef struct {
    float (*sinf_)(float);           /* sin of radians */
    float (*exp2f_)(float);          /* 2^x */
    float sr;                        /* sample rate */
} one_host_t;

typedef struct {
    int16_t buf[ONE_COMBS][ONE_COMB_LEN];
    float lp[ONE_COMBS];
    uint32_t w;
    float ph, nph, nph2, env, fenv, hz, hz_target, pitch_env, lfo1, lfo2, age_f;
    float flt[4];                    /* the ladder's four poles */
    float sub_ph;
    int32_t note;
    uint8_t on, gate, stage;
    uint32_t age, rng;
} one_voice_t;

typedef struct {
    one_host_t host;
    uint8_t p[ONE_PAGES][ONE_NPARAM];
    one_voice_t v[ONE_VOICES];
    uint32_t age_clock;
    float lfo1_ph, lfo2_ph, out_gain;
    int32_t last_note;
} one_t;

void one_init(one_t *o, const one_host_t *host);
void one_note(one_t *o, int32_t note, uint32_t vel, int on);
void one_render(one_t *o, float *out, uint32_t n);                  /* adds n mono frames into out (nominal 1.0) */
void one_panic(one_t *o);
extern const char *const ONE_NAMES[ONE_PAGES][ONE_NPARAM];
const char *one_page_title(uint32_t page);
const char *const *one_page_names(uint32_t page);
const char *one_param_text(const one_t *o, uint32_t page, uint32_t k, char *buf);   /* words for a value, or 0 */

/* presets: name + the seven pages */
typedef struct { const char *name; uint8_t p[ONE_PAGES][ONE_NPARAM]; } one_preset_t;
extern const one_preset_t ONE_PRESETS[];
extern const uint32_t ONE_NPRESETS;
void one_preset_load(one_t *o, uint32_t i);

#endif
