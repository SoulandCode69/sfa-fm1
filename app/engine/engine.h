/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * The engine contract. Every animal implements this and nothing more; the app owns the keys, the pages, the mix. */
#pragma once
#include <stdint.h>

#define SR 44100.0f
#define NPARAM 4                          /* one page: K1..K4 */

typedef struct engine engine_t;
struct engine {
    const char *name;                     /* "OTTER" */
    const char *const *param_name;        /* NPARAM short labels */
    uint8_t *param;                       /* NPARAM values 0..127, live in the app's settings */
    void (*init)(void);
    void (*note)(int32_t note, uint32_t vel, int on);   /* main loop or audio irq; must be cheap */
    void (*render)(float *mono, uint32_t n);            /* add n frames into mono (nominal full scale 1.0) */
    void (*panic)(void);
};

/* a tiny shared helper: MIDI note -> phase increment per sample */
static inline float note_hz(int32_t note) { return 440.0f * fm_exp2f((float)(note - 69) * (1.0f / 12.0f)); }
static inline float p01(uint8_t p) { return (float)p * (1.0f / 127.0f); }

/* a cheap deterministic noise */
static inline float noise(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return (float)(int32_t)*s * (1.0f / 2147483648.0f);
}
