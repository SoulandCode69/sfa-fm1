/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * FOX BASS & 808: the low end. The ALGO knob browses 16 factory 808 / sub / bass one-shots (played chromatically from
 * their root, mono with glide) and 8 synth bass presets (a mono 303 / sub / reese synth). Clean: a soft ceiling only.
 *   808 pages: FILTER DECAY GLIDE DRIVE · ATTACK RELEASE SUB PITCHENV
 *   BASS pages: CUTOFF RES ENVMOD DECAY · WAVE SUB GLIDE DRIVE · LFO WOBBLE ACCENT RELEASE */
#include "fastmath.h"
#include "engine.h"
#include "../samples.h"

static const char *const EN1[NPARAM] = {"FILTER", "DECAY", "GLIDE", "DRIVE"};
static const char *const EN2[NPARAM] = {"ATTACK", "RELEASE", "SUB", "PITCHENV"};
static const char *const EN3[NPARAM] = {"TONE", "CLICK", "PUNCH", "-"};
static const char *const BN1[NPARAM] = {"CUTOFF", "RES", "ENVMOD", "DECAY"};
static const char *const BN2[NPARAM] = {"WAVE", "SUB", "GLIDE", "DRIVE"};
static const char *const BN3[NPARAM] = {"LFO", "WOBBLE", "ACCENT", "RELEASE"};
static const char *NAMES1[NPARAM], *NAMES2[NPARAM], *NAMES3[NPARAM];
static uint8_t par1[NPARAM], par2[NPARAM], par3[NPARAM];
static uint8_t e_par[3][NPARAM] = {{127, 105, 10, 0}, {0, 20, 0, 0}, {64, 30, 35, 0}};        /* 808 mode's own */
static uint8_t b_par[3][NPARAM] = {{40, 70, 80, 45}, {0, 60, 20, 30}, {30, 0, 60, 20}};    /* bass mode's own */
static uint8_t mode;                                     /* 0 = 808 sample, 1 = bass synth */
static const char *TITLES[3];
static const char *const ET[3] = {"PLAY", "SHAPE", "TONE"}, *const BT[3] = {"FILTER", "OSCILLATOR", "MODULATION"};
static uint8_t preset_i;                                 /* 0..15 the 808 bank, 16..23 the synth presets */

typedef struct { const char *name; uint8_t p[3][NPARAM]; } bpreset_t;
#define NBPRESET 24
static const bpreset_t BPRESETS[NBPRESET] = {
    /*                CUT RES ENV DEC    WAVE SUB GLI DRV   LFO WOB ACC REL */
    {"REPTILE ACID", {{36, 90, 100, 40}, {0, 0, 25, 55}, {30, 0, 100, 15}}},
    {"SCALE SUB", {{15, 5, 30, 110}, {127, 127, 15, 20}, {0, 0, 30, 60}}},
    {"VENOM", {{48, 70, 50, 70}, {0, 40, 10, 120}, {90, 100, 60, 25}}},
    {"LIZARD SQR", {{60, 45, 110, 30}, {127, 30, 0, 35}, {25, 0, 80, 10}}},
    {"CAVE RUMBLE", {{25, 20, 40, 95}, {0, 100, 5, 70}, {15, 0, 20, 40}}},
    {"TONGUE", {{70, 95, 120, 20}, {127, 20, 0, 50}, {30, 0, 90, 5}}},
    {"COLD BLOOD", {{18, 0, 10, 127}, {127, 127, 30, 15}, {10, 0, 10, 50}}},
    {"SWAMP", {{40, 55, 85, 40}, {0, 70, 12, 45}, {35, 20, 60, 15}}},
    {"RAZOR SAW", {{55, 40, 50, 60}, {0, 40, 0, 127}, {40, 0, 70, 20}}},
    {"CRAWL", {{30, 20, 5, 100}, {0, 60, 0, 70}, {8, 25, 20, 50}}},
    {"MOLT", {{35, 50, 60, 55}, {0, 85, 15, 40}, {20, 0, 50, 20}}},
    {"HISS SQUELCH", {{45, 100, 110, 35}, {127, 10, 30, 45}, {30, 0, 100, 10}}},
    {"DEEP NEST", {{15, 0, 15, 100}, {127, 127, 25, 10}, {10, 0, 10, 60}}},
    {"ELECTRO SCALE", {{65, 35, 70, 40}, {127, 40, 5, 60}, {25, 0, 70, 15}}},
    {"SWEEP TAIL", {{25, 65, 120, 90}, {0, 50, 0, 35}, {40, 60, 30, 40}}},
    {"PSY LIZARD", {{40, 75, 95, 20}, {0, 30, 0, 70}, {30, 0, 90, 5}}},
    {"BASILISK", {{50, 60, 80, 80}, {0, 50, 40, 90}, {60, 80, 70, 30}}},
    {"STRIKE", {{75, 50, 120, 15}, {0, 20, 0, 60}, {0, 0, 100, 5}}},
    {"SLITHER", {{30, 45, 30, 70}, {127, 80, 60, 30}, {45, 40, 40, 35}}},
    {"REESE SCALE", {{28, 30, 20, 90}, {0, 55, 0, 80}, {10, 30, 30, 45}}},
    {"WOBBLE NEST", {{35, 55, 40, 85}, {0, 60, 0, 60}, {100, 110, 40, 40}}},
    {"CLAW", {{85, 70, 110, 20}, {127, 0, 0, 80}, {20, 0, 100, 8}}},
    {"UNDERGROUND", {{20, 25, 35, 110}, {0, 110, 8, 50}, {15, 10, 30, 55}}},
    {"REPTILE LEAD", {{95, 45, 60, 50}, {0, 0, 20, 70}, {50, 20, 80, 30}}},
};

/* the 808 bank among the sample kits */
static const smpkit_t *bank(void) { uint32_t i; for (i = 0; i < SMPKIT_N; i++) if (SMPKIT[i].is808) return &SMPKIT[i]; return &SMPKIT[0]; }
static uint32_t npre(void) { return bank()->n + NBPRESET; }
static const smp_t *cur_smp(void) { const smpkit_t *b = bank(); return preset_i < b->n ? &SMP[b->first + preset_i] : &SMP[b->first]; }

static void mode_apply(void)
{
    uint32_t g, p;
    const char *const *n1 = mode ? BN1 : EN1, *const *n2 = mode ? BN2 : EN2, *const *n3 = mode ? BN3 : EN3;
    uint8_t (*src)[NPARAM] = mode ? b_par : e_par;
    for (p = 0; p < NPARAM; p++) { NAMES1[p] = n1[p]; NAMES2[p] = n2[p]; NAMES3[p] = n3[p]; }
    for (p = 0; p < 3u; p++) TITLES[p] = mode ? BT[p] : ET[p];
    for (g = 0; g < 3u; g++) for (p = 0; p < NPARAM; p++) (g == 0 ? par1 : g == 1 ? par2 : par3)[p] = src[g][p];
}
static void mode_stash(void)
{
    uint32_t g, p;
    uint8_t (*dst)[NPARAM] = mode ? b_par : e_par;
    for (g = 0; g < 3u; g++) for (p = 0; p < NPARAM; p++) dst[g][p] = (g == 0 ? par1 : g == 1 ? par2 : par3)[p];
}
void fox_preset(int32_t step)
{
    uint32_t g, p, n = npre();
    mode_stash();
    preset_i = (uint8_t)(((int32_t)preset_i + step + (int32_t)n) % (int32_t)n);
    mode = preset_i >= bank()->n;
    if (mode) for (g = 0; g < 3u; g++) for (p = 0; p < NPARAM; p++) b_par[g][p] = BPRESETS[preset_i - bank()->n].p[g][p];
    mode_apply();
}
const char *fox_preset_name(void) { return mode ? BPRESETS[preset_i - bank()->n].name : cur_smp()->name; }
int fox_mode(void) { return mode; }
int fox_preset_index(void) { return preset_i; }
void fox_preset_set(int i) { mode_stash(); preset_i = (uint8_t)((uint32_t)i % npre()); mode = preset_i >= bank()->n; mode_apply(); }
void fox_mode_set_params(void) { mode_stash(); }

/* ---- the 808 player: mono, legato glide, a decay on top of the sample's own, a lowpass, a sub sine under it */
static struct {
    adpcm_t a; float inc, inc_target, env, aenv, sub_ph, pitch_env;
    int32_t held[8]; uint8_t nheld; int32_t note; uint8_t gate, on;
    svf_t flt; uint32_t rng; float click_lp;
} e8;

static void e8_note(int32_t note, uint32_t vel, int on)
{
    uint32_t i, j;
    const smp_t *s = cur_smp();
    float target;
    (void)vel;
    if (on) {
        for (i = 0; i < e8.nheld; i++) if (e8.held[i] == note) break;
        if (i == e8.nheld && e8.nheld < 8u) e8.held[e8.nheld++] = note;
        target = fm_exp2f((float)(note - s->root) * (1.0f / 12.0f)) * ((float)s->rate / SR);
        if (!e8.gate || e8.nheld == 1u) {                               /* a new phrase: start the sample */
            adpcm_start(&e8.a, s); e8.inc = target; e8.env = 1.0f; e8.aenv = par2[0] ? 0.0f : 1.0f; e8.on = 1; e8.pitch_env = 1.0f;
        }
        e8.inc_target = target; e8.gate = 1; e8.note = note;
    } else {
        for (i = 0; i < e8.nheld; i++) if (e8.held[i] == note) { for (j = i; j + 1u < e8.nheld; j++) e8.held[j] = e8.held[j + 1]; e8.nheld--; break; }
        if (e8.nheld) { e8.note = e8.held[e8.nheld - 1]; e8.inc_target = fm_exp2f((float)(e8.note - s->root) * (1.0f / 12.0f)) * ((float)s->rate / SR); }
        else e8.gate = 0;
    }
}

static void e8_render(float *out, uint32_t n)
{
    uint32_t i;
    float filt = p01(par1[0]), dec = p01(par1[1]), glide = par1[2] ? 0.03f * fm_exp2f(-p01(par1[2]) * 6.0f) : 1.0f;
    float drive = p01(par1[3]), dgain = 1.0f + drive * drive * 5.0f, dnorm = drive > 0.0f ? 1.0f / fm_soft(dgain * 0.7f) : 1.0f;
    float att = par2[0] ? 1.0f / ((0.002f + p01(par2[0]) * 0.3f) * SR) : 1.0f, rel = decay_coef(0.004f + p01(par2[1]) * 0.6f);
    float sub = p01(par2[2]) * 0.6f, penv = p01(par2[3]) * 1.5f, tone = p01(par3[0]), click = p01(par3[1]) * 0.8f, punch = p01(par3[2]) * 1.6f;
    float dcoef = dec >= 0.99f ? 1.0f : decay_coef(0.02f + dec * dec * 1.5f);
    svfc_t fc;
    int filt_on = filt < 0.99f;
    if (!e8.on) return;
    if (filt_on) svf_coef(&fc, 60.0f * fm_exp2f(filt * 7.6f), 0.15f + tone * 0.5f);          /* 60 Hz .. 12 kHz */
    e8.inc += (e8.inc_target - e8.inc) * (glide >= 1.0f ? 1.0f : glide * (float)n * 0.1f);
    for (i = 0; i < n; i++) {
        float x;
        e8.pitch_env *= 0.9994f;                                        /* PITCHENV: a quick drop at the start */
        x = adpcm_read(&e8.a, e8.inc * (1.0f + penv * e8.pitch_env));
        if (sub > 0.0f) {                                               /* a sine under it at the played note */
            float hz = 440.0f * fm_exp2f((float)(e8.note - 69) * (1.0f / 12.0f));
            e8.sub_ph += hz * (1.0f / SR); if (e8.sub_ph >= 1.0f) e8.sub_ph -= 1.0f;
            x += fm_sinf(e8.sub_ph * 6.2831853f) * sub * e8.env;
        }
        if (filt_on) x = svf_lp(&e8.flt, &fc, x);
        if (e8.a.pos < 1800.0f) {                                       /* the first 40 ms: PUNCH lifts it, CLICK adds the beater */
            float lift = 1.0f - e8.a.pos * (1.0f / 1800.0f);
            x *= 1.0f + punch * lift * lift;
            if (click > 0.0f && e8.a.pos < 180.0f) {                            /* CLICK: a 4 ms beater knock (lowpassed noise) */
                e8.click_lp += (noise(&e8.rng) - e8.click_lp) * 0.22f;
                x += e8.click_lp * click * 1.6f * (1.0f - e8.a.pos * (1.0f / 180.0f));
            }
        }
        if (e8.gate) { if (e8.aenv < 1.0f) { e8.aenv += att; if (e8.aenv > 1.0f) e8.aenv = 1.0f; } } else e8.aenv *= rel;
        e8.env *= dcoef;
        x *= e8.env * e8.aenv;
        if (drive > 0.0f) x = fm_soft(x * 0.7f * dgain) * dnorm;
        out[i] += x * 1.1f;
        if ((adpcm_done(&e8.a) && sub <= 0.0f) || e8.aenv < 0.0005f || e8.env < 0.0005f) { e8.on = 0; break; }
    }
}

/* ---- the bass synth */
static struct {
    float hz, hz_target, ph, phs, env, aenv, lfo_ph, sm_cut;
    int32_t held[8]; uint8_t nheld;
    int32_t note; uint8_t gate, accent;
    svf_t flt;
} bs;

static void bass_note(int32_t note, uint32_t vel, int on)
{
    uint32_t i, j;
    if (on) {
        for (i = 0; i < bs.nheld; i++) if (bs.held[i] == note) break;
        if (i == bs.nheld && bs.nheld < 8u) bs.held[bs.nheld++] = note;
        bs.hz_target = note_hz(note);
        if (!bs.gate || bs.nheld == 1u) { bs.hz = bs.hz_target; bs.env = 1.0f; bs.aenv = 0.0f; }
        bs.gate = 1; bs.note = note;
        bs.accent = vel > 100u;
    } else {
        for (i = 0; i < bs.nheld; i++) if (bs.held[i] == note) { for (j = i; j + 1u < bs.nheld; j++) bs.held[j] = bs.held[j + 1]; bs.nheld--; break; }
        if (bs.nheld) { bs.note = bs.held[bs.nheld - 1]; bs.hz_target = note_hz(bs.note); }
        else bs.gate = 0;
    }
}

static void bass_render(float *out, uint32_t n)
{
    uint32_t i;
    float cut = p01(par1[0]), res = p01(par1[1]) * 0.97f, envmod = p01(par1[2]), dec = decay_coef(0.03f + p01(par1[3]) * p01(par1[3]) * 2.0f);
    int sqr = par2[0] >= 64; float sub = p01(par2[1]) * 0.8f, glide = par2[2] ? 0.03f * fm_exp2f(-p01(par2[2]) * 6.0f) : 1.0f;
    float drive = 1.0f + p01(par2[3]) * p01(par2[3]) * 8.0f, dnorm = 1.0f / fm_soft(drive * 0.7f);
    float lfo_inc = (0.2f + p01(par3[0]) * p01(par3[0]) * 14.0f) / SR, wob = p01(par3[1]), acc = p01(par3[2]);
    float rel = decay_coef(0.005f + p01(par3[3]) * 0.4f);
    float lfo, dt, dts, fbase, accg = bs.accent ? 1.0f + acc : 1.0f;
    svfc_t fc;
    if (!bs.gate && bs.aenv < 0.0005f) return;
    bs.sm_cut += (cut - bs.sm_cut) * 0.3f;
    lfo = fm_sinf(bs.lfo_ph * 6.2831853f); bs.lfo_ph += lfo_inc * (float)n; if (bs.lfo_ph >= 1.0f) bs.lfo_ph -= 1.0f;
    bs.hz += (bs.hz_target - bs.hz) * (glide >= 1.0f ? 1.0f : glide * (float)n * 0.1f);
    dt = bs.hz * (1.0f / SR); dts = dt * 0.5f;
    fbase = 60.0f * fm_exp2f(bs.sm_cut * 7.0f) * (1.0f + lfo * wob * 0.9f);
    for (i = 0; i < n; i++) {
        float o, s;
        if (bs.gate) bs.aenv += (1.0f - bs.aenv) * 0.02f; else bs.aenv *= rel;
        bs.env *= dec;
        bs.ph += dt; if (bs.ph >= 1.0f) bs.ph -= 1.0f;
        bs.phs += dts; if (bs.phs >= 1.0f) bs.phs -= 1.0f;
        if (sqr) { float t2 = bs.ph + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f; o = (bs.ph < 0.5f ? 1.0f : -1.0f) + polyblep(bs.ph, dt) - polyblep(t2, dt); }
        else o = 2.0f * bs.ph - 1.0f - polyblep(bs.ph, dt);
        o = o * 0.8f + fm_sinf(bs.phs * 6.2831853f) * sub;
        if ((i & 15u) == 0u) svf_coef(&fc, fbase * (1.0f + envmod * envmod * 24.0f * bs.env * accg), res);
        s = svf_lp(&bs.flt, &fc, o);
        s = fm_soft(s * (1.0f + res * 0.8f) * 0.7f * drive) * dnorm;
        out[i] += s * bs.aenv * (bs.accent ? 1.0f + acc * 0.4f : 1.0f) * 0.6f;
    }
    if (!bs.gate && bs.aenv < 0.0005f) bs.flt.ic1 = bs.flt.ic2 = 0;
}

/* ---- the engine */
static volatile int32_t q_note[8]; static volatile uint8_t q_vel[8], q_on[8], q_n;
static void f_init(void) { e8.on = 0; e8.gate = 0; e8.nheld = 0; bs.gate = 0; bs.nheld = 0; bs.aenv = 0; q_n = 0; mode_apply(); }
static void f_panic(void) { e8.on = 0; e8.gate = 0; e8.nheld = 0; bs.gate = 0; bs.nheld = 0; bs.aenv = 0; }
static void f_note(int32_t note, uint32_t vel, int on)
{
    uint8_t k = q_n; if (k >= 8u) return;
    q_note[k] = note; q_vel[k] = (uint8_t)vel; q_on[k] = (uint8_t)on; q_n = (uint8_t)(k + 1u);
}
static void f_render(float *out, uint32_t n)
{
    while (q_n) { q_n--; if (mode) bass_note(q_note[q_n], q_vel[q_n], q_on[q_n]); else e8_note(q_note[q_n], q_vel[q_n], q_on[q_n]); }
    if (mode) bass_render(out, n); else e8_render(out, n);
}
static const char *f_text(uint8_t page, uint32_t p, char *buf)
{
    (void)buf;
    if (mode) return page == 1u && p == 0u ? (par2[0] >= 64u ? "SQUARE" : "SAW") : 0;
    if (page == 0u && p == 0u && par1[0] >= 126u) return "OPEN";
    if (page == 0u && p == 1u && par1[1] >= 126u) return "FULL";
    if (page == 0u && p == 3u && par1[3] == 0u) return "OFF";
    return 0;
}
const engine_t fox = {"REPTILES", f_init, f_note, f_render, f_panic, 3, {NAMES1, NAMES2, NAMES3, 0, 0}, {par1, par2, par3, 0, 0}, f_text, TITLES};
