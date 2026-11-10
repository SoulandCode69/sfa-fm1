/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * MANTIS: a synthesized drum machine. No samples: 16 voices, each a small analog / FM / noise model, on the 16
 * white keys. Every voice has PITCH DECAY TONE LEVEL and BEND SNAP DRIVE NOISE of its own; a kit is a full set of
 * those for all 16 voices, 8 kits (ALGO), and your edits are saved per kit. The voice pages follow the pad you hit.
 *   page 1 VOICE:  PITCH DECAY TONE LEVEL      page 2 VOICE+:  BEND SNAP DRIVE NOISE
 *   page 3 LAYER:  SAMPLE (an ABDUCT hit layered under the voice, at its pitch and level)
 *   page 4 KIT:    KIT TUNE DECAY LEVEL        page 5 BUS:     COMP PUNCH LOWCUT DRIVE
 * The 16 voices: KICK KICK2 SNARE RIM CLAP CHAT OHAT CYM LTOM MTOM HTOM BELL ZAP CLICK METAL BLIP. */
#include "fastmath.h"
#include "engine.h"
#include "../samples.h"

static const char *const NAMES1[NPARAM] = {"PITCH", "DECAY", "TONE", "LEVEL"};
static const char *const NAMES1B[NPARAM] = {"BEND", "SNAP", "DRIVE", "NOISE"};
static const char *const NAMES1C[NPARAM] = {"SAMPLE", "-", "-", "-"};
static const char *const NAMES2[NPARAM] = {"KIT", "TUNE", "DECAY", "LEVEL"};
static const char *const NAMES3[NPARAM] = {"COMP", "PUNCH", "LOWCUT", "DRIVE"};
static uint8_t par1[NPARAM] = {64, 64, 64, 100};
static uint8_t par1b[NPARAM] = {64, 30, 0, 30};
static uint8_t par1c[NPARAM] = {0, 0, 0, 0};
static uint8_t par2[NPARAM] = {0, 64, 64, 100};
static uint8_t par3[NPARAM] = {25, 15, 0, 0};
#define NPADS 16
#define MAXKITS 8
#define PP 9                                             /* + LAYER: the sample */
enum { V_KICK, V_KICK2, V_SNARE, V_RIM, V_CLAP, V_CHAT, V_OHAT, V_CYM, V_LTOM, V_MTOM, V_HTOM, V_BELL, V_ZAP, V_CLICK, V_METAL, V_BLIP };
static const char *const VNAME[NPADS] = {"KICK", "KICK2", "SNARE", "RIM", "CLAP", "CHAT", "OHAT", "CYM", "LTOM", "MTOM", "HTOM", "BELL", "ZAP", "CLICK", "METAL", "BLIP"};
static const char *const KNAME[MAXKITS] = {"MANTIS", "ANALOG", "DIGITAL", "INDUSTRY", "GLITCH", "TRIBAL", "MINIMAL", "SAUCER"};
static uint8_t kit_i;
static uint8_t pad_par[MAXKITS][NPADS][PP];
static volatile int8_t last_pad = 0;
static uint8_t shown_pad = 0, shown_kit = 0;

/* the factory kits: PITCH DECAY TONE LEVEL BEND SNAP DRIVE NOISE per voice; a kit tweaks the base per voice */
static const uint8_t BASE[NPADS][PP] = {
    {64, 70, 50, 110, 70, 40, 20, 0},    /* KICK */
    {64, 50, 70, 104, 90, 60, 30, 0},    /* KICK2 */
    {64, 55, 64, 100, 40, 50, 10, 70},   /* SNARE */
    {64, 30, 80, 84, 0, 30, 0, 20},      /* RIM */
    {64, 55, 60, 94, 0, 20, 0, 100},     /* CLAP */
    {64, 22, 80, 74, 0, 0, 0, 90},       /* CHAT */
    {64, 70, 80, 66, 0, 0, 0, 90},       /* OHAT */
    {64, 100, 70, 60, 0, 0, 0, 80},      /* CYM */
    {64, 60, 50, 92, 50, 20, 10, 10},    /* LTOM */
    {64, 55, 55, 92, 50, 20, 10, 10},    /* MTOM */
    {64, 50, 60, 92, 50, 20, 10, 10},    /* HTOM */
    {64, 75, 60, 78, 0, 10, 0, 0},       /* BELL */
    {64, 35, 70, 84, 127, 30, 20, 0},    /* ZAP */
    {64, 15, 90, 80, 0, 60, 0, 40},      /* CLICK */
    {64, 60, 70, 76, 0, 20, 30, 20},     /* METAL */
    {64, 25, 64, 82, 20, 30, 0, 0},      /* BLIP */
};
/* per kit: offsets to PITCH DECAY TONE, and absolute DRIVE / NOISE multipliers (percent) */
typedef struct { int8_t pitch, decay, tone; uint8_t drive, noise, bend; } kitmod_t;
static const kitmod_t KITMOD[MAXKITS] = {
    {0, 0, 0, 100, 100, 100},            /* MANTIS: the base */
    {-4, 10, -10, 60, 90, 80},           /* ANALOG: rounder, longer */
    {6, -10, 20, 40, 60, 160},           /* DIGITAL: brighter, snappier, more bend */
    {-8, 15, -5, 220, 120, 90},          /* INDUSTRY: driven, long */
    {14, -25, 25, 140, 70, 250},         /* GLITCH: short, pitched up, wild bends */
    {-2, 5, -15, 70, 50, 120},           /* TRIBAL: woody toms, less noise */
    {0, -15, -5, 30, 80, 60},            /* MINIMAL: tight, clean */
    {10, 20, 30, 90, 40, 200},           /* SAUCER: metallic, ringing, bendy */
};
static uint8_t clamp8(int32_t v) { return (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v); }
void cat_pads_default(void)
{
    uint32_t k, p;
    for (k = 0; k < MAXKITS; k++) for (p = 0; p < NPADS; p++) {
        const uint8_t *b = BASE[p]; const kitmod_t *m = &KITMOD[k]; uint8_t *q = pad_par[k][p];
        q[0] = clamp8(b[0] + m->pitch); q[1] = clamp8(b[1] + m->decay); q[2] = clamp8(b[2] + m->tone); q[3] = b[3];
        q[4] = clamp8((int32_t)b[4] * m->bend / 100); q[5] = b[5]; q[6] = clamp8((int32_t)b[6] * m->drive / 100); q[7] = clamp8((int32_t)b[7] * m->noise / 100);
        q[8] = (k == 2u || k == 4u || k == 7u) ? (uint8_t)(8u + p * 8u) : 0u;   /* DIGITAL, GLITCH and SAUCER come layered with the ABDUCT hits */
    }
}
uint8_t *cat_pad_blob(uint32_t *size) { *size = sizeof pad_par; return &pad_par[0][0][0]; }
const char *cat_pad_name(int pad) { return pad >= 0 && pad < NPADS ? VNAME[pad] : "-"; }
const char *cat_kit_name(void) { return KNAME[kit_i]; }
int cat_last_pad(void) { return last_pad; }
void cat_kit(int32_t step) { kit_i = (uint8_t)(((int32_t)kit_i + step + MAXKITS) % MAXKITS); }
int cat_kit_index(void) { return kit_i; }
void cat_kit_set(int k) { kit_i = (uint8_t)(k % MAXKITS); }
static void pad_view_load(void) { uint32_t i; for (i = 0; i < NPARAM; i++) { par1[i] = pad_par[kit_i][shown_pad][i]; par1b[i] = pad_par[kit_i][shown_pad][4 + i]; } par1c[0] = pad_par[kit_i][shown_pad][8]; }
static void pad_view_store(void) { uint32_t i; for (i = 0; i < NPARAM; i++) { pad_par[kit_i][shown_pad][i] = par1[i]; pad_par[kit_i][shown_pad][4 + i] = par1b[i]; } pad_par[kit_i][shown_pad][8] = par1c[0]; }
void cat_sync(void)
{
    if ((uint8_t)last_pad != shown_pad || shown_kit != kit_i) { shown_pad = (uint8_t)last_pad; shown_kit = kit_i; pad_view_load(); }
    else pad_view_store();
}

/* a voice: two oscillators (phase, freq, pitch envelope), a noise path through an SVF, envelopes */
typedef struct {
    float ph1, ph2, hz, hz_end, penv, pcoef, env, dcoef, nenv, ncoef, gain, drive, nmix, snap, snap_lp, tone, att;
    svf_t nf; svfc_t nfc; uint8_t on, kind, cyc, layer;
    adpcm_t a; float ainc, again;                                    /* the LAYER sample */
} voice_t;
static voice_t v[NPADS];
#define NTAIL 4
static voice_t tail[NTAIL]; static uint8_t tail_i;          /* retriggered voices fade out here instead of cutting */
static volatile uint8_t trig[NPADS], trig_vel[NPADS];
static float comp_env, comp_g = 1.0f, hp_x, hp_y;
static uint32_t rng = 99u;

static void m_init(void) { uint32_t i; for (i = 0; i < NPADS; i++) v[i].on = 0; comp_env = 0; pad_view_load(); }
static void m_panic(void) { m_init(); }
static void m_note(int32_t note, uint32_t vl, int on)
{
    if (!on || note < 0 || note >= NPADS) return;
    trig_vel[note] = (uint8_t)vl; trig[note] = 1; last_pad = (int8_t)note;
}

/* a voice's start: the pad's params turned into frequencies, times and mixes per voice kind */
static void start(uint32_t p, float vel)
{
    voice_t *w = &v[p];
    const uint8_t *q = pad_par[kit_i][p];
    float pitch = ((float)q[0] - 64.0f) * (24.0f / 127.0f) + ((float)par2[1] - 64.0f) * (12.0f / 127.0f);
    float dec = p01(q[1]) * p01(par2[2]) * 2.0f, tone = p01(q[2]), bend = p01(q[4]);
    float semi = fm_exp2f(pitch * (1.0f / 12.0f)), base_dec, base_hz, nhz;
    if (w->on) { tail[tail_i] = *w; tail[tail_i].dcoef = 0.97f; tail[tail_i].ncoef = 0.97f; tail[tail_i].snap = 0.0f; tail[tail_i].layer = 0; tail[tail_i].att = 1.0f; tail_i = (uint8_t)((tail_i + 1u) % NTAIL); }
    if (p == V_CHAT && v[V_OHAT].on) { tail[tail_i] = v[V_OHAT]; tail[tail_i].dcoef = 0.97f; tail[tail_i].ncoef = 0.97f; tail[tail_i].snap = 0.0f; tail[tail_i].layer = 0; tail_i = (uint8_t)((tail_i + 1u) % NTAIL); v[V_OHAT].on = 0; }   /* the closed hat chokes the open one, softly */
    tone = tone * (0.75f + 0.25f * vel);                              /* velocity brightens */
    w->kind = (uint8_t)p; w->tone = tone;
    w->gain = (0.25f + 0.75f * vel * vel) * p01(q[3]) * 1.1f;
    w->drive = p01(q[6]); w->nmix = p01(q[7]); w->snap = p01(q[5]) * 0.9f; w->snap_lp = 0.0f;
    w->ph1 = w->ph2 = 0.0f; w->env = 1.0f; w->nenv = 1.0f; w->cyc = 0; w->att = 0.0f;
    switch (p) {
    case V_KICK:  base_hz = 50.0f;  base_dec = 0.35f; w->penv = 2.0f + bend * 6.0f; w->pcoef = decay_coef(0.02f + tone * 0.03f); break;
    case V_KICK2: base_hz = 58.0f;  base_dec = 0.22f; w->penv = 3.0f + bend * 8.0f; w->pcoef = decay_coef(0.012f + tone * 0.02f); break;
    case V_SNARE: base_hz = 185.0f; base_dec = 0.18f; w->penv = 0.6f + bend * 2.0f; w->pcoef = decay_coef(0.01f); break;
    case V_RIM:   base_hz = 1700.0f; base_dec = 0.05f; w->penv = 0.0f; w->pcoef = 1.0f; break;
    case V_CLAP:  base_hz = 1100.0f; base_dec = 0.2f; w->penv = 0.0f; w->pcoef = 1.0f; break;
    case V_CHAT:  base_hz = 7000.0f; base_dec = 0.05f; w->penv = 0.0f; w->pcoef = 1.0f; break;
    case V_OHAT:  base_hz = 7000.0f; base_dec = 0.3f; w->penv = 0.0f; w->pcoef = 1.0f; break;
    case V_CYM:   base_hz = 5500.0f; base_dec = 0.9f; w->penv = 0.0f; w->pcoef = 1.0f; break;
    case V_LTOM:  base_hz = 95.0f;  base_dec = 0.3f; w->penv = 0.8f + bend * 2.0f; w->pcoef = decay_coef(0.04f); break;
    case V_MTOM:  base_hz = 140.0f; base_dec = 0.26f; w->penv = 0.8f + bend * 2.0f; w->pcoef = decay_coef(0.035f); break;
    case V_HTOM:  base_hz = 200.0f; base_dec = 0.22f; w->penv = 0.8f + bend * 2.0f; w->pcoef = decay_coef(0.03f); break;
    case V_BELL:  base_hz = 560.0f; base_dec = 0.5f; w->penv = 0.0f; w->pcoef = 1.0f; break;
    case V_ZAP:   base_hz = 300.0f; base_dec = 0.15f; w->penv = 6.0f + bend * 20.0f; w->pcoef = decay_coef(0.02f + tone * 0.05f); break;
    case V_CLICK: base_hz = 2500.0f; base_dec = 0.02f; w->penv = 0.0f; w->pcoef = 1.0f; break;
    case V_METAL: base_hz = 800.0f; base_dec = 0.35f; w->penv = 0.3f * bend; w->pcoef = decay_coef(0.05f); break;
    default:      base_hz = 880.0f; base_dec = 0.08f; w->penv = 1.0f * bend; w->pcoef = decay_coef(0.01f); break;   /* BLIP */
    }
    w->layer = 0;
    if (q[8]) {                                                       /* LAYER: an ABDUCT hit under the voice */
        uint32_t ki, idx = ((uint32_t)q[8] - 1u) * 16u / 127u;
        for (ki = 0; ki < SMPKIT_N; ki++) if (!SMPKIT[ki].is808) {
            const smp_t *sm = &SMP[SMPKIT[ki].first + (idx < SMPKIT[ki].n ? idx : SMPKIT[ki].n - 1u)];
            adpcm_start(&w->a, sm); w->ainc = semi * ((float)sm->rate / SR); w->again = 0.8f; w->layer = 1; break;
        }
    }
    w->hz_end = base_hz * semi;
    w->hz = w->hz_end;
    w->dcoef = decay_coef(0.01f + base_dec * dec * dec * 1.6f);
    w->ncoef = decay_coef(0.005f + base_dec * dec * 0.8f);
    nhz = p == V_SNARE ? 1800.0f + tone * 4000.0f : p == V_CLAP ? 900.0f + tone * 1800.0f : p == V_CHAT || p == V_OHAT ? 7000.0f + tone * 6000.0f
        : p == V_CYM ? 4000.0f + tone * 5000.0f : p == V_CLICK ? 2500.0f + tone * 6000.0f : 1000.0f + tone * 5000.0f;
    svf_coef(&w->nfc, nhz, p == V_CLAP ? 0.75f : p == V_RIM ? 0.9f : 0.5f);
    w->nf.ic1 = w->nf.ic2 = 0.0f;
    w->on = 1;
}

static inline float tri_of(float ph) { return ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f; }

static void m_render(float *out, uint32_t n)
{
    uint32_t i, p;
    float comp = p01(par3[0]), punch = p01(par3[1]) * 1.5f, lowcut = p01(par3[2]), drive = p01(par3[3]);
    float level = p01(par2[3]);
    float hpc = lowcut > 0.0f ? 1.0f - fm_exp2f(-6.2831853f * (20.0f + lowcut * 200.0f) / SR) : 0.0f;
    float thr = 1.0f - comp * 0.6f, slope = comp * 0.6f, makeup = 1.0f + comp * 0.35f;
    float dgain = 1.0f + drive * drive * 4.0f, dnorm = drive > 0.0f ? 1.0f / fm_soft(dgain * 0.7f) : 1.0f;
    for (p = 0; p < NPADS; p++) if (trig[p]) { trig[p] = 0; start(p, (float)trig_vel[p] * (1.0f / 127.0f)); }
    for (i = 0; i < n; i++) {
        float s = 0.0f, g;
        for (p = 0; p < NPADS + NTAIL; p++) {
            voice_t *w = p < NPADS ? &v[p] : &tail[p - NPADS];
            float x, tonal = 0.0f, nz, dt;
            if (!w->on) continue;
            w->penv *= w->pcoef;
            dt = w->hz_end * (1.0f + w->penv) / SR;
            if (dt > 0.45f) dt = 0.45f;
            w->ph1 += dt; if (w->ph1 >= 1.0f) w->ph1 -= 1.0f;
            switch (w->kind) {
            case V_KICK: case V_KICK2: case V_LTOM: case V_MTOM: case V_HTOM: case V_ZAP: case V_BLIP:
                tonal = fm_sinf(6.2831853f * w->ph1);                                           /* a sine body, saturated */
                if (w->kind <= V_KICK2) tonal += fm_sinf(12.566371f * w->ph1) * 0.25f * w->env * w->env;   /* a 2nd harmonic that dies first: definition */
                tonal = fm_soft(tonal * (1.0f + w->tone * 2.5f + w->drive * 4.0f));
                if (w->kind >= V_LTOM && w->kind <= V_HTOM) tonal = tonal * 0.8f + tri_of(w->ph1) * 0.3f;
                break;
            case V_SNARE:
                tonal = fm_sinf(6.2831853f * w->ph1) * 0.7f;
                w->ph2 += dt * 1.62f; if (w->ph2 >= 1.0f) w->ph2 -= 1.0f;
                tonal += fm_sinf(6.2831853f * w->ph2) * 0.45f;
                break;
            case V_BELL: case V_METAL: {                                                        /* FM: an inharmonic pair */
                float r = w->kind == V_BELL ? 3.47f : 2.76f;
                w->ph2 += dt * r; if (w->ph2 >= 1.0f) w->ph2 -= 1.0f;
                tonal = fm_sinf(6.2831853f * (w->ph1 + fm_sinf(6.2831853f * w->ph2) * (0.3f + w->tone * 1.6f) * w->env));
                if (w->kind == V_METAL) tonal = fm_soft(tonal * (1.5f + w->drive * 3.0f));
                break; }
            case V_RIM: case V_CLICK:
                tonal = (w->ph1 < 0.5f ? 1.0f : -1.0f) * (w->kind == V_RIM ? 0.6f : 0.3f);     /* a tiny square, filtered by the env */
                break;
            default: break;                                                                     /* hats, cymbal, clap: noise */
            }
            nz = noise(&rng);
            if (w->kind == V_CHAT || w->kind == V_OHAT || w->kind == V_CYM) {                   /* metallic: six squares' worth, cheaply */
                w->ph2 += (w->hz_end * 0.173f) / SR; if (w->ph2 >= 1.0f) w->ph2 -= 1.0f;
                nz = nz * 0.6f + ((w->ph2 < 0.5f) ? 0.4f : -0.4f) * fm_sinf(6.2831853f * w->ph1 * 1.0f);
            }
            if (w->kind == V_CLAP) {                                                            /* three bursts then the tail */
                float tt = 1.0f - w->nenv;
                if (tt < 0.035f && ((int32_t)(tt * 260.0f) & 1)) nz *= 0.05f;
            }
            nz = svf_bp(&w->nf, &w->nfc, nz) * 2.2f;
            x = tonal * w->env * (1.0f - w->nmix * 0.6f) + nz * w->nenv * w->nmix * (w->kind >= V_CLAP && w->kind <= V_CYM ? 1.4f : 1.0f);
            if (w->snap > 0.0f && w->env > 0.93f) {                                              /* SNAP: a bright tick on the attack */
                float t2 = noise(&rng); w->snap_lp += (t2 - w->snap_lp) * 0.35f;
                x += (t2 - w->snap_lp) * w->snap * (w->env - 0.93f) * 14.0f;
            }
            if (w->layer) { x += adpcm_read(&w->a, w->ainc) * w->again * w->env * 1.2f; if (adpcm_done(&w->a)) w->layer = 0; }
            if (w->drive > 0.0f && w->kind != V_METAL) x = fm_soft(x * (1.0f + w->drive * 3.0f)) * (1.0f / (1.0f + w->drive * 0.8f));
            x *= w->gain;
            if (w->att < 1.0f) { w->att += 0.09f; if (w->att > 1.0f) w->att = 1.0f; x *= w->att; }   /* a 0.25 ms onset ramp: a noise or snap burst never starts as a one-sample step */
            if (w->env > 0.8f) x *= 1.0f + punch * (w->env - 0.8f) * 5.0f;                      /* the bus PUNCH on every attack */
            w->env *= w->dcoef; w->nenv *= w->ncoef;
            s += x;
            if (w->env < 0.0008f && w->nenv < 0.0008f && !w->layer) w->on = 0;
        }
        if (hpc > 0.0f) { float y = s - hp_x + (1.0f - hpc) * hp_y; hp_x = s; hp_y = y; s = y; }
        {
            float a = fm_fabsf(s);
            comp_env += (a > comp_env ? 0.4f : 0.0002f) * (a - comp_env);
            if ((i & 7u) == 0u) comp_g = comp_env > thr ? fm_powf(comp_env / thr, -slope) : 1.0f;
            g = comp_g; s *= g * makeup;
        }
        if (drive > 0.0f) s = fm_soft(s * 0.7f * dgain) * dnorm;
        s *= level;
        if (s > 1.2f) s = 1.2f; else if (s < -1.2f) s = -1.2f;
        out[i] += s;
    }
}

static const char *m_text(uint8_t page, uint32_t p, char *buf)
{
    if (page == 3u && p == 0u) return cat_kit_name();
    if (page == 2u && p == 0u) { if (!par1c[0]) return "OFF"; { uint32_t ki, idx = ((uint32_t)par1c[0] - 1u) * 16u / 127u; for (ki = 0; ki < SMPKIT_N; ki++) if (!SMPKIT[ki].is808) return SMP[SMPKIT[ki].first + (idx < SMPKIT[ki].n ? idx : SMPKIT[ki].n - 1u)].name; } return "-"; }
    if (page == 2u) return "-";
    if (page == 0u && p == 0u) { int32_t st = ((int32_t)par1[0] - 64) * 24 / 127; buf[0] = st < 0 ? '-' : '+'; st = st < 0 ? -st : st; buf[1] = (char)('0' + st / 10); buf[2] = (char)('0' + st % 10); buf[3] = 0; return buf; }
    if (page == 1u && p > 0u && par1b[p] == 0u) return "OFF";
    if (page == 4u && (p == 2u || p == 3u) && (p == 2u ? par3[2] : par3[3]) == 0u) return "OFF";
    return 0;
}

static const char *const TITLES[5] = {"VOICE", "VOICE+", "LAYER", "KIT", "BUS"};
const engine_t cat = {"MANTIS", m_init, m_note, m_render, m_panic, 5, {NAMES1, NAMES1B, NAMES1C, NAMES2, NAMES3}, {par1, par1b, par1c, par2, par3}, m_text, TITLES, "mantis", 1};
