/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * OTTER SYNTH: a 4-voice polysynth. Two band-limited oscillators (PolyBLEP saw / pulse / square) or 2-op FM, detuned
 * against each other, a sub an octave down, into a driven 2-pole resonant lowpass (Chamberlin SVF, run twice a sample)
 * that tracks the keys and follows the envelope; ADSR on the amp, the same shape on the filter.
 *   page 1: K1 TONE   cutoff        K2 RES    resonance    K3 WAVE   SAW PULSE SQUARE FM    K4 DETUNE  unison spread
 *   page 2: K1 ATTACK               K2 DECAY               K3 SUSTAIN                       K4 RELEASE
 *   page 3: K1 LFO    rate          K2 VIBRATO lfo->pitch  K3 WOBBLE lfo->filter            K4 GLIDE   portamento */
#include "fastmath.h"
#include "engine.h"

static const char *const NAMES[3][NPARAM] = {{"TONE", "RES", "WAVE", "DETUNE"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"},
                                             {"LFO", "VIBRATO", "WOBBLE", "GLIDE"}};
static uint8_t par[3][NPARAM] = {{70, 35, 0, 40}, {3, 55, 70, 30}, {40, 10, 0, 0}};
static float lfo_ph, last_hz = 220.0f;
static const char *const WAVES[4] = {"SAW", "PULSE", "SQUARE", "FM"};

#define NV 4
typedef struct {
    int32_t note;
    float vel, hz, hz_target, ph1, ph2, phs, env, fenv, lp, bp;
    uint8_t stage;                        /* 0 off, 1 attack, 2 decay, 3 sustain, 4 release */
    uint32_t age;
} voice_t;
static voice_t v[NV];
static volatile int32_t pend_note[8];
static volatile uint8_t pend_vel[8], pend_on[8], pend_n;
static uint32_t clock_;

static void o_init(void) { uint32_t i; for (i = 0; i < NV; i++) { v[i].note = -1; v[i].stage = 0; v[i].env = 0; v[i].lp = v[i].bp = 0; } pend_n = 0; }
static void o_panic(void) { o_init(); }

static void o_note(int32_t note, uint32_t vel, int on)     /* queued: the render owns the voices */
{
    uint8_t n = pend_n;
    if (n >= 8u) return;
    pend_note[n] = note; pend_vel[n] = (uint8_t)vel; pend_on[n] = (uint8_t)on;
    pend_n = (uint8_t)(n + 1u);
}

static void voice_on(int32_t note, uint32_t vel)
{
    uint32_t i, pick = NV, oldest = 0;
    for (i = 0; i < NV && pick == NV; i++) if (v[i].note == note && v[i].stage && v[i].stage < 4u) pick = i;   /* retrigger */
    for (i = 0; i < NV && pick == NV; i++) if (!v[i].stage) pick = i;                                        /* free */
    for (i = 0; i < NV && pick == NV; i++) if (v[i].stage == 4u) pick = i;                                   /* releasing */
    if (pick == NV) {                                                                                       /* steal */
        for (i = 1; i < NV; i++) if (v[i].age < v[oldest].age) oldest = i;
        pick = oldest;
    }
    v[pick].note = note;
    v[pick].vel = 0.3f + 0.7f * (float)vel * (1.0f / 127.0f);
    v[pick].hz_target = note_hz(note);
    v[pick].hz = par[2][3] ? last_hz : v[pick].hz_target;        /* GLIDE: start from the last note played */
    last_hz = v[pick].hz_target;
    v[pick].stage = 1;
    v[pick].age = ++clock_;
    if (v[pick].env < 0.01f) { v[pick].ph1 = 0; v[pick].ph2 = 0.37f; v[pick].phs = 0; v[pick].env = 0; v[pick].fenv = 0; }
}

static void voice_off(int32_t note)
{
    uint32_t i;
    for (i = 0; i < NV; i++) if (v[i].note == note && v[i].stage && v[i].stage < 4u) v[i].stage = 4;
}

static void o_render(float *out, uint32_t n)
{
    uint32_t i, k;
    uint8_t wave = (uint8_t)(par[0][2] >> 5);
    float cut = p01(par[0][0]), res = p01(par[0][1]), det = p01(par[0][3]);
    float att = fm_exp2f(-5.0f - p01(par[1][0]) * 8.0f);
    float dec = fm_exp2f(-5.5f - p01(par[1][1]) * 7.5f);
    float sus = p01(par[1][2]) * p01(par[1][2]);
    float rel = fm_exp2f(-5.0f - p01(par[1][3]) * 8.0f);
    float q = 2.0f - res * 1.9f;
    float drive = 1.0f + res * 1.5f;
    float spread = fm_exp2f(det * det * 0.25f / 12.0f);
    float fmidx = det * 4.0f;
    float lfo_inc = (0.1f + p01(par[2][0]) * p01(par[2][0]) * 12.0f) / SR;
    float vib = p01(par[2][1]) * p01(par[2][1]) * 0.06f;        /* up to about a semitone */
    float wob = p01(par[2][2]);
    float glide = par[2][3] ? 0.02f * fm_exp2f(-p01(par[2][3]) * 7.0f) : 1.0f;
    float lfo = fm_sinf(lfo_ph * 6.2831853f);                    /* one LFO for all voices, per block */
    lfo_ph += lfo_inc * (float)n; if (lfo_ph >= 1.0f) lfo_ph -= 1.0f;
    while (pend_n) {
        pend_n--;
        if (pend_on[pend_n]) voice_on(pend_note[pend_n], pend_vel[pend_n]); else voice_off(pend_note[pend_n]);
    }
    for (k = 0; k < NV; k++) {
        voice_t *w = &v[k];
        float dt1, dt2, dts, fbase, tilt;
        if (!w->stage) continue;
        w->hz += (w->hz_target - w->hz) * glide * (float)n * 0.1f;   /* glide, per block */
        if (glide >= 1.0f) w->hz = w->hz_target;
        {
            float hz = w->hz * (1.0f + lfo * vib);
            dt1 = hz * spread * (1.0f / SR);
            dt2 = hz / spread * (1.0f / SR);
            dts = hz * (0.5f / SR);
        }
        fbase = w->hz * (0.5f + cut * cut * 20.0f) * (1.0f + lfo * wob * 0.8f);
        tilt = fm_exp2f((60.0f - (float)w->note) * 0.15f / 12.0f);
        if (tilt > 1.6f) tilt = 1.6f;
        for (i = 0; i < n; i++) {
            float s, f, fhz, o1, o2;
            if (w->stage == 1u) { w->env += (1.05f - w->env) * att; if (w->env >= 1.0f) { w->env = 1.0f; w->stage = 2; } }
            else if (w->stage == 2u) { w->env += (sus - w->env) * dec; if (w->env <= sus + 0.001f) w->stage = 3; }
            else if (w->stage == 4u) { w->env -= w->env * rel; if (w->env < 0.0005f) { w->stage = 0; w->env = 0; break; } }
            w->fenv += (w->env - w->fenv) * 0.02f;
            w->ph1 += dt1; if (w->ph1 >= 1.0f) w->ph1 -= 1.0f;
            w->ph2 += dt2; if (w->ph2 >= 1.0f) w->ph2 -= 1.0f;
            w->phs += dts; if (w->phs >= 1.0f) w->phs -= 1.0f;
            if (wave == 0u) {
                o1 = 2.0f * w->ph1 - 1.0f - polyblep(w->ph1, dt1);
                o2 = 2.0f * w->ph2 - 1.0f - polyblep(w->ph2, dt2);
            } else if (wave < 3u) {
                float pw = wave == 1u ? 0.25f : 0.5f, t1 = w->ph1 + pw, t2 = w->ph2 + pw;
                if (t1 >= 1.0f) t1 -= 1.0f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                o1 = (w->ph1 < pw ? 1.0f : -1.0f) + polyblep(w->ph1, dt1) - polyblep(t1, dt1);
                o2 = (w->ph2 < pw ? 1.0f : -1.0f) + polyblep(w->ph2, dt2) - polyblep(t2, dt2);
            } else {
                float m = fm_sinf(w->ph2 * 2.0f * 6.2831853f) * fmidx * (0.2f + 0.8f * w->fenv);
                o1 = fm_sinf(w->ph1 * 6.2831853f + m);
                o2 = fm_sinf(w->ph2 * 6.2831853f + m * 0.5f);
            }
            s = (o1 + o2) * 0.5f + (w->phs < 0.5f ? 0.35f : -0.35f) * (wave == 3u ? 0.4f : 1.0f);
            fhz = fbase * (0.25f + 0.75f * w->fenv);
            if (fhz > 9000.0f) fhz = 9000.0f;
            if (fhz < 30.0f) fhz = 30.0f;
            f = 2.0f * fm_sinf(3.1415926f * fhz / (2.0f * SR));
            s = fm_tanhf(s * drive);
            w->lp += f * w->bp; w->bp += f * (s - w->lp - q * w->bp);
            w->lp += f * w->bp; w->bp += f * (s - w->lp - q * w->bp);
            if (w->lp > 3.0f) w->lp = 3.0f; else if (w->lp < -3.0f) w->lp = -3.0f;
            out[i] += w->lp * w->env * w->vel * tilt * (wave == 3u ? 0.75f : 0.5f);
        }
    }
}

static const char *o_text(uint8_t page, uint32_t p, char *buf)
{
    (void)buf;
    return page == 0u && p == 2u ? WAVES[par[0][2] >> 5] : 0;
}

const engine_t otter = {"OTTER", o_init, o_note, o_render, o_panic, 3, {NAMES[0], NAMES[1], NAMES[2]}, {par[0], par[1], par[2]}, o_text};
