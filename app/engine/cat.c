/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * CAT KIT: 8 pads on the first 8 white keys. Pads 0..5 are synthesised drums, pads 6..7 play the Fox sample bank
 * (pad 6 the whole sample, pad 7 pitched down an octave): kits are half synth, half sample from day one.
 *   K1 TUNE    kick/tom pitch           K3 SNAP    attack click / noise amount
 *   K2 DECAY   every pad's decay        K4 DRIVE   soft clip */
#include "fastmath.h"
#include "engine.h"

void fox_oneshot(float *out, uint32_t n, int slot);   /* pads 6 and 7 */

static const char *const NAMES[NPARAM] = {"TUNE", "DECAY", "SNAP", "DRIVE"};
static uint8_t par[NPARAM] = {50, 64, 70, 30};

enum { KICK, SNARE, CHAT, OHAT, CLAP, TOM, SMP_A, SMP_B, NPADS };
typedef struct { float env, ph, hz, nenv; uint8_t on; } pad_t;
static pad_t pad[NPADS];
static uint32_t rng = 0x5F3759DFu;
static volatile uint8_t trig[NPADS];
static volatile uint8_t trig_vel[NPADS];
static float vel[NPADS];

static void c_init(void) { uint32_t i; for (i = 0; i < NPADS; i++) pad[i].on = 0; }
static void c_panic(void) { c_init(); }

static void c_note(int32_t note, uint32_t v, int on)
{
    /* note 0..7 = pad (the app maps the first 8 white keys and MIDI 36..43 to 0..7) */
    if (!on || note < 0 || note >= NPADS)
        return;
    trig_vel[note] = (uint8_t)v;
    trig[note] = 1;
}

static void c_render(float *out, uint32_t n)
{
    uint32_t i, p;
    float decay = 1.0f - fm_exp2f(-5.0f - p01(par[1]) * 7.0f);
    float tune = 0.6f + p01(par[0]) * 1.2f;
    float snap = p01(par[2]);
    float drive = 1.0f + p01(par[3]) * 6.0f;
    for (p = 0; p < NPADS; p++)
        if (trig[p]) {
            trig[p] = 0;
            vel[p] = (float)trig_vel[p] * (1.0f / 127.0f);
            pad[p].on = 1;
            pad[p].env = 1.0f;
            pad[p].nenv = 1.0f;
            pad[p].ph = 0;
            pad[p].hz = p == KICK ? 160.0f * tune : p == TOM ? 220.0f * tune : 180.0f;
            if (p >= SMP_A) {
                fox_oneshot(0, 0, p - SMP_A);   /* start the sample voice */
                pad[p].on = 0;
            }
        }
    for (i = 0; i < n; i++) {
        float s = 0.0f, nz = noise(&rng);
        pad_t *k;
        k = &pad[KICK];
        if (k->on) {
            k->hz += (45.0f * tune - k->hz) * 0.0015f;     /* pitch sweep down */
            k->ph += k->hz * (1.0f / SR);
            if (k->ph >= 1.0f) k->ph -= 1.0f;
            k->env *= decay;
            s += fm_sinf(k->ph * 6.2831853f) * k->env * vel[KICK] * 1.2f + nz * k->env * k->env * snap * 0.3f * vel[KICK];
            if (k->env < 0.001f) k->on = 0;
        }
        k = &pad[SNARE];
        if (k->on) {
            k->ph += k->hz * (1.0f / SR);
            if (k->ph >= 1.0f) k->ph -= 1.0f;
            k->env *= decay;
            k->nenv *= decay * 0.999f;
            s += (fm_sinf(k->ph * 6.2831853f) * k->env * 0.5f + nz * k->nenv * (0.4f + snap * 0.6f)) * vel[SNARE];
            if (k->nenv < 0.001f) k->on = 0;
        }
        k = &pad[CHAT];
        if (k->on) {
            k->env *= decay * decay * 0.995f;
            k->ph += (nz - k->ph) * 0.6f;                /* crude highpass: fast noise */
            s += (nz - k->ph) * k->env * 0.6f * vel[CHAT];
            if (k->env < 0.001f) k->on = 0;
        }
        k = &pad[OHAT];
        if (k->on) {
            k->env *= 0.9995f + p01(par[1]) * 0.0004f;
            k->ph += (nz - k->ph) * 0.6f;
            s += (nz - k->ph) * k->env * 0.5f * vel[OHAT];
            if (k->env < 0.001f) k->on = 0;
        }
        k = &pad[CLAP];
        if (k->on) {
            float burst;
            k->ph += 1.0f / SR;                           /* time */
            burst = k->ph < 0.03f ? (0.5f + 0.5f * fm_sinf(k->ph * 6.2831853f * 100.0f)) : 1.0f;
            k->env *= k->ph < 0.03f ? 1.0f : decay * 0.998f;
            s += nz * burst * k->env * 0.7f * vel[CLAP];
            if (k->env < 0.001f) k->on = 0;
        }
        k = &pad[TOM];
        if (k->on) {
            k->hz += (120.0f * tune - k->hz) * 0.0008f;
            k->ph += k->hz * (1.0f / SR);
            if (k->ph >= 1.0f) k->ph -= 1.0f;
            k->env *= decay;
            s += fm_sinf(k->ph * 6.2831853f) * k->env * vel[TOM] + nz * k->env * k->env * snap * 0.15f * vel[TOM];
            if (k->env < 0.001f) k->on = 0;
        }
        out[i] += fm_tanhf(s * drive) * (0.6f / fm_tanhf(drive * 0.6f));
    }
    fox_oneshot(out, n, -1);                              /* the sample pads' voices */
}

const engine_t cat = {"CAT", NAMES, par, c_init, c_note, c_render, c_panic};
