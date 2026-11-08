/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * CAT KIT: a sample drum machine. 16 pads on the 16 white keys, one factory kit at a time (ALGO browses them),
 * every pad its own PITCH / DECAY / FILTER / LEVEL (page 1 edits the pad you hit last). Clean gain staging:
 * the pads sum with headroom, a gentle bus compressor and a soft ceiling on the kit, no distortion by default.
 *   page 1 (the pad):  PITCH DECAY FILTER LEVEL        page 2 (the kit): KIT TUNE DECAY LEVEL
 *   page 3 (the bus):  COMP PUNCH LOWCUT DRIVE */
#include "fastmath.h"
#include "engine.h"
#include "../samples.h"

static const char *const NAMES1[NPARAM] = {"PITCH", "DECAY", "FILTER", "LEVEL"};
static const char *const NAMES2[NPARAM] = {"KIT", "TUNE", "DECAY", "LEVEL"};
static const char *const NAMES3[NPARAM] = {"COMP", "PUNCH", "LOWCUT", "DRIVE"};
static uint8_t par1[NPARAM] = {64, 127, 127, 100};      /* a view of the selected pad's params */
static uint8_t par2[NPARAM] = {0, 64, 64, 100};
static uint8_t par3[NPARAM] = {35, 20, 0, 0};
#define NPADS 16
#define MAXKITS 4
static uint8_t kit_i;                                   /* index into the drum kits (not the 808 bank) */
static uint8_t pad_par[MAXKITS][NPADS][NPARAM];         /* PITCH DECAY FILTER LEVEL per pad per kit (saved by the app) */
static volatile int8_t last_pad = 0;
static uint8_t shown_pad = 0;

/* the drum kits among the sample kits */
static uint32_t kit_count(void) { uint32_t i, n = 0; for (i = 0; i < SMPKIT_N; i++) if (!SMPKIT[i].is808) n++; return n; }
static const smpkit_t *kit_at(uint32_t k) { uint32_t i, n = 0; for (i = 0; i < SMPKIT_N; i++) if (!SMPKIT[i].is808) { if (n == k) return &SMPKIT[i]; n++; } return &SMPKIT[0]; }
static const smp_t *pad_smp(uint32_t pad) { const smpkit_t *k = kit_at(kit_i); return pad < k->n ? &SMP[k->first + pad] : 0; }

uint8_t *cat_pad_blob(uint32_t *size) { *size = sizeof pad_par; return &pad_par[0][0][0]; }
const char *cat_pad_name(int pad) { const smp_t *s = pad >= 0 ? pad_smp((uint32_t)pad) : 0; return s ? s->name : "-"; }
const char *cat_kit_name(void) { return kit_at(kit_i)->name; }
int cat_last_pad(void) { return last_pad; }
void cat_kit(int32_t step) { uint32_t n = kit_count(); kit_i = (uint8_t)(((int32_t)kit_i + step + (int32_t)n) % (int32_t)n); }
int cat_kit_index(void) { return kit_i; }
void cat_kit_set(int k) { uint32_t n = kit_count(); kit_i = (uint8_t)(k % (int)n); }
void cat_pads_default(void)
{
    /* kick kick2 snare rim clap chat ohat cym ltom mtom htom perc perc perc fx fx: hats and cymbals sit under the kicks */
    static const uint8_t LVL[NPADS] = {105, 105, 100, 85, 95, 72, 66, 62, 92, 92, 92, 80, 82, 70, 88, 100};
    uint32_t k, p;
    for (k = 0; k < MAXKITS; k++) for (p = 0; p < NPADS; p++) { pad_par[k][p][0] = 64; pad_par[k][p][1] = 127; pad_par[k][p][2] = 127; pad_par[k][p][3] = LVL[p]; }
}
static void pad_view_load(void) { uint32_t i; for (i = 0; i < NPARAM; i++) par1[i] = pad_par[kit_i][shown_pad][i]; }
static void pad_view_store(void) { uint32_t i; for (i = 0; i < NPARAM; i++) pad_par[kit_i][shown_pad][i] = par1[i]; }
void cat_sync(void)                                     /* main loop, every frame: the page-1 knobs are the pad's */
{
    if ((uint8_t)last_pad != shown_pad) { shown_pad = (uint8_t)last_pad; pad_view_load(); }
    else pad_view_store();
}

typedef struct { adpcm_t a; float env, dcoef, gain, inc; svf_t flt; svfc_t fc; uint8_t on, filt; } voice_t;
static voice_t v[NPADS];
static volatile uint8_t trig[NPADS], trig_vel[NPADS];
static float comp_env, comp_g = 1.0f, hp_x, hp_y;

static void c_init(void) { uint32_t i; for (i = 0; i < NPADS; i++) v[i].on = 0; comp_env = 0; pad_view_load(); }
static void c_panic(void) { c_init(); }

static void c_note(int32_t note, uint32_t vl, int on)
{
    if (!on || note < 0 || note >= NPADS) return;
    trig_vel[note] = (uint8_t)vl;
    trig[note] = 1;
    last_pad = (int8_t)note;
}

static void start(uint32_t p, float vel)
{
    const smp_t *s = pad_smp(p);
    voice_t *w = &v[p];
    const uint8_t *pp = pad_par[kit_i][p];
    float semis = ((float)pp[0] - 64.0f) * (24.0f / 127.0f) + ((float)par2[1] - 64.0f) * (12.0f / 127.0f);
    float dec = p01(pp[1]) * p01(par2[2]) * 2.0f, filt = p01(pp[2]);
    if (!s) return;
    if (p == 6u && v[5].on) { }                                      /* (an open hat does not cut the closed one) */
    if (p == 5u) v[6].on = 0;                                        /* the closed hat chokes the open one */
    adpcm_start(&w->a, s);
    w->inc = fm_exp2f(semis * (1.0f / 12.0f)) * ((float)s->rate / SR);
    w->env = 1.0f;
    w->dcoef = dec >= 1.99f ? 1.0f : decay_coef(0.01f + dec * dec * 0.6f);   /* DECAY 127 = the whole sample */
    w->gain = (0.2f + 0.8f * vel * vel) * p01(pp[3]) * 1.27f;          /* a velocity curve, the pad's level */
    w->filt = filt < 0.99f;
    if (w->filt) { svf_coef(&w->fc, 100.0f * fm_exp2f(filt * 7.2f), 0.1f); w->flt.ic1 = w->flt.ic2 = 0; }   /* 100 Hz .. 15 kHz */
    w->on = 1;
}

static void c_render(float *out, uint32_t n)
{
    uint32_t i, p;
    float comp = p01(par3[0]), punch = p01(par3[1]) * 1.5f, lowcut = p01(par3[2]), drive = p01(par3[3]);
    float level = p01(par2[3]) * 1.1f;
    float hpc = lowcut > 0.0f ? 1.0f - fm_exp2f(-6.2831853f * (20.0f + lowcut * 200.0f) / SR) : 0.0f;
    float thr = 1.0f - comp * 0.7f, slope = comp * 0.75f, makeup = 1.0f + comp * 0.6f;
    float dgain = 1.0f + drive * drive * 4.0f, dnorm = drive > 0.0f ? 1.0f / fm_soft(dgain * 0.7f) : 1.0f;
    for (p = 0; p < NPADS; p++)
        if (trig[p]) { trig[p] = 0; start(p, (float)trig_vel[p] * (1.0f / 127.0f)); }
    for (i = 0; i < n; i++) {
        float s = 0.0f, g;
        for (p = 0; p < NPADS; p++) {
            voice_t *w = &v[p];
            float x;
            if (!w->on) continue;
            x = adpcm_read(&w->a, w->inc) * w->env * w->gain;
            if (w->filt) x = svf_lp(&w->flt, &w->fc, x);
            if (punch > 0.0f && w->a.pos < 900.0f) x *= 1.0f + punch * (1.0f - w->a.pos * (1.0f / 900.0f));   /* ~40 ms transient lift */
            w->env *= w->dcoef;
            s += x;
            if (adpcm_done(&w->a) || w->env < 0.001f) w->on = 0;
        }
        if (hpc > 0.0f) { float y = s - hp_x + (1.0f - hpc) * hp_y; hp_x = s; hp_y = y; s = y; }   /* LOWCUT: a highpass */
        /* bus compressor: peak follower, fast attack, 120 ms release, soft knee */
        {
            float a = fm_fabsf(s);
            comp_env += (a > comp_env ? 0.4f : 0.0002f) * (a - comp_env);
            if ((i & 7u) == 0u) comp_g = comp_env > thr ? fm_powf(comp_env / thr, -slope) : 1.0f;   /* the gain every 8 samples */
            g = comp_g;
            s *= g * makeup;
        }
        if (drive > 0.0f) s = fm_soft(s * 0.7f * dgain) * dnorm;
        s *= level;
        if (s > 1.5f) s = 1.5f - fm_tanhf(1.5f - s) * 0.0f; else if (s < -1.5f) s = -1.5f;   /* the ceiling (the master limits) */
        out[i] += s;
    }
}

static const char *c_text(uint8_t page, uint32_t p, char *buf)
{
    if (page == 1u && p == 0u) return cat_kit_name();
    if (page == 0u && p == 0u) { int32_t st = ((int32_t)par1[0] - 64) * 24 / 127; buf[0] = st < 0 ? '-' : '+'; st = st < 0 ? -st : st; buf[1] = (char)('0' + st / 10); buf[2] = (char)('0' + st % 10); buf[3] = 0; return buf; }
    if (page == 0u && p == 1u && par1[1] >= 127u) return "FULL";
    if (page == 0u && p == 2u && par1[2] >= 126u) return "OPEN";
    if (page == 2u && (p == 2u || p == 3u) && (p == 2u ? par3[2] : par3[3]) == 0u) return "OFF";
    return 0;
}

static const char *const TITLES[3] = {"PAD", "KIT", "BUS"};
const engine_t cat = {"CAT", c_init, c_note, c_render, c_panic, 3, {NAMES1, NAMES2, NAMES3, 0, 0}, {par1, par2, par3, 0, 0}, c_text, TITLES};
