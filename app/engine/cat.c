/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * CAT KIT: 16 pads on the 16 white keys.
 *   pads 1-8   synthesised: KICK SNARE CHAT OHAT CLAP TOM-LO TOM-HI RIM
 *   pads 9-16  sample drums: the Fox bank cut into 8 equal slices (load a drum loop or a row of hits with
 *              tools/sfa_sample.py and every slice is a pad); pad 9 = slice 1 ... pad 16 = slice 8
 *   K1 TUNE    kick / tom / sample pitch         K3 SNAP    attack click and noise
 *   K2 DECAY   every pad's decay                 K4 DRIVE   soft clip */
#include "fastmath.h"
#include "engine.h"

void fox_slice(int slot, float rate, float vel);          /* a sample pad: start slice slot of 8 */
void fox_slices_render(float *out, uint32_t n);
void fox_slices_panic(void);

static const char *const NAMES[NPARAM] = {"TUNE", "DECAY", "SNAP", "DRIVE"};
static uint8_t par[NPARAM] = {50, 64, 70, 30};

enum { KICK, SNARE, CHAT, OHAT, CLAP, TOMLO, TOMHI, RIM, NSYN, NPADS = 16 };
typedef struct { float env, ph, hz, nenv, hp; uint8_t on; float vel; } pad_t;
static pad_t pad[NSYN];
static uint32_t rng = 0x5F3759DFu;
static volatile uint8_t trig[NPADS], trig_vel[NPADS];

static void c_init(void) { uint32_t i; for (i = 0; i < NSYN; i++) pad[i].on = 0; fox_slices_panic(); }
static void c_panic(void) { c_init(); }

static void c_note(int32_t note, uint32_t v, int on)
{
    if (!on || note < 0 || note >= NPADS) return;
    trig_vel[note] = (uint8_t)v;
    trig[note] = 1;
}

static inline float tone(pad_t *k, float target_hz, float sweep, float dec)
{
    k->hz += (target_hz - k->hz) * sweep;
    k->ph += k->hz * (1.0f / SR);
    if (k->ph >= 1.0f) k->ph -= 1.0f;
    k->env *= dec;
    return fm_sinf(k->ph * 6.2831853f) * k->env;
}

static void c_render(float *out, uint32_t n)
{
    uint32_t i, p;
    float decay = 1.0f - fm_exp2f(-5.0f - p01(par[1]) * 7.0f);
    float tune = 0.6f + p01(par[0]) * 1.2f;
    float snap = p01(par[2]);
    float drive = 1.0f + p01(par[3]) * 6.0f;
    float norm = 0.6f / fm_tanhf(drive * 0.6f);
    for (p = 0; p < NPADS; p++)
        if (trig[p]) {
            float vel = (float)trig_vel[p] * (1.0f / 127.0f);
            trig[p] = 0;
            if (p >= NSYN) {
                fox_slice((int)(p - NSYN), fm_exp2f((p01(par[0]) - 0.5f) * 2.0f), vel);
                continue;
            }
            pad[p].on = 1; pad[p].env = 1.0f; pad[p].nenv = 1.0f; pad[p].ph = 0; pad[p].vel = vel;
            pad[p].hz = p == KICK ? 180.0f * tune : p == TOMLO ? 200.0f * tune : p == TOMHI ? 320.0f * tune
                      : p == RIM ? 900.0f : 180.0f;
        }
    for (i = 0; i < n; i++) {
        float s = 0.0f, nz = noise(&rng);
        pad_t *k;
        if ((k = &pad[KICK])->on) {
            s += tone(k, 48.0f * tune, 0.002f, decay) * k->vel * 1.3f + nz * k->env * k->env * k->env * snap * 0.4f * k->vel;
            if (k->env < 0.001f) k->on = 0;
        }
        if ((k = &pad[SNARE])->on) {
            k->nenv *= decay * 0.999f;
            s += (tone(k, 170.0f, 0.001f, decay * 0.998f) * 0.5f + nz * k->nenv * (0.35f + snap * 0.6f)) * k->vel;
            if (k->nenv < 0.001f) k->on = 0;
        }
        if ((k = &pad[CHAT])->on) {
            k->env *= decay * decay * 0.994f;
            k->hp += (nz - k->hp) * 0.5f;
            s += (nz - k->hp) * k->env * 0.55f * k->vel;
            if (k->env < 0.001f) k->on = 0;
        }
        if ((k = &pad[OHAT])->on) {
            k->env *= 0.9994f + p01(par[1]) * 0.0005f;
            k->hp += (nz - k->hp) * 0.5f;
            s += (nz - k->hp) * k->env * 0.45f * k->vel;
            if (k->env < 0.001f) k->on = 0;
        }
        if ((k = &pad[CLAP])->on) {
            float burst;
            k->ph += 1.0f / SR;                                   /* time */
            burst = k->ph < 0.03f ? 0.5f + 0.5f * fm_sinf(k->ph * 6.2831853f * 100.0f) : 1.0f;
            k->env *= k->ph < 0.03f ? 1.0f : decay * 0.998f;
            k->hp += (nz - k->hp) * 0.3f;
            s += (nz - k->hp) * burst * k->env * 0.8f * k->vel;
            if (k->env < 0.001f) k->on = 0;
        }
        if ((k = &pad[TOMLO])->on) {
            s += tone(k, 110.0f * tune, 0.0008f, decay) * k->vel + nz * k->env * k->env * snap * 0.1f * k->vel;
            if (k->env < 0.001f) k->on = 0;
        }
        if ((k = &pad[TOMHI])->on) {
            s += tone(k, 180.0f * tune, 0.0008f, decay) * k->vel + nz * k->env * k->env * snap * 0.1f * k->vel;
            if (k->env < 0.001f) k->on = 0;
        }
        if ((k = &pad[RIM])->on) {
            s += tone(k, 900.0f, 0.0f, decay * decay * 0.985f) * 0.6f * k->vel + nz * k->env * k->env * snap * 0.25f * k->vel;
            if (k->env < 0.001f) k->on = 0;
        }
        out[i] += fm_tanhf(s * drive) * norm;
    }
    fox_slices_render(out, n);
}

const engine_t cat = {"CAT", NAMES, par, c_init, c_note, c_render, c_panic, 1, 0, 0};
