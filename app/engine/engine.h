/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * The engine contract. Every animal implements this and nothing more; the app owns the keys, the pages, the mix. */
#pragma once
#include <stdint.h>

#define SR 44100.0f
#define NPARAM 4                          /* one page: K1..K4 */

#define NPAGES 5
typedef struct engine engine_t;
struct engine {
    const char *name;                     /* "OTTER" */
    void (*init)(void);
    void (*note)(int32_t note, uint32_t vel, int on);   /* main loop or audio irq; must be cheap */
    void (*render)(float *mono, uint32_t n);            /* add n frames into mono (nominal full scale 1.0) */
    void (*panic)(void);
    uint8_t npages;                       /* 1..NPAGES */
    const char *const *names[NPAGES];     /* each page's NPARAM short labels */
    uint8_t *params[NPAGES];              /* each page's NPARAM values 0..127 (the app saves them) */
    const char *(*param_text)(uint8_t page, uint32_t p, char *buf);   /* a value shown as words, or 0 */
    const char *const *titles;            /* each page's title (or 0) */
};

/* a trapezoidal state-variable filter (Simper): stable at any cutoff and resonance. Coefficients per block,
 * svf_run per sample; lp / bp / hp outputs. res 0..1 (1 = self-oscillation's edge). */
typedef struct { float ic1, ic2; } svf_t;
typedef struct { float a1, a2, a3, k; } svfc_t;
static inline void svf_coef(svfc_t *c, float hz, float res)
{
    float g, s, co;
    if (hz > 18000.0f) hz = 18000.0f;
    if (hz < 10.0f) hz = 10.0f;
    s = fm_sinf(3.1415926f * hz / SR); co = fm_cosf(3.1415926f * hz / SR);
    g = s / (co < 0.05f ? 0.05f : co);
    c->k = 2.0f - 1.96f * res;
    c->a1 = 1.0f / (1.0f + g * (g + c->k));
    c->a2 = g * c->a1;
    c->a3 = g * c->a2;
}
static inline float svf_lp(svf_t *f, const svfc_t *c, float in)
{
    float v3 = in - f->ic2, v1 = c->a1 * f->ic1 + c->a2 * v3, v2 = f->ic2 + c->a2 * f->ic1 + c->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    return v2;
}
static inline float svf_bp(svf_t *f, const svfc_t *c, float in)
{
    float v3 = in - f->ic2, v1 = c->a1 * f->ic1 + c->a2 * v3, v2 = f->ic2 + c->a2 * f->ic1 + c->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    return v1;
}

/* a cheap soft clip (a Padé tanh: within 2% of tanh, no exp): for every per-sample saturation */
static inline float fm_soft(float x)
{
    float x2;
    if (x > 3.0f) x = 3.0f; else if (x < -3.0f) x = -3.0f;
    x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/* seconds -> a per-sample multiplier that decays to 1/e in that time */
static inline float decay_coef(float sec) { return fm_exp2f(-1.4427f / (sec * SR)); }

/* PolyBLEP: the step of a band-limited saw / pulse (t = phase 0..1, dt = phase per sample) */
static inline float polyblep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

/* a tiny shared helper: MIDI note -> phase increment per sample */
static inline float note_hz(int32_t note) { return 440.0f * fm_exp2f((float)(note - 69) * (1.0f / 12.0f)); }
static inline float p01(uint8_t p) { return (float)p * (1.0f / 127.0f); }

/* a cheap deterministic noise */
static inline float noise(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return (float)(int32_t)*s * (1.0f / 2147483648.0f);
}
