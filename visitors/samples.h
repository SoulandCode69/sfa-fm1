/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * The factory samples: IMA ADPCM 4-bit in the firmware image (tools/pack_samples.py -> samples_data.c), decoded as
 * they play. A voice decodes sequentially; pitch is a fractional read over the decoded stream. */
#pragma once
#include <stdint.h>

typedef struct { uint32_t off, n, rate; uint8_t root; char name[13]; } smp_t;
typedef struct { char name[9]; uint8_t first, n, is808; } smpkit_t;
extern const uint8_t SMP_DATA[];
extern const smp_t SMP[];
extern const smpkit_t SMPKIT[];
extern const uint32_t SMP_N, SMPKIT_N;

/* a decoding voice */
typedef struct {
    const uint8_t *p; uint32_t n, i;      /* the stream, its length in samples, samples decoded so far */
    int32_t pred; int8_t idx;
    float s0, s1;                         /* the last two decoded samples (-1..1) */
    float pos;                            /* the read position in decoded samples */
} adpcm_t;

extern const int16_t ADPCM_STEP[89];
extern const int8_t ADPCM_IDX[8];

static inline void adpcm_start(adpcm_t *a, const smp_t *s)
{
    a->p = SMP_DATA + s->off; a->n = s->n; a->i = 0; a->pred = 0; a->idx = 0; a->s0 = a->s1 = 0.0f; a->pos = 0.0f;
}
static inline float adpcm_step1(adpcm_t *a)          /* decode one sample */
{
    uint32_t i = a->i;
    uint8_t code;
    int32_t step, d;
    if (i >= a->n) return 0.0f;
    code = a->p[i >> 1]; code = (i & 1u) ? code >> 4 : code & 15u;
    step = ADPCM_STEP[a->idx];
    d = step >> 3;
    if (code & 4) d += step;
    if (code & 2) d += step >> 1;
    if (code & 1) d += step >> 2;
    a->pred += (code & 8) ? -d : d;
    if (a->pred > 32767) a->pred = 32767; else if (a->pred < -32768) a->pred = -32768;
    a->idx += ADPCM_IDX[code & 7];
    if (a->idx < 0) a->idx = 0; else if (a->idx > 88) a->idx = 88;
    a->i = i + 1u;
    return (float)a->pred * (1.0f / 32768.0f);
}
/* the sample at pos (fractional), decoding ahead as needed; 0 past the end; inc = pitch ratio x rate ratio */
static inline float adpcm_read(adpcm_t *a, float inc)
{
    uint32_t want = (uint32_t)a->pos + 2u, k;
    float f, out;
    if ((uint32_t)a->pos >= a->n) return 0.0f;
    for (k = a->i; k < want && k < a->n; k++) { a->s0 = a->s1; a->s1 = adpcm_step1(a); }
    f = a->pos - (float)(uint32_t)a->pos;
    out = a->s0 + (a->s1 - a->s0) * f;
    a->pos += inc;
    return out;
}
static inline int adpcm_done(const adpcm_t *a) { return (uint32_t)a->pos >= a->n; }
