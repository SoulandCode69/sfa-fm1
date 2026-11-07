/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * CAT KIT: 16 drums on the 16 white keys, six kits. Each drum is one of nine families, each its own algorithm:
 *   KICK   sine (or tri) body, exponential pitch drop, a lowpassed-noise click, soft drive, long low tail
 *   SNARE  two tones + noise highpassed and band-shaped, the noise with its own decay and SNAP
 *   HAT    six squares (808 ratios) -> bandpass ~9 kHz -> highpass, plus a little noise; closed chokes open
 *   CLAP   bandpassed noise, three 10 ms bursts then a tail
 *   TOM    sine with a pitch drop and a noise tick
 *   METAL  two squares -> bandpass (cowbell 587 + 845 Hz at 2.6 kHz, bells)
 *   CLICK  a short pitched blip through a resonant band (rim, clave, stick, woodblock)
 *   NOISE  noise through highpass / bandpass with a decay (shaker, maracas, wind, hiss)
 *   ZAP    a pitch sweep on sine / square (zaps, lasers, risers, FM blips)
 *   page 1: TUNE DECAY SNAP DRIVE    page 2: KIT HATS TONE LEVEL    page 3: PENV NDECAY PUNCH COMP (the kit's bus compressor) */
#include "fastmath.h"
#include "engine.h"

static const char *const NAMES1[NPARAM] = {"TUNE", "DECAY", "SNAP", "DRIVE"};
static const char *const NAMES2[NPARAM] = {"KIT", "HATS", "TONE", "LEVEL"};
static const char *const NAMES3[NPARAM] = {"PENV", "NDECAY", "PUNCH", "COMP"};
static uint8_t par1[NPARAM] = {64, 64, 64, 25};
static uint8_t par2[NPARAM] = {0, 50, 64, 100};
static uint8_t par3[NPARAM] = {64, 64, 30, 50};
static const char *const KITS[6] = {"808", "909", "LOFI", "FX", "ROOM", "GLITCH"};
#define NPADS 16
#define NKITS 6
#define KIT_I (par2[0] / 22u > 5u ? 5u : par2[0] / 22u)

enum { KICK, SNARE, HAT, CLAP, TOM, METAL, CLICK, NOISE, ZAP };
typedef struct {
    uint8_t fam, tri;            /* family; tri: KICK/TOM triangle body, HAT/NOISE "open" (long), ZAP square */
    float f0, f1;                /* hz: start / end (KICK TOM ZAP); tones (SNARE METAL); band (HAT CLAP NOISE CLICK) */
    float pd;                    /* pitch drop seconds */
    float dec;                   /* body decay seconds */
    float ndec;                  /* noise decay seconds (SNARE CLAP HAT TOM) */
    float nz;                    /* noise amount / click amount */
    float drv, lvl;
} drum_t;
#define D(fam, tri, f0, f1, pd, dec, ndec, nz, drv, lvl) {fam, tri, f0, f1, pd, dec, ndec, nz, drv, lvl}

static const drum_t KIT[NKITS][NPADS] = {
    {   /* 808 */
        D(KICK, 0, 190, 50, 0.040f, 0.85f, 0.01f, 0.25f, 1.6f, 1.0f),
        D(KICK, 0, 150, 42, 0.090f, 1.80f, 0.01f, 0.10f, 1.3f, 1.0f),
        D(SNARE, 0, 180, 330, 0.0f, 0.16f, 0.22f, 0.80f, 1.2f, 0.85f),
        D(CLICK, 0, 1750, 0, 0.0f, 0.02f, 0.0f, 0.3f, 1.2f, 0.55f),            /* rim */
        D(CLAP, 0, 1100, 0, 0.0f, 0.0f, 0.22f, 1.0f, 1.3f, 0.75f),
        D(HAT, 0, 9000, 7000, 0.0f, 0.045f, 0.0f, 0.2f, 1.0f, 0.5f),
        D(HAT, 1, 9000, 7000, 0.0f, 0.45f, 0.0f, 0.2f, 1.0f, 0.45f),
        D(HAT, 1, 6500, 4000, 0.0f, 1.60f, 0.0f, 0.5f, 1.0f, 0.4f),            /* cymbal */
        D(TOM, 0, 160, 88, 0.05f, 0.55f, 0.04f, 0.12f, 1.2f, 0.9f),
        D(TOM, 0, 220, 130, 0.05f, 0.45f, 0.04f, 0.12f, 1.2f, 0.9f),
        D(TOM, 0, 310, 185, 0.04f, 0.35f, 0.04f, 0.12f, 1.2f, 0.9f),
        D(METAL, 0, 587, 845, 0.0f, 0.30f, 0.0f, 2600.0f, 1.3f, 0.6f),         /* cowbell */
        D(CLICK, 0, 2500, 0, 0.0f, 0.035f, 0.0f, 0.0f, 1.2f, 0.55f),           /* clave */
        D(NOISE, 0, 6000, 0, 0.0f, 0.06f, 0.0f, 0.0f, 1.0f, 0.4f),             /* maracas */
        D(TOM, 0, 420, 370, 0.015f, 0.25f, 0.02f, 0.05f, 1.3f, 0.75f),         /* conga */
        D(ZAP, 0, 90, 28, 0.6f, 1.2f, 0.0f, 0.0f, 1.6f, 1.0f),                 /* sub drop */
    },
    {   /* 909 */
        D(KICK, 1, 300, 56, 0.025f, 0.40f, 0.004f, 0.9f, 2.2f, 1.0f),
        D(KICK, 1, 260, 60, 0.015f, 0.26f, 0.004f, 1.0f, 2.8f, 1.0f),
        D(SNARE, 1, 230, 185, 0.012f, 0.13f, 0.18f, 1.0f, 1.5f, 0.9f),
        D(SNARE, 1, 180, 170, 0.010f, 0.09f, 0.10f, 0.6f, 1.6f, 0.8f),
        D(CLAP, 0, 1500, 0, 0.0f, 0.0f, 0.16f, 1.0f, 1.5f, 0.8f),
        D(HAT, 0, 10000, 8000, 0.0f, 0.04f, 0.0f, 0.5f, 1.2f, 0.5f),
        D(HAT, 1, 10000, 8000, 0.0f, 0.50f, 0.0f, 0.5f, 1.2f, 0.45f),
        D(HAT, 1, 7000, 5000, 0.0f, 1.80f, 0.0f, 0.7f, 1.0f, 0.4f),            /* ride */
        D(TOM, 1, 180, 95, 0.045f, 0.38f, 0.03f, 0.15f, 1.5f, 0.9f),
        D(TOM, 1, 250, 140, 0.045f, 0.32f, 0.03f, 0.15f, 1.5f, 0.9f),
        D(TOM, 1, 340, 200, 0.035f, 0.26f, 0.03f, 0.15f, 1.5f, 0.9f),
        D(CLICK, 0, 1800, 0, 0.0f, 0.015f, 0.0f, 0.5f, 1.5f, 0.55f),           /* rim */
        D(METAL, 0, 800, 1130, 0.0f, 0.35f, 0.0f, 3000.0f, 1.2f, 0.5f),        /* bell */
        D(NOISE, 0, 8000, 0, 0.0f, 0.05f, 0.0f, 0.0f, 1.0f, 0.4f),             /* shaker */
        D(ZAP, 1, 900, 300, 0.05f, 0.12f, 0.0f, 0.0f, 1.2f, 0.6f),
        D(NOISE, 1, 400, 1.2f, 0.0f, 0.5f, 0.0f, 0.0f, 1.2f, 0.6f),            /* swell: bandpassed */
    },
    {   /* LOFI */
        D(KICK, 1, 140, 46, 0.02f, 0.25f, 0.01f, 0.3f, 3.5f, 1.0f),
        D(KICK, 0, 100, 42, 0.04f, 0.50f, 0.01f, 0.0f, 4.0f, 1.0f),
        D(SNARE, 1, 170, 160, 0.01f, 0.10f, 0.12f, 0.9f, 2.5f, 0.85f),
        D(NOISE, 0, 2500, 1.0f, 0.0f, 0.05f, 0.0f, 0.5f, 2.0f, 0.6f),          /* snap */
        D(CLAP, 0, 1000, 0, 0.0f, 0.0f, 0.12f, 1.0f, 2.0f, 0.75f),
        D(NOISE, 0, 7000, 0, 0.0f, 0.03f, 0.0f, 0.0f, 1.5f, 0.42f),            /* closed hat */
        D(NOISE, 1, 7000, 0, 0.0f, 0.30f, 0.0f, 0.0f, 1.5f, 0.38f),            /* open hat */
        D(NOISE, 1, 5000, 0.4f, 0.0f, 0.80f, 0.0f, 0.0f, 1.2f, 0.35f),         /* wash */
        D(TOM, 0, 230, 200, 0.02f, 0.30f, 0.02f, 0.05f, 1.5f, 0.8f),           /* conga lo */
        D(TOM, 0, 330, 300, 0.02f, 0.25f, 0.02f, 0.05f, 1.5f, 0.8f),           /* conga hi */
        D(TOM, 0, 520, 470, 0.015f, 0.15f, 0.02f, 0.05f, 1.5f, 0.8f),          /* bongo */
        D(METAL, 0, 660, 950, 0.0f, 0.2f, 0.0f, 2200.0f, 1.3f, 0.55f),         /* cowbell */
        D(CLICK, 0, 2200, 0, 0.0f, 0.025f, 0.0f, 0.3f, 1.3f, 0.55f),           /* woodblock */
        D(CLICK, 0, 1200, 0, 0.0f, 0.06f, 0.0f, 0.1f, 1.2f, 0.5f),             /* tek */
        D(TOM, 0, 90, 85, 0.1f, 0.6f, 0.01f, 0.0f, 1.2f, 0.9f),                /* tabla bass */
        D(NOISE, 0, 300, 0.4f, 0.0f, 0.02f, 0.0f, 0.5f, 3.0f, 0.5f),           /* vinyl tick */
    },
    {   /* FX */
        D(ZAP, 0, 3000, 60, 0.08f, 0.30f, 0.0f, 0.0f, 1.5f, 0.8f),
        D(ZAP, 0, 60, 2500, 0.30f, 0.35f, 0.0f, 0.0f, 1.5f, 0.7f),
        D(ZAP, 1, 4000, 300, 0.12f, 0.25f, 0.0f, 0.0f, 1.2f, 0.6f),            /* laser */
        D(ZAP, 1, 200, 2000, 0.05f, 0.08f, 0.0f, 0.0f, 1.2f, 0.6f),            /* blip */
        D(METAL, 0, 300, 423, 0.0f, 0.6f, 0.0f, 1200.0f, 1.5f, 0.6f),          /* gong */
        D(HAT, 0, 5000, 3000, 0.0f, 0.06f, 0.0f, 0.0f, 1.3f, 0.5f),            /* metal tick */
        D(HAT, 1, 12000, 9000, 0.0f, 0.6f, 0.0f, 0.3f, 1.3f, 0.4f),            /* metal ring */
        D(NOISE, 1, 200, 1.5f, 0.0f, 1.5f, 0.0f, 0.0f, 1.5f, 0.5f),            /* wind */
        D(ZAP, 0, 80, 20, 1.0f, 1.8f, 0.0f, 0.0f, 2.5f, 1.0f),                 /* sub drop */
        D(KICK, 0, 55, 55, 0.0f, 2.0f, 0.01f, 0.0f, 1.2f, 0.9f),               /* sub hold */
        D(ZAP, 1, 50, 50, 0.0f, 0.4f, 0.0f, 0.0f, 4.0f, 0.8f),                 /* buzz */
        D(METAL, 0, 1200, 1700, 0.0f, 0.35f, 0.0f, 4000.0f, 1.3f, 0.5f),       /* glass */
        D(NOISE, 0, 3500, 0.2f, 0.0f, 0.25f, 0.0f, 0.0f, 1.5f, 0.5f),          /* steam */
        D(CLICK, 0, 800, 0, 0.0f, 0.5f, 0.0f, 0.8f, 1.3f, 0.6f),               /* ping */
        D(ZAP, 1, 150, 1500, 0.4f, 0.5f, 0.0f, 0.0f, 1.5f, 0.6f),              /* riser */
        D(NOISE, 0, 90, 0.8f, 0.0f, 0.4f, 0.0f, 0.5f, 2.0f, 0.9f),             /* boom */
    },
    {   /* ROOM */
        D(KICK, 1, 150, 60, 0.025f, 0.35f, 0.02f, 0.4f, 1.3f, 1.0f),
        D(KICK, 0, 115, 52, 0.05f, 0.70f, 0.03f, 0.1f, 1.2f, 1.0f),
        D(SNARE, 1, 200, 190, 0.012f, 0.22f, 0.30f, 0.8f, 1.2f, 0.9f),
        D(SNARE, 1, 240, 230, 0.010f, 0.15f, 0.15f, 0.5f, 1.3f, 0.75f),        /* side stick snare */
        D(CLAP, 0, 1300, 0, 0.0f, 0.0f, 0.25f, 1.0f, 1.2f, 0.7f),
        D(NOISE, 0, 9000, 0, 0.0f, 0.07f, 0.0f, 0.0f, 1.1f, 0.4f),
        D(NOISE, 1, 8000, 0, 0.0f, 0.50f, 0.0f, 0.0f, 1.1f, 0.36f),
        D(NOISE, 1, 6000, 0.3f, 0.0f, 2.0f, 0.0f, 0.0f, 1.0f, 0.35f),          /* crash */
        D(TOM, 0, 130, 80, 0.06f, 0.6f, 0.06f, 0.15f, 1.2f, 0.9f),
        D(TOM, 0, 180, 115, 0.05f, 0.5f, 0.05f, 0.15f, 1.2f, 0.9f),
        D(TOM, 0, 250, 165, 0.04f, 0.4f, 0.04f, 0.15f, 1.2f, 0.9f),
        D(CLICK, 0, 1100, 0, 0.0f, 0.08f, 0.0f, 0.5f, 1.2f, 0.5f),             /* stick */
        D(NOISE, 0, 7000, 0, 0.0f, 0.10f, 0.0f, 0.0f, 1.0f, 0.4f),             /* brush */
        D(CLICK, 0, 2800, 0, 0.0f, 0.4f, 0.0f, 0.3f, 1.1f, 0.5f),              /* triangle */
        D(METAL, 0, 900, 1270, 0.0f, 0.8f, 0.0f, 3500.0f, 1.1f, 0.45f),        /* chime */
        D(KICK, 0, 70, 45, 0.3f, 1.5f, 0.01f, 0.0f, 1.3f, 0.9f),               /* big floor */
    },
    {   /* GLITCH */
        D(KICK, 1, 300, 50, 0.01f, 0.12f, 0.005f, 1.0f, 4.0f, 0.9f),
        D(KICK, 0, 900, 40, 0.005f, 0.20f, 0.01f, 1.0f, 2.0f, 1.0f),           /* thump */
        D(NOISE, 0, 4000, 1.5f, 0.0f, 0.04f, 0.0f, 1.0f, 3.0f, 0.7f),          /* snare bit */
        D(CLICK, 0, 2200, 0, 0.0f, 0.01f, 0.0f, 1.0f, 2.0f, 0.5f),             /* click */
        D(CLAP, 0, 1200, 0, 0.0f, 0.0f, 0.06f, 1.0f, 3.0f, 0.7f),              /* clap bits */
        D(HAT, 0, 6000, 6000, 0.0f, 0.015f, 0.0f, 0.5f, 1.5f, 0.45f),          /* tick hat */
        D(HAT, 1, 4000, 4000, 0.0f, 0.12f, 0.0f, 0.7f, 1.5f, 0.4f),            /* buzz hat */
        D(NOISE, 1, 12000, 0.2f, 0.0f, 0.3f, 0.0f, 0.0f, 1.5f, 0.35f),         /* hiss */
        D(ZAP, 1, 120, 60, 0.02f, 0.15f, 0.0f, 0.0f, 3.0f, 0.7f),              /* sq tom lo */
        D(ZAP, 1, 200, 100, 0.02f, 0.12f, 0.0f, 0.0f, 3.0f, 0.7f),             /* sq tom mid */
        D(ZAP, 1, 320, 160, 0.02f, 0.10f, 0.0f, 0.0f, 3.0f, 0.7f),             /* sq tom hi */
        D(ZAP, 0, 1500, 300, 0.03f, 0.10f, 0.0f, 0.0f, 1.5f, 0.6f),            /* blip */
        D(ZAP, 0, 200, 3000, 0.02f, 0.06f, 0.0f, 0.0f, 1.5f, 0.6f),            /* chirp */
        D(ZAP, 0, 5000, 100, 0.004f, 0.05f, 0.0f, 0.0f, 2.0f, 0.6f),           /* zip */
        D(NOISE, 0, 2000, 0.5f, 0.0f, 0.008f, 0.0f, 1.0f, 4.0f, 0.6f),         /* crackle */
        D(ZAP, 1, 40, 40, 0.0f, 0.25f, 0.0f, 0.0f, 6.0f, 0.8f),                /* bit bass */
    },
};

static const char *const PAD_NAMES[NKITS][NPADS] = {
    {"KICK", "BOOM", "SNARE", "RIM", "CLAP", "CHAT", "OHAT", "CYMBAL", "LO TOM", "MID TOM", "HI TOM", "COWBELL", "CLAVE", "MARACAS", "CONGA", "SUB DROP"},
    {"KICK", "TIGHT KICK", "SNARE", "DRY SNARE", "CLAP", "CHAT", "OHAT", "RIDE", "LO TOM", "MID TOM", "HI TOM", "RIM", "BELL", "SHAKER", "ZAP", "SWELL"},
    {"KICK", "CRUSHED", "SNARE", "SNAP", "CLAP", "CHAT", "OHAT", "WASH", "CONGA LO", "CONGA HI", "BONGO", "COWBELL", "WOODBLOCK", "TEK", "TABLA", "TICK"},
    {"ZAP DOWN", "ZAP UP", "LASER", "BLIP", "GONG", "TICK", "RING", "WIND", "SUB DROP", "SUB HOLD", "BUZZ", "GLASS", "STEAM", "PING", "RISER", "BOOM"},
    {"KICK", "FLOOR KICK", "SNARE", "SIDESTICK", "CLAP", "CHAT", "OHAT", "CRASH", "FLOOR TOM", "MID TOM", "RACK TOM", "STICK", "BRUSH", "TRIANGLE", "CHIME", "BIG FLOOR"},
    {"KICK", "THUMP", "SNARE BIT", "CLICK", "CLAP BITS", "TICK", "BUZZ HAT", "HISS", "SQ TOM LO", "SQ TOM MID", "SQ TOM HI", "FM BLIP", "FM CHIRP", "ZIP", "CRACKLE", "BIT BASS"},
};
const char *cat_pad_name(int pad) { return pad >= 0 && pad < NPADS ? PAD_NAMES[KIT_I][pad] : ""; }
const char *cat_kit_name(void) { return KITS[KIT_I]; }
static volatile int8_t last_pad = -1;
int cat_last_pad(void) { return last_pad; }
void cat_kit(int32_t step) { int32_t k = ((int32_t)KIT_I + step + NKITS) % NKITS; par2[0] = (uint8_t)(k * 22 + 2); }

typedef struct {
    const drum_t *d;
    float env, nenv, penv, ph, ph2, hz, f0, f1, t, vel, acoef, ncoef, pcoef, hp1, hp2, lp;
    svf_t f_a, f_b; svfc_t c_a, c_b;
    uint8_t on;
} voice_t;
static voice_t v[NPADS];
static float metal_ph[6];
static uint32_t rng = 0x5F3759DFu;
static volatile uint8_t trig[NPADS], trig_vel[NPADS];
static float comp_env;                   /* the kit's bus compressor */

static void c_init(void) { uint32_t i; for (i = 0; i < NPADS; i++) v[i].on = 0; comp_env = 0; }
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
    const drum_t *d = &KIT[KIT_I][p];
    voice_t *w = &v[p];
    int tuned = d->fam == KICK || d->fam == TOM || d->fam == ZAP || d->fam == METAL || d->fam == CLICK;
    float tune = fm_exp2f((p01(par1[0]) - 0.5f) * (d->fam == KICK ? 1.4f : tuned ? 1.0f : 0.0f));
    float dec = fm_exp2f((p01(par1[1]) - 0.5f) * 3.0f);
    float hats = fm_exp2f((p01(par2[1]) - 0.5f) * 3.0f);
    float ndm = fm_exp2f((p01(par3[1]) - 0.5f) * 2.0f);
    float tone = p01(par2[2]);
    uint32_t i;
    float len;
    if (d->fam == HAT || (d->fam == NOISE && d->f1 == 0.0f && d->dec < 1.0f))   /* hats choke each other */
        for (i = 0; i < NPADS; i++) if (i != p && v[i].on && v[i].d->fam == d->fam && v[i].d->tri && !d->tri) v[i].on = 0;
    w->d = d;
    w->on = 1; w->vel = 0.25f + 0.75f * vel; w->t = 0; w->ph = 0; w->ph2 = 0.25f; w->hp1 = w->hp2 = w->lp = 0;
    w->f_a.ic1 = w->f_a.ic2 = w->f_b.ic1 = w->f_b.ic2 = 0;
    w->env = 1.0f; w->nenv = 1.0f; w->penv = 1.0f;
    w->f0 = d->f0 * tune; w->f1 = d->f1 * (tuned ? tune : 1.0f); w->hz = w->f0;
    len = d->dec * (d->fam == HAT || (d->fam == NOISE && d->f1 == 0.0f) ? hats : dec);
    w->acoef = decay_coef(len < 0.003f ? 0.003f : len);
    w->ncoef = decay_coef((d->ndec > 0.0f ? d->ndec : d->dec) * dec * ndm);
    w->pcoef = d->pd > 0.0f ? decay_coef(d->pd * fm_exp2f((p01(par3[0]) - 0.5f) * 3.0f)) : 0.0f;
    switch (d->fam) {                                               /* the filters, set at the hit */
    case SNARE: svf_coef(&w->c_a, 1200.0f + tone * 2500.0f, 0.3f); break;            /* noise band */
    case HAT: svf_coef(&w->c_a, w->f0 * (0.7f + tone * 0.6f), 0.35f); break;          /* metal band */
    case CLAP: svf_coef(&w->c_a, d->f0 * (0.7f + tone * 0.6f), 0.8f); break;
    case METAL: svf_coef(&w->c_a, d->nz * (0.8f + tone * 0.4f), 0.4f); break;
    case CLICK: svf_coef(&w->c_a, w->f0, 0.92f); break;
    case NOISE: if (d->f1 > 0.0f) svf_coef(&w->c_a, d->f0 * (0.7f + tone * 0.6f), d->f1 > 1.0f ? 0.85f : d->f1 * 0.6f); break;
    default: break;
    }
}

static inline float hp1(float *st, float x, float c) { *st += (x - *st) * c; return x - *st; }

static void c_render(float *out, uint32_t n)
{
    uint32_t i, p;
    float snap = p01(par1[2]) * 2.0f;
    float drive = 1.0f + p01(par1[3]) * 3.0f, norm = 1.0f / fm_tanhf(drive * 0.6f);
    float tone = p01(par2[2]);
    float level = p01(par2[3]) * 1.3f;
    float punch = p01(par3[2]) * 2.0f;
    float comp = p01(par3[3]);
    int metal_on = 0;
    for (p = 0; p < NPADS; p++)
        if (trig[p]) { trig[p] = 0; start(p, (float)trig_vel[p] * (1.0f / 127.0f)); }
    for (p = 0; p < NPADS; p++) if (v[p].on && v[p].d->fam == HAT) metal_on = 1;
    for (i = 0; i < n; i++) {
        float s = 0.0f, nz = noise(&rng), metal = 0.0f, g;
        uint32_t m;
        if (metal_on) {                                            /* six squares at the 808's ratios */
            static const float HZ[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
            for (m = 0; m < 6u; m++) {
                metal_ph[m] += HZ[m] * (1.0f + tone * 0.6f) * (1.0f / SR);
                if (metal_ph[m] >= 1.0f) metal_ph[m] -= 1.0f;
                metal += metal_ph[m] < 0.5f ? 1.0f : -1.0f;
            }
            metal *= 1.0f / 6.0f;
        }
        for (p = 0; p < NPADS; p++) {
            voice_t *w = &v[p];
            const drum_t *d;
            float o = 0.0f, tr;
            if (!w->on) continue;
            d = w->d;
            w->env *= w->acoef;
            w->nenv *= w->ncoef;
            if (w->pcoef) { w->penv *= w->pcoef; w->hz = w->f1 + (w->f0 - w->f1) * w->penv; }
            w->t += 1.0f / SR;
            tr = w->t < 0.02f ? 1.0f - w->t * 50.0f : 0.0f;                     /* the transient window */
            switch (d->fam) {
            case KICK: {
                float body, click;
                w->ph += w->hz * (1.0f / SR); if (w->ph >= 1.0f) w->ph -= 1.0f;
                body = d->tri ? 4.0f * fm_fabsf(w->ph - 0.5f) - 1.0f : fm_sinf(w->ph * 6.2831853f);
                body = fm_tanhf(body * (1.2f + d->drv * 0.6f * w->env)) * w->env;      /* drive eases off as it decays */
                w->lp += (nz - w->lp) * 0.35f;                                         /* the click: lowpassed noise */
                click = w->lp * w->nenv * d->nz * snap;
                o = body * 1.1f + click + (tr > 0.0f ? body * tr * punch * 0.5f : 0.0f);
                break; }
            case SNARE: {
                float tn, ns;
                w->ph += w->hz * (1.0f / SR); if (w->ph >= 1.0f) w->ph -= 1.0f;
                w->ph2 += (d->tri ? w->hz * 1.6f : w->f1) * (1.0f / SR); if (w->ph2 >= 1.0f) w->ph2 -= 1.0f;
                tn = (fm_sinf(w->ph * 6.2831853f) + fm_sinf(w->ph2 * 6.2831853f) * 0.6f) * w->env;
                w->lp += (nz - w->lp) * (0.22f + tone * 0.3f);                         /* the noise: lowpassed ~3 kHz.. */
                ns = hp1(&w->hp1, w->lp, 0.06f);                                        /* ..and highpassed ~450 Hz */
                ns = ns * 0.6f + svf_bp(&w->f_a, &w->c_a, ns) * 1.2f;                  /* plus a band for body */
                ns *= w->nenv * d->nz * (0.5f + snap * 0.5f);
                o = tn * 1.0f + ns * 0.7f;
                break; }
            case HAT: {
                float x = svf_bp(&w->f_a, &w->c_a, metal) * 2.2f;                    /* the metal, banded ~9 kHz */
                x = hp1(&w->hp1, x, 0.5f);                                              /* then highpassed */
                x += hp1(&w->hp2, nz, 0.6f) * d->nz * 0.6f;
                o = x * w->env * 1.2f;
                break; }
            case CLAP: {
                float burst = w->t < 0.03f ? ((w->t - (float)(int)(w->t * 100.0f) * 0.01f) < 0.0035f ? 1.0f : 0.12f) : 1.0f;
                if (w->t >= 0.03f) w->env *= 1.0f; else w->env = 1.0f;
                o = svf_bp(&w->f_a, &w->c_a, nz) * 3.5f * burst * w->nenv * (0.6f + snap * 0.4f);
                if (w->t < 0.03f) w->nenv = 1.0f;
                break; }
            case TOM: {
                float body;
                w->ph += w->hz * (1.0f / SR); if (w->ph >= 1.0f) w->ph -= 1.0f;
                body = d->tri ? 4.0f * fm_fabsf(w->ph - 0.5f) - 1.0f : fm_sinf(w->ph * 6.2831853f);
                w->lp += (nz - w->lp) * 0.3f;
                o = body * w->env + w->lp * w->nenv * d->nz * 0.25f * snap + body * tr * punch * 0.4f;
                break; }
            case METAL: {
                float x;
                w->ph += w->f0 * (1.0f / SR); if (w->ph >= 1.0f) w->ph -= 1.0f;
                w->ph2 += w->f1 * (1.0f / SR); if (w->ph2 >= 1.0f) w->ph2 -= 1.0f;
                x = (w->ph < 0.5f ? 1.0f : -1.0f) + (w->ph2 < 0.5f ? 1.0f : -1.0f);
                o = svf_bp(&w->f_a, &w->c_a, x * 0.5f) * 2.0f * w->env;
                break; }
            case CLICK: {
                float x = w->t < 0.0015f ? 1.0f : 0.0f;                                   /* an impulse into a ringing band */
                x += nz * d->nz * w->nenv * 0.3f;
                o = (svf_bp(&w->f_a, &w->c_a, x) * 3.0f + x * 0.3f) * w->env;
                break; }
            case NOISE: {
                float x = d->f1 > 0.0f ? svf_bp(&w->f_a, &w->c_a, nz) * 2.5f : hp1(&w->hp1, nz, 0.3f + (d->f0 > 5000.0f ? 0.3f : 0.0f) + tone * 0.2f);
                if (d->f1 == 0.0f) x = hp1(&w->hp2, x, 0.3f) * 1.2f;                     /* twice: steeper */
                o = x * w->env * (1.0f + d->nz * snap * tr);
                break; }
            default: {                                                                    /* ZAP */
                float x;
                w->ph += w->hz * (1.0f / SR); if (w->ph >= 1.0f) w->ph -= 1.0f;
                x = d->tri ? (w->ph < 0.5f ? 1.0f : -1.0f) : fm_sinf(w->ph * 6.2831853f);
                o = x * w->env;
                break; }
            }
            s += fm_tanhf(o * d->drv) * d->lvl * w->vel * (1.0f + punch * tr * 0.6f);
            if (w->env < 0.0004f && w->nenv < 0.0004f) w->on = 0;
        }
        /* the kit bus: compressor (fast attack, 80 ms release) then drive */
        {
            float a = fm_fabsf(s), thr = 1.0f - comp * 0.75f, ratio_inv = 1.0f - comp * 0.75f;
            comp_env += (a > comp_env ? 0.3f : 0.0003f) * (a - comp_env);
            g = comp_env > thr ? fm_powf(comp_env / thr, ratio_inv - 1.0f) : 1.0f;
            s *= g * (1.0f + comp * 0.8f);                                            /* makeup */
        }
        out[i] += fm_tanhf(s * 0.6f * drive) * norm * level;
    }
}

static const char *c_text(uint8_t page, uint32_t p, char *buf)
{
    (void)buf;
    return page == 1u && p == 0u ? KITS[KIT_I] : 0;
}

const engine_t cat = {"CAT", c_init, c_note, c_render, c_panic, 3, {NAMES1, NAMES2, NAMES3, 0, 0}, {par1, par2, par3, 0, 0}, c_text};
