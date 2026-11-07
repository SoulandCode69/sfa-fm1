/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * CAT KIT: 16 drums on the 16 white keys, four kits (64 drums). Every drum is the same parametric voice:
 *   a source (sine / triangle / square / six-square metal / noise / 2-op FM) with a pitch sweep f0 -> f1,
 *   an amp decay, a noise layer with its own decay and colour (highpass or bandpass), a click, drive and level.
 *   Pads, every kit: 1 KICK 2 KICK2 3 SNARE 4 RIM/SNARE2 5 CLAP 6 CHAT 7 OHAT 8 CRASH/RIDE
 *                    9 LOW TOM 10 MID TOM 11 HI TOM 12 PERC 13 PERC 14 PERC 15 FX 16 FX
 *   page 1: K1 TUNE   kicks / toms / perc pitch   K2 DECAY  every drum's length   K3 SNAP  noise & click   K4 DRIVE
 *   page 2: K1 KIT    808 909 LOFI FX             K2 HATS   hat decay            K3 TONE  noise colour    K4 LEVEL */
#include "fastmath.h"
#include "engine.h"

static const char *const NAMES1[NPARAM] = {"TUNE", "DECAY", "SNAP", "DRIVE"};
static const char *const NAMES2[NPARAM] = {"KIT", "HATS", "TONE", "LEVEL"};
static uint8_t par1[NPARAM] = {64, 64, 64, 30};
static uint8_t par2[NPARAM] = {0, 50, 64, 100};
static const char *const KITS[4] = {"808", "909", "LOFI", "FX"};
#define NPADS 16
#define NKITS 4

enum { SINE, TRI, SQR, METAL, NOISE, FM };
typedef struct {
    uint8_t src, grp;           /* grp: 1 kick-ish (TUNE), 2 hats (HATS decay, choke), 3 perc (TUNE/2) */
    float f0, f1, psw;          /* start / end hz, pitch sweep seconds */
    float amp;                  /* amp decay seconds */
    float nmix, ndec, nbp, nq;  /* noise: mix, decay seconds, bandpass hz (0 = highpassed white), damping */
    float click, drive, lvl;
} drum_t;

#define D(src, grp, f0, f1, psw, amp, nmix, ndec, nbp, nq, click, drive, lvl) {src, grp, f0, f1, psw, amp, nmix, ndec, nbp, nq, click, drive, lvl}

static const drum_t KIT[NKITS][NPADS] = {
    {   /* 808: round and deep */
        D(SINE, 1, 160, 48, 0.03f, 0.9f, 0.0f, 0.01f, 0, 0, 0.3f, 1.8f, 1.2f),       /* kick */
        D(SINE, 1, 120, 40, 0.08f, 1.6f, 0.0f, 0.01f, 0, 0, 0.1f, 1.4f, 1.2f),       /* kick 2: long boom */
        D(TRI, 0, 190, 180, 0.01f, 0.18f, 0.9f, 0.22f, 1800, 0.8f, 0.2f, 1.2f, 0.9f),/* snare */
        D(SINE, 0, 1700, 1700, 0.0f, 0.02f, 0.3f, 0.015f, 3000, 0.3f, 0.6f, 1.5f, 0.5f),/* rim */
        D(NOISE, 0, 0, 0, 0.0f, 0.2f, 1.0f, 0.2f, 1300, 0.6f, 0.0f, 1.3f, 0.9f),     /* clap */
        D(METAL, 2, 0, 1.0f, 0.0f, 0.05f, 0.2f, 0.04f, 0, 0, 0.0f, 1.0f, 0.22f),      /* closed hat */
        D(METAL, 2, 0, 1.0f, 0.0f, 0.45f, 0.2f, 0.4f, 0, 0, 0.0f, 1.0f, 0.21f),        /* open hat */
        D(METAL, 0, 0, 1.6f, 0.0f, 1.4f, 0.5f, 1.2f, 0, 0, 0.0f, 1.0f, 0.2f),         /* cymbal */
        D(SINE, 1, 150, 85, 0.05f, 0.5f, 0.1f, 0.05f, 0, 0, 0.1f, 1.3f, 1.0f),       /* low tom */
        D(SINE, 1, 210, 125, 0.05f, 0.4f, 0.1f, 0.05f, 0, 0, 0.1f, 1.3f, 1.0f),      /* mid tom */
        D(SINE, 1, 300, 180, 0.04f, 0.3f, 0.1f, 0.05f, 0, 0, 0.1f, 1.3f, 1.0f),      /* hi tom */
        D(FM, 3, 560, 560, 0.0f, 0.25f, 0.0f, 0.01f, 0, 0, 0.0f, 1.2f, 0.8f),        /* cowbell */
        D(SINE, 3, 2500, 2400, 0.0f, 0.03f, 0.0f, 0.01f, 0, 0, 0.3f, 1.2f, 0.5f),    /* clave */
        D(NOISE, 0, 0, 0, 0.0f, 0.08f, 1.0f, 0.08f, 6000, 0.4f, 0.0f, 1.0f, 0.21f),   /* maracas */
        D(SINE, 3, 420, 380, 0.02f, 0.25f, 0.05f, 0.02f, 0, 0, 0.4f, 1.3f, 0.9f),    /* conga */
        D(SINE, 1, 90, 30, 0.6f, 1.2f, 0.0f, 0.01f, 0, 0, 0.0f, 1.6f, 1.2f),         /* sub drop */
    },
    {   /* 909: punchy and bright */
        D(TRI, 1, 240, 52, 0.02f, 0.45f, 0.05f, 0.005f, 0, 0, 0.8f, 2.4f, 1.2f),     /* kick */
        D(SINE, 1, 200, 55, 0.015f, 0.3f, 0.1f, 0.005f, 0, 0, 1.0f, 3.0f, 1.2f),     /* kick 2: tight */
        D(TRI, 0, 230, 185, 0.015f, 0.14f, 1.0f, 0.18f, 2600, 1.1f, 0.5f, 1.5f, 1.0f),/* snare */
        D(TRI, 0, 180, 170, 0.01f, 0.1f, 0.6f, 0.1f, 1200, 0.8f, 0.4f, 1.6f, 0.9f),  /* snare 2: dry */
        D(NOISE, 0, 0, 0, 0.0f, 0.15f, 1.0f, 0.15f, 1600, 0.5f, 0.0f, 1.6f, 1.0f),   /* clap */
        D(METAL, 2, 0, 1.35f, 0.0f, 0.04f, 0.5f, 0.03f, 0, 0, 0.0f, 1.2f, 0.22f),      /* closed hat */
        D(METAL, 2, 0, 1.35f, 0.0f, 0.5f, 0.5f, 0.45f, 0, 0, 0.0f, 1.2f, 0.21f),      /* open hat */
        D(METAL, 0, 0, 1.9f, 0.0f, 1.8f, 0.7f, 1.6f, 0, 0, 0.0f, 1.0f, 0.2f),         /* ride */
        D(TRI, 1, 170, 95, 0.04f, 0.35f, 0.15f, 0.03f, 0, 0, 0.3f, 1.6f, 1.0f),      /* low tom */
        D(TRI, 1, 240, 140, 0.04f, 0.3f, 0.15f, 0.03f, 0, 0, 0.3f, 1.6f, 1.0f),      /* mid tom */
        D(TRI, 1, 330, 200, 0.03f, 0.25f, 0.15f, 0.03f, 0, 0, 0.3f, 1.6f, 1.0f),     /* hi tom */
        D(SINE, 0, 1800, 1800, 0.0f, 0.015f, 0.4f, 0.01f, 4000, 0.3f, 1.0f, 1.5f, 0.5f),/* rim */
        D(FM, 3, 800, 800, 0.0f, 0.3f, 0.0f, 0.01f, 0, 0, 0.0f, 1.2f, 0.7f),         /* bell */
        D(NOISE, 0, 0, 0, 0.0f, 0.06f, 1.0f, 0.06f, 8000, 0.5f, 0.0f, 1.0f, 0.21f),   /* shaker */
        D(SQR, 3, 900, 300, 0.05f, 0.12f, 0.0f, 0.01f, 0, 0, 0.0f, 1.2f, 0.7f),      /* zap */
        D(NOISE, 0, 0, 0, 0.0f, 0.5f, 1.0f, 0.5f, 400, 1.2f, 0.0f, 1.2f, 0.8f),      /* noise swell */
    },
    {   /* LOFI: short, gritty, percussion */
        D(SQR, 1, 130, 45, 0.02f, 0.25f, 0.1f, 0.01f, 0, 0, 0.3f, 3.0f, 1.0f),       /* kick */
        D(SINE, 1, 100, 42, 0.04f, 0.5f, 0.0f, 0.01f, 0, 0, 0.0f, 3.5f, 1.1f),       /* kick 2: crushed */
        D(SQR, 0, 170, 160, 0.01f, 0.1f, 0.9f, 0.12f, 900, 1.3f, 0.3f, 2.2f, 0.9f),  /* snare */
        D(NOISE, 0, 0, 0, 0.0f, 0.05f, 1.0f, 0.05f, 2500, 0.9f, 0.5f, 2.0f, 0.5f),   /* snap */
        D(NOISE, 0, 0, 0, 0.0f, 0.12f, 1.0f, 0.12f, 1000, 1.0f, 0.0f, 2.0f, 0.9f),   /* clap */
        D(NOISE, 2, 0, 0, 0.0f, 0.03f, 1.0f, 0.03f, 0, 0, 0.0f, 1.5f, 0.22f),         /* closed hat */
        D(NOISE, 2, 0, 0, 0.0f, 0.3f, 1.0f, 0.3f, 0, 0, 0.0f, 1.5f, 0.21f),           /* open hat */
        D(NOISE, 0, 0, 0, 0.0f, 0.8f, 1.0f, 0.8f, 5000, 0.9f, 0.0f, 1.2f, 0.21f),     /* wash */
        D(SINE, 3, 230, 200, 0.02f, 0.3f, 0.05f, 0.02f, 0, 0, 0.3f, 1.5f, 0.9f),     /* conga low */
        D(SINE, 3, 330, 300, 0.02f, 0.25f, 0.05f, 0.02f, 0, 0, 0.3f, 1.5f, 0.9f),    /* conga hi */
        D(SINE, 3, 520, 470, 0.015f, 0.15f, 0.05f, 0.02f, 0, 0, 0.5f, 1.5f, 0.9f),   /* bongo */
        D(FM, 3, 660, 660, 0.0f, 0.2f, 0.0f, 0.01f, 0, 0, 0.0f, 1.3f, 0.7f),         /* cowbell */
        D(SINE, 3, 2200, 2200, 0.0f, 0.025f, 0.0f, 0.01f, 0, 0, 0.6f, 1.3f, 0.5f),   /* woodblock */
        D(TRI, 3, 1200, 900, 0.03f, 0.08f, 0.0f, 0.01f, 0, 0, 0.2f, 1.2f, 0.7f),     /* tabla tek */
        D(SINE, 3, 90, 85, 0.1f, 0.6f, 0.0f, 0.01f, 0, 0, 0.0f, 1.2f, 1.0f),         /* tabla bass */
        D(NOISE, 0, 0, 0, 0.0f, 0.02f, 1.0f, 0.02f, 300, 0.4f, 0.0f, 3.0f, 0.4f),    /* vinyl tick */
    },
    {   /* FX: lasers, zaps, hits, drops */
        D(SINE, 1, 3000, 60, 0.08f, 0.3f, 0.0f, 0.01f, 0, 0, 0.0f, 1.5f, 0.9f),      /* zap down */
        D(SINE, 1, 60, 2500, 0.3f, 0.35f, 0.0f, 0.01f, 0, 0, 0.0f, 1.5f, 0.8f),      /* zap up */
        D(SQR, 3, 4000, 300, 0.12f, 0.25f, 0.0f, 0.01f, 0, 0, 0.0f, 1.2f, 0.7f),     /* laser */
        D(SQR, 3, 200, 2000, 0.05f, 0.08f, 0.0f, 0.01f, 0, 0, 0.0f, 1.2f, 0.7f),     /* blip */
        D(FM, 3, 300, 300, 0.0f, 0.6f, 0.0f, 0.01f, 0, 0, 0.0f, 1.5f, 0.8f),         /* gong */
        D(METAL, 2, 0, 0.6f, 0.0f, 0.06f, 0.0f, 0.01f, 0, 0, 0.0f, 1.3f, 0.21f),       /* metal tick */
        D(METAL, 2, 0, 2.6f, 0.0f, 0.6f, 0.3f, 0.5f, 0, 0, 0.0f, 1.3f, 0.21f),         /* metal ring */
        D(NOISE, 0, 0, 0, 0.0f, 1.5f, 1.0f, 1.5f, 200, 1.5f, 0.0f, 1.5f, 0.5f),      /* wind */
        D(TRI, 1, 80, 20, 1.0f, 1.8f, 0.0f, 0.01f, 0, 0, 0.0f, 2.5f, 1.2f),          /* sub drop */
        D(SINE, 1, 55, 55, 0.0f, 2.0f, 0.0f, 0.01f, 0, 0, 0.0f, 1.2f, 1.0f),         /* sub hold */
        D(SQR, 1, 50, 50, 0.0f, 0.4f, 0.0f, 0.01f, 0, 0, 0.0f, 4.0f, 1.0f),          /* buzz */
        D(FM, 3, 1200, 1200, 0.0f, 0.35f, 0.0f, 0.01f, 0, 0, 0.0f, 1.3f, 0.6f),      /* glass */
        D(NOISE, 0, 0, 0, 0.0f, 0.25f, 1.0f, 0.25f, 3500, 0.2f, 0.0f, 1.5f, 0.5f),   /* steam */
        D(SINE, 3, 800, 790, 0.0f, 0.5f, 0.0f, 0.01f, 0, 0, 0.8f, 1.3f, 0.8f),       /* ping */
        D(TRI, 3, 150, 1500, 0.4f, 0.5f, 0.0f, 0.01f, 0, 0, 0.0f, 1.5f, 0.8f),       /* riser */
        D(NOISE, 0, 0, 0, 0.0f, 0.4f, 1.0f, 0.4f, 90, 0.8f, 0.5f, 2.0f, 1.0f),       /* boom */
    },
};

static const char *const PAD_NAMES[NKITS][NPADS] = {
    {"KICK", "BOOM", "SNARE", "RIM", "CLAP", "CHAT", "OHAT", "CYMBAL", "LO TOM", "MID TOM", "HI TOM", "COWBELL", "CLAVE", "MARACAS", "CONGA", "SUB DROP"},
    {"KICK", "TIGHT KICK", "SNARE", "DRY SNARE", "CLAP", "CHAT", "OHAT", "RIDE", "LO TOM", "MID TOM", "HI TOM", "RIM", "BELL", "SHAKER", "ZAP", "SWELL"},
    {"KICK", "CRUSHED", "SNARE", "SNAP", "CLAP", "CHAT", "OHAT", "WASH", "CONGA LO", "CONGA HI", "BONGO", "COWBELL", "WOODBLOCK", "TEK", "TABLA", "TICK"},
    {"ZAP DOWN", "ZAP UP", "LASER", "BLIP", "GONG", "TICK", "RING", "WIND", "SUB DROP", "SUB HOLD", "BUZZ", "GLASS", "STEAM", "PING", "RISER", "BOOM"},
};
const char *cat_pad_name(int pad) { return pad >= 0 && pad < NPADS ? PAD_NAMES[par2[0] >> 5][pad] : ""; }
const char *cat_kit_name(void) { return KITS[par2[0] >> 5]; }
static volatile int8_t last_pad = -1;
int cat_last_pad(void) { return last_pad; }

typedef struct {
    const drum_t *d;
    float env, nenv, penv, ph, ph2, hz, lp, bp, hp, t, vel, acoef, ncoef, pcoef, f0, f1;
    uint8_t on;
} voice_t;
static voice_t v[NPADS];
static float metal_ph[6];
static uint32_t rng = 0x5F3759DFu;
static volatile uint8_t trig[NPADS], trig_vel[NPADS];

static void c_init(void) { uint32_t i; for (i = 0; i < NPADS; i++) v[i].on = 0; }
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
    const drum_t *d = &KIT[par2[0] >> 5][p];
    voice_t *w = &v[p];
    float tune = fm_exp2f((p01(par1[0]) - 0.5f) * (d->grp == 1u ? 1.6f : d->grp == 3u ? 0.8f : 0.0f));
    float dec = fm_exp2f((p01(par1[1]) - 0.5f) * 3.0f);           /* 0.35 .. 2.8 x */
    float hats = fm_exp2f((p01(par2[1]) - 0.5f) * 3.0f);
    float len = d->amp * (d->grp == 2u ? hats : dec);
    uint32_t i;
    if (d->grp == 2u)                                               /* a hat chokes the other hats */
        for (i = 0; i < NPADS; i++) if (i != p && v[i].on && v[i].d->grp == 2u && v[i].d->amp > d->amp) v[i].on = 0;
    w->d = d;
    w->on = 1; w->vel = vel; w->t = 0; w->ph = 0; w->ph2 = 0.25f; w->lp = w->bp = w->hp = 0;
    w->env = 1.0f; w->nenv = 1.0f; w->penv = 1.0f;
    w->f0 = d->f0 * tune; w->f1 = d->f1 * tune; w->hz = w->f0;
    w->acoef = decay_coef(len < 0.004f ? 0.004f : len);
    w->ncoef = decay_coef(d->ndec * (d->grp == 2u ? hats : dec));
    w->pcoef = d->psw > 0.0f ? decay_coef(d->psw) : 0.0f;
}

static void c_render(float *out, uint32_t n)
{
    uint32_t i, p;
    float snap = p01(par1[2]) * 2.0f;
    float drive = 1.0f + p01(par1[3]) * 3.0f;
    float norm = 1.0f / fm_tanhf(drive * 0.6f);
    float tone = p01(par2[2]);
    float level = p01(par2[3]) * 1.3f;
    int metal_on = 0;
    for (p = 0; p < NPADS; p++)
        if (trig[p]) { trig[p] = 0; start(p, (float)trig_vel[p] * (1.0f / 127.0f)); }
    for (p = 0; p < NPADS; p++) if (v[p].on && v[p].d->src == METAL) metal_on = 1;
    for (i = 0; i < n; i++) {
        float s = 0.0f, nz = noise(&rng), metal = 0.0f;
        uint32_t m;
        if (metal_on) {                                            /* six squares, 808 ratios, shared */
            static const float HZ[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
            for (m = 0; m < 6u; m++) {
                metal_ph[m] += HZ[m] * (0.8f + tone * 1.6f) * (1.0f / SR);
                if (metal_ph[m] >= 1.0f) metal_ph[m] -= 1.0f;
                metal += metal_ph[m] < 0.5f ? 1.0f : -1.0f;
            }
            metal *= 1.0f / 6.0f;
        }
        for (p = 0; p < NPADS; p++) {
            voice_t *w = &v[p];
            const drum_t *d;
            float o = 0.0f, ns = 0.0f, y;
            if (!w->on) continue;
            d = w->d;
            w->env *= w->acoef;
            w->nenv *= w->ncoef;
            if (w->pcoef) { w->penv *= w->pcoef; w->hz = w->f1 + (w->f0 - w->f1) * w->penv; }
            w->t += 1.0f / SR;
            if (d->src != NOISE) {
                float f = d->src == METAL ? w->f1 : w->hz;                 /* METAL: f1 is a rate multiplier */
                if (d->src == METAL) {
                    y = metal - w->hp; w->hp += (metal - w->hp) * (0.15f + tone * 0.5f);   /* highpass */
                    o = y * 0.7f;
                } else {
                    w->ph += f * (1.0f / SR); if (w->ph >= 1.0f) w->ph -= 1.0f;
                    if (d->src == SINE) o = fm_sinf(w->ph * 6.2831853f);
                    else if (d->src == TRI) o = 4.0f * fm_fabsf(w->ph - 0.5f) - 1.0f;
                    else if (d->src == SQR) o = w->ph < 0.5f ? 1.0f : -1.0f;
                    else {                                                   /* FM: ratio 1.41, index on the envelope */
                        w->ph2 += f * 1.41f * (1.0f / SR); if (w->ph2 >= 1.0f) w->ph2 -= 1.0f;
                        o = fm_sinf(w->ph * 6.2831853f + fm_sinf(w->ph2 * 6.2831853f) * (1.5f + 2.0f * w->env));
                    }
                }
                o *= w->env;
            }
            if (d->nmix > 0.0f) {
                if (d->nbp > 0.0f) {                                         /* bandpassed noise, TONE moves it */
                    float f = 2.0f * fm_sinf(3.1415926f * d->nbp * (0.6f + tone * 0.8f) / SR);
                    if (f > 1.2f) f = 1.2f;
                    w->lp += f * w->bp; w->bp += f * (nz - w->lp - d->nq * w->bp);
                    ns = w->bp * 1.2f;
                } else {                                                     /* highpassed white */
                    w->hp += (nz - w->hp) * (0.3f + tone * 0.5f);
                    ns = (nz - w->hp) * 0.9f;
                }
                ns *= w->nenv * d->nmix * (0.5f + snap * 0.5f);
                if (d->src == NOISE && d->nmix >= 1.0f && w->t < 0.03f && d->nbp > 800.0f && d->nbp < 2000.0f)
                    ns *= (w->t - (float)(int)(w->t * 100.0f) * 0.01f) < 0.004f ? 1.0f : 0.2f;   /* a clap's bursts */
            }
            if (d->click > 0.0f && w->t < 0.004f) o += (w->t < 0.001f ? 1.0f : -0.5f) * d->click * snap * 0.6f;
            s += fm_tanhf((o + ns) * d->drive) * d->lvl * w->vel;
            if (w->env < 0.0005f && w->nenv < 0.0005f) w->on = 0;
        }
        out[i] += fm_tanhf(s * 0.6f * drive) * norm * level;
    }
}

static const char *c_text(uint8_t page, uint32_t p, char *buf)
{
    (void)buf;
    return page == 1u && p == 0u ? KITS[par2[0] >> 5] : 0;
}

const engine_t cat = {"CAT", c_init, c_note, c_render, c_panic, 2, {NAMES1, NAMES2, 0}, {par1, par2, 0}, c_text};
