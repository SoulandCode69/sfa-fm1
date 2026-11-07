/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * OTTER SYNTH: one voice. 2-op FM (sine carrier, sine modulator) + a sub oscillator, into a 2-pole resonant
 * lowpass that tracks the keyboard, AD envelope on the filter and FM, a gate on the level. Last note wins, legato glide.
 *   page 1: K1 TONE   cutoff        K2 DECAY  envelope      K3 RATIO  mod ratio     K4 FM     mod depth
 *   page 2: K1 RES    resonance     K2 ATTACK envelope      K3 SUB    sub osc mix   K4 GLIDE  portamento */
#include "fastmath.h"
#include "engine.h"

static const char *const NAMES[2][NPARAM] = {{"TONE", "DECAY", "RATIO", "FM"}, {"RES", "ATTACK", "SUB", "GLIDE"}};
static uint8_t par[2][NPARAM] = {{80, 60, 32, 45}, {30, 4, 40, 10}};

static volatile int32_t cur_note = -1;
static volatile float cur_vel;
static float ph_c, ph_m, ph_s, env, hz_now, lpf, bpf, gate, keytilt;
static uint8_t retrig, stage;            /* stage 1 = attack */

static void o_init(void) { cur_note = -1; env = 0; lpf = bpf = 0; gate = 0; }
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
    float ratio = RATIOS[par[0][2] >> 4];
    float depth = p01(par[0][3]) * p01(par[0][3]) * 5.0f;
    float dec = fm_exp2f(-4.0f - p01(par[0][1]) * 9.0f);       /* per-sample decay rate */
    float att = fm_exp2f(-3.0f - p01(par[1][1]) * 9.0f);       /* per-sample attack rate */
    float res = p01(par[1][0]);
    float q = 1.8f - res * 1.7f;                                /* damping: 1.8 (none) .. 0.1 (ringing) */
    float sub = p01(par[1][2]) * 0.8f;
    float glide = 0.03f * fm_exp2f(-p01(par[1][3]) * 8.0f);    /* 0.03 .. 0.0001 per sample */
    float g_target = note >= 0 ? 1.0f : 0.0f;
    if (retrig) {
        retrig = 0;
        stage = 1;
        if (gate < 0.01f) {
            hz_now = hz_target;          /* a new phrase starts on pitch; a legato note glides */
            env = 0.0f;
        }
    }
    for (i = 0; i < n; i++) {
        float m, c, s, f, fhz;
        hz_now += (hz_target - hz_now) * glide;
        gate += (g_target - gate) * 0.003f;                      /* ~8 ms release */
        if (stage) {
            env += (1.0f - env) * att;
            if (env > 0.995f) stage = 0;
        } else {
            env -= env * dec;
        }
        ph_m += hz_now * ratio * (1.0f / SR);
        ph_c += hz_now * (1.0f / SR);
        ph_s += hz_now * (0.5f / SR);
        if (ph_m >= 1.0f) ph_m -= 1.0f;
        if (ph_c >= 1.0f) ph_c -= 1.0f;
        if (ph_s >= 1.0f) ph_s -= 1.0f;
        m = fm_sinf(ph_m * 6.2831853f) * depth * (0.3f + 0.7f * env) * (0.5f + 0.5f * cur_vel);
        c = fm_sinf(ph_c * 6.2831853f + m);
        s = c + (ph_s < 0.5f ? sub : -sub);                      /* square sub an octave down */
        /* the filter: cutoff from TONE, tracking the note, opened by the envelope; Chamberlin SVF */
        fhz = hz_now * (0.5f + p01(par[0][0]) * p01(par[0][0]) * 24.0f) * (0.3f + 0.7f * env);
        if (fhz > 9000.0f) fhz = 9000.0f;
        f = 2.0f * fm_sinf(3.1415926f * fhz / (2.0f * SR));      /* the filter runs twice a sample: half-rate f, stable */
        lpf += f * bpf;
        bpf += f * (s - lpf - q * bpf);
        lpf += f * bpf;                                          /* run twice: steadier at high cutoff */
        bpf += f * (s - lpf - q * bpf);
        if (lpf > 4.0f) lpf = 4.0f; else if (lpf < -4.0f) lpf = -4.0f;
        out[i] += lpf * gate * cur_vel * 0.45f * keytilt;
    }
    /* level tilt: about 1.2 dB per octave, down towards the top, up towards the bottom */
    keytilt = note >= 0 ? fm_exp2f((60.0f - (float)note) * 0.2f / 12.0f) : keytilt;
    if (keytilt > 2.0f) keytilt = 2.0f;
    if (keytilt < 0.35f) keytilt = 0.35f;
}

const engine_t otter = {"OTTER", NAMES[0], par[0], o_init, o_note, o_render, o_panic, 2, NAMES[1], par[1]};
