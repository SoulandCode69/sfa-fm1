/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * OTTER SYNTH: one voice, 2-operator FM into a one-pole lowpass, attack/decay. Last note wins, legato glide.
 *   K1 TONE    lowpass cutoff           K3 RATIO   modulator ratio (0.5 .. 8)
 *   K2 DECAY   envelope decay           K4 FM      modulator depth */
#include "fastmath.h"
#include "engine.h"

static const char *const NAMES[NPARAM] = {"TONE", "DECAY", "RATIO", "FM"};
static uint8_t par[NPARAM] = {90, 60, 32, 40};

static volatile int32_t cur_note = -1;
static volatile float cur_vel;
static float ph_c, ph_m, env, hz_now, lp, gate;
static uint8_t retrig;

static void o_init(void) { cur_note = -1; env = 0; lp = 0; gate = 0; }
static void o_panic(void) { cur_note = -1; env = 0; gate = 0; }

static void o_note(int32_t note, uint32_t vel, int on)
{
    if (on) {
        cur_note = note;
        cur_vel = (float)vel * (1.0f / 127.0f);
        retrig = 1;
    } else if (cur_note == note) {
        cur_note = -1;
    }
}

static const float RATIOS[8] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};

static void o_render(float *out, uint32_t n)
{
    uint32_t i;
    int32_t note = cur_note;
    float hz_target = note >= 0 ? note_hz(note) : hz_now;
    float ratio = RATIOS[par[2] >> 4];
    float depth = p01(par[3]) * p01(par[3]) * 6.0f;
    float dec = 1.0f - fm_exp2f(-4.0f - p01(par[1]) * 9.0f);   /* per-sample decay toward 0 */
    float cut = 0.02f + p01(par[0]) * p01(par[0]) * 0.8f;        /* one-pole coefficient */
    float g_target = note >= 0 ? 1.0f : 0.0f;
    if (retrig) {
        retrig = 0;
        env = 1.0f;
        if (gate < 0.01f)
            hz_now = hz_target;              /* a new phrase starts on pitch; a legato note glides */
    }
    for (i = 0; i < n; i++) {
        float m, c, s;
        hz_now += (hz_target - hz_now) * 0.004f;
        gate += (g_target - gate) * 0.003f;                    /* ~8 ms release */
        env += (0.0f - env) * (1.0f - dec);
        ph_m += hz_now * ratio * (1.0f / SR);
        ph_c += hz_now * (1.0f / SR);
        if (ph_m >= 1.0f) ph_m -= 1.0f;
        if (ph_c >= 1.0f) ph_c -= 1.0f;
        m = fm_sinf(ph_m * 6.2831853f) * depth * env;
        c = fm_sinf(ph_c * 6.2831853f + m);
        lp += (c - lp) * cut;
        s = lp * (0.25f + 0.75f * env) * gate * cur_vel * 1.1f;
        out[i] += s;
    }
}

const engine_t otter = {"OTTER", NAMES, par, o_init, o_note, o_render, o_panic};
