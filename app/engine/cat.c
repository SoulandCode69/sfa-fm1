/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * CAT KIT: 16 pads on the 16 white keys.
 *   pads 1-8   synthesised, in the 808 / 909 manner:
 *              KICK   sine with an exponential pitch drop, a click, soft drive
 *              SNARE  two detuned tones + bandpassed noise with its own decay
 *              CHAT / OHAT   six detuned square oscillators through a highpass (the 808's metal), two decays
 *              CLAP   four noise bursts 10 ms apart then a tail, bandpassed
 *              TOM-LO / TOM-HI   sine with a pitch drop and a little noise
 *              RIM    a short 909-style ring (two clicks through a resonant band)
 *   pads 9-16  the Fox sample bank in 8 equal slices (load a drum loop with tools/sfa_sample.py)
 *   page 1: K1 TUNE  kick / tom / sample pitch   K2 DECAY  kick & toms   K3 SNAP  snare & clap noise   K4 DRIVE
 *   page 2: K1 HATS  hat decay                   K2 SNARE  snare decay   K3 TONE  hat / snare colour    K4 LEVEL  samples */
#include "fastmath.h"
#include "engine.h"

void fox_slice(int slot, float rate, float vel);
void fox_slices_render(float *out, uint32_t n);
void fox_slices_panic(void);

static const char *const NAMES[2][NPARAM] = {{"TUNE", "DECAY", "SNAP", "DRIVE"}, {"HATS", "SNARE", "TONE", "SMPLVL"}};
static uint8_t par[2][NPARAM] = {{55, 60, 60, 35}, {45, 55, 64, 100}};

enum { KICK, SNARE, CHAT, OHAT, CLAP, TOMLO, TOMHI, RIM, NSYN, NPADS = 16 };
typedef struct { float env, env2, ph, ph2, hz, t, bp, lp, hp; float vel; uint8_t on; } pad_t;
static pad_t pad[NSYN];
static float metal_ph[6];
static uint32_t rng = 0x5F3759DFu;
static volatile uint8_t trig[NPADS], trig_vel[NPADS];
static float hp_x, hp_y;

static void c_init(void) { uint32_t i; for (i = 0; i < NSYN; i++) pad[i].on = 0; fox_slices_panic(); }
static void c_panic(void) { c_init(); }

static void c_note(int32_t note, uint32_t v, int on)
{
    if (!on || note < 0 || note >= NPADS) return;
    trig_vel[note] = (uint8_t)v;
    trig[note] = 1;
}

/* a resonant bandpass, one step: state in k->bp / k->lp */
static inline float bandpass(pad_t *k, float x, float f, float q)
{
    k->lp += f * k->bp;
    k->bp += f * (x - k->lp - q * k->bp);
    return k->bp;
}

static void c_render(float *out, uint32_t n)
{
    uint32_t i, p;
    float tune = fm_exp2f((p01(par[0][0]) - 0.5f) * 1.6f);               /* +- 0.8 octave */
    float kdec = 1.0f - fm_exp2f(-6.5f - p01(par[0][1]) * 5.0f);
    float snap = p01(par[0][2]);
    float drive = 1.0f + p01(par[0][3]) * 5.0f;
    float norm = 0.7f / fm_tanhf(drive * 0.7f);
    float hdec = 1.0f - fm_exp2f(-6.0f - p01(par[1][0]) * 6.0f);
    float sdec = 1.0f - fm_exp2f(-6.0f - p01(par[1][1]) * 5.0f);
    float tone = p01(par[1][2]);
    float smpl = p01(par[1][3]) * 1.5f;
    for (p = 0; p < NPADS; p++)
        if (trig[p]) {
            float vel = (float)trig_vel[p] * (1.0f / 127.0f);
            pad_t *k;
            trig[p] = 0;
            if (p >= NSYN) { fox_slice((int)(p - NSYN), tune, vel * smpl); continue; }
            k = &pad[p];
            k->on = 1; k->env = 1.0f; k->env2 = 1.0f; k->ph = 0; k->ph2 = 0; k->t = 0; k->vel = vel;
            k->hz = p == KICK ? 220.0f * tune : p == TOMLO ? 260.0f * tune : p == TOMHI ? 400.0f * tune : 0.0f;
            if (p == CHAT) pad[OHAT].on = 0;                                 /* a closed hat chokes the open one */
        }
    for (i = 0; i < n; i++) {
        float s = 0.0f, nz = noise(&rng), metal = 0.0f, y;
        pad_t *k;
        uint32_t m;
        /* the hats' metal: six squares at 808-ish ratios, made once a sample */
        if (pad[CHAT].on || pad[OHAT].on) {
            static const float HZ[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
            for (m = 0; m < 6u; m++) {
                metal_ph[m] += HZ[m] * (1.2f + tone * 1.2f) * (1.0f / SR);
                if (metal_ph[m] >= 1.0f) metal_ph[m] -= 1.0f;
                metal += metal_ph[m] < 0.5f ? 1.0f : -1.0f;
            }
            metal *= 1.0f / 6.0f;
            y = metal - hp_x + 0.9f * hp_y;                                 /* highpass the metal */
            hp_x = metal; hp_y = y;
            metal = y + nz * 0.25f;
        }
        if ((k = &pad[KICK])->on) {
            float body;
            k->hz += (46.0f * tune - k->hz) * 0.0035f;                      /* exponential-ish pitch drop */
            k->ph += k->hz * (1.0f / SR); if (k->ph >= 1.0f) k->ph -= 1.0f;
            k->env *= kdec;
            k->env2 *= 0.9985f;                                              /* the click */
            body = fm_sinf(k->ph * 6.2831853f);
            s += (fm_tanhf(body * 2.2f) * k->env * 1.2f + nz * k->env2 * k->env2 * 0.6f * snap) * k->vel;
            if (k->env < 0.0005f) k->on = 0;
        }
        if ((k = &pad[SNARE])->on) {
            float tn, ns;
            k->ph += 180.0f * tune * (1.0f / SR); if (k->ph >= 1.0f) k->ph -= 1.0f;
            k->ph2 += 330.0f * tune * (1.0f / SR); if (k->ph2 >= 1.0f) k->ph2 -= 1.0f;
            k->env *= sdec * 0.998f;                                         /* the tones go first */
            k->env2 *= sdec;                                                 /* the noise rings */
            tn = (fm_sinf(k->ph * 6.2831853f) + fm_sinf(k->ph2 * 6.2831853f) * 0.6f) * k->env;
            ns = bandpass(k, nz, 0.25f + tone * 0.3f, 0.9f) * k->env2 * 2.0f;
            s += (tn * 0.6f + ns * (0.4f + snap * 0.8f)) * k->vel;
            if (k->env2 < 0.0005f) k->on = 0;
        }
        if ((k = &pad[CHAT])->on) {
            k->env *= hdec * 0.997f;
            s += metal * k->env * 0.7f * k->vel;
            if (k->env < 0.0005f) k->on = 0;
        }
        if ((k = &pad[OHAT])->on) {
            k->env *= 0.99965f + p01(par[1][0]) * 0.0003f;
            s += metal * k->env * 0.6f * k->vel;
            if (k->env < 0.0005f) k->on = 0;
        }
        if ((k = &pad[CLAP])->on) {
            float burst, ns;
            k->t += 1.0f / SR;
            if (k->t < 0.03f) burst = k->t - (float)(int)(k->t * 100.0f) * 0.01f < 0.004f ? 1.0f : 0.15f;   /* 3 bursts */
            else { burst = 1.0f; k->env *= sdec * 0.998f; }
            ns = bandpass(k, nz, 0.2f + tone * 0.25f, 0.7f) * 2.2f;
            s += ns * burst * k->env * (0.6f + snap * 0.5f) * k->vel;
            if (k->env < 0.0005f) k->on = 0;
        }
        if ((k = &pad[TOMLO])->on) {
            k->hz += (95.0f * tune - k->hz) * 0.0012f;
            k->ph += k->hz * (1.0f / SR); if (k->ph >= 1.0f) k->ph -= 1.0f;
            k->env *= kdec * 1.0005f;
            s += (fm_sinf(k->ph * 6.2831853f) * k->env + nz * k->env * k->env * 0.08f) * k->vel;
            if (k->env < 0.0005f) k->on = 0;
        }
        if ((k = &pad[TOMHI])->on) {
            k->hz += (150.0f * tune - k->hz) * 0.0012f;
            k->ph += k->hz * (1.0f / SR); if (k->ph >= 1.0f) k->ph -= 1.0f;
            k->env *= kdec * 1.0005f;
            s += (fm_sinf(k->ph * 6.2831853f) * k->env + nz * k->env * k->env * 0.08f) * k->vel;
            if (k->env < 0.0005f) k->on = 0;
        }
        if ((k = &pad[RIM])->on) {
            float x;
            k->t += 1.0f / SR;
            x = k->t < 0.002f ? 1.0f : k->t > 0.004f && k->t < 0.006f ? -0.7f : 0.0f;    /* two clicks */
            k->env *= 0.9975f;
            s += (bandpass(k, x, 0.14f, 0.08f) * 1.5f + x * 0.2f) * k->env * 0.8f * k->vel;
            if (k->env < 0.0005f) k->on = 0;
        }
        out[i] += fm_tanhf(s * drive) * norm;
    }
    fox_slices_render(out, n);
}

const engine_t cat = {"CAT", NAMES[0], par[0], c_init, c_note, c_render, c_panic, 2, NAMES[1], par[1], 0};
