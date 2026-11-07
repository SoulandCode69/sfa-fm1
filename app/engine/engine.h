/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * The engine contract. Every animal implements this and nothing more; the app owns the keys, the pages, the mix. */
#pragma once
#include <stdint.h>

#define SR 44100.0f
#define NPARAM 4                          /* one page: K1..K4 */

#define NPAGES 3
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
};

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
