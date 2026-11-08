/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * SFA ONE: the engine. See sfa_one.h for the voice and the pages. Portable: no libm, no allocation, no globals. */
#include "sfa_one.h"

#define TWO_PI 6.2831853f
static inline float p01(uint8_t p) { return (float)p * (1.0f / 127.0f); }
static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float soft(float x)                                   /* a Padé tanh */
{
    float x2; if (x > 3.0f) x = 3.0f; else if (x < -3.0f) x = -3.0f;
    x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float noise(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return (float)(int32_t)*s * (1.0f / 2147483648.0f); }
static inline float note_hz(const one_t *o, int32_t note) { return 440.0f * o->host.exp2f_((float)(note - 69) * (1.0f / 12.0f)); }
static inline float decay_coef(const one_t *o, float sec) { return o->host.exp2f_(-1.4427f / (sec * o->host.sr)); }

const char *const ONE_NAMES[ONE_PAGES][ONE_NPARAM] = {
    {"TONE", "BODY", "COLOR", "MOTION"}, {"SHAPE", "NOISE", "BURST", "PITCHENV"}, {"DECAY", "DAMP", "SPREAD", "MIX"},
    {"CUTOFF", "RES", "FOLD", "ENVAMT"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"}, {"LFO1", "L1>PITCH", "LFO2", "L2>FILT"},
    {"GLIDE", "UNISON", "SUB", "LEVEL"}};
static const char *const TITLES[ONE_PAGES] = {"MACROS", "EXCITER", "RESONATOR", "FILTER", "ENVELOPE", "MOTION", "VOICE"};
const char *one_page_title(uint32_t page) { return TITLES[page < ONE_PAGES ? page : 0]; }
const char *const *one_page_names(uint32_t page) { return ONE_NAMES[page < ONE_PAGES ? page : 0]; }

/* the presets: MACROS | EXCITER | RESONATOR | FILTER | ENVELOPE | MOTION | VOICE */
const one_preset_t ONE_PRESETS[] = {
    {"FIRST LIGHT",   {{64, 64, 64, 40}, {30, 10, 60, 20}, {70, 40, 10, 90}, {90, 20, 0, 40}, {0, 60, 40, 50}, {20, 5, 30, 10}, {0, 20, 30, 100}}},
    {"PLUCK",         {{60, 50, 40, 20}, {20, 25, 90, 10}, {55, 55, 5, 110}, {100, 10, 0, 30}, {0, 45, 0, 35}, {0, 0, 0, 0}, {0, 10, 20, 100}}},
    {"BOWED",         {{50, 80, 50, 50}, {45, 40, 20, 0}, {95, 30, 8, 120}, {70, 25, 0, 20}, {40, 60, 100, 60}, {35, 12, 40, 20}, {15, 30, 0, 100}}},
    {"GLASS BAR",     {{80, 60, 70, 30}, {10, 15, 100, 40}, {80, 20, 60, 115}, {110, 15, 10, 50}, {0, 70, 10, 70}, {0, 0, 20, 10}, {0, 0, 0, 100}}},
    {"BREATH PIPE",   {{40, 70, 30, 60}, {25, 70, 10, 5}, {85, 60, 3, 100}, {60, 30, 0, 20}, {30, 50, 100, 45}, {50, 8, 30, 25}, {10, 15, 10, 100}}},
    {"SFA BASS",      {{45, 40, 60, 10}, {70, 5, 40, 30}, {40, 70, 0, 50}, {55, 45, 40, 70}, {0, 55, 60, 25}, {0, 0, 0, 0}, {10, 25, 100, 100}}},
    {"FOLDED KEYS",   {{75, 45, 95, 25}, {55, 10, 50, 15}, {50, 50, 30, 70}, {95, 20, 80, 40}, {0, 65, 30, 45}, {0, 0, 25, 15}, {0, 40, 30, 100}}},
    {"CHORD CLOUD",   {{55, 90, 55, 70}, {35, 30, 20, 0}, {100, 25, 95, 120}, {65, 20, 0, 10}, {70, 80, 100, 100}, {15, 6, 20, 35}, {0, 60, 10, 100}}},
    {"STRIKE",        {{90, 30, 50, 15}, {5, 40, 120, 60}, {30, 80, 15, 90}, {120, 25, 20, 60}, {0, 35, 0, 20}, {0, 0, 0, 0}, {0, 0, 40, 100}}},
    {"SINGING",       {{50, 75, 40, 45}, {40, 20, 10, 0}, {90, 35, 12, 110}, {75, 35, 0, 30}, {25, 60, 100, 70}, {45, 14, 0, 0}, {30, 20, 0, 100}}},
    {"DEEP BELL",     {{70, 70, 60, 20}, {15, 10, 100, 35}, {100, 15, 70, 120}, {90, 10, 10, 30}, {0, 90, 20, 110}, {0, 0, 15, 8}, {0, 0, 50, 100}}},
    {"REED",          {{45, 60, 75, 40}, {75, 25, 15, 0}, {75, 45, 5, 80}, {60, 40, 30, 45}, {15, 50, 100, 40}, {40, 10, 50, 30}, {5, 0, 0, 100}}},
    {"TENSION PAD",   {{35, 85, 45, 90}, {50, 35, 25, 0}, {110, 20, 40, 115}, {50, 30, 10, 20}, {90, 90, 100, 120}, {10, 10, 25, 60}, {0, 70, 20, 100}}},
    {"KALIMBA ONE",   {{85, 35, 30, 10}, {12, 20, 110, 25}, {60, 60, 2, 110}, {115, 10, 0, 25}, {0, 40, 0, 30}, {0, 0, 0, 0}, {0, 0, 0, 100}}},
    {"WIRE",          {{65, 55, 85, 35}, {60, 15, 70, 20}, {65, 35, 20, 100}, {105, 30, 70, 50}, {0, 55, 20, 40}, {0, 0, 40, 20}, {0, 30, 10, 100}}},
    {"SUB ONE",       {{30, 20, 20, 0}, {20, 0, 20, 10}, {20, 90, 0, 20}, {40, 20, 0, 40}, {0, 60, 90, 30}, {0, 0, 0, 0}, {15, 0, 127, 100}}},
};
const uint32_t ONE_NPRESETS = sizeof ONE_PRESETS / sizeof *ONE_PRESETS;

void one_preset_load(one_t *o, uint32_t i)
{
    uint32_t a, b;
    if (i >= ONE_NPRESETS) i = 0;
    for (a = 0; a < ONE_PAGES; a++) for (b = 0; b < ONE_NPARAM; b++) o->p[a][b] = ONE_PRESETS[i].p[a][b];
}

void one_init(one_t *o, const one_host_t *host)
{
    uint32_t k;
    o->host = *host;
    for (k = 0; k < ONE_VOICES; k++) { one_voice_t *v = &o->v[k]; uint32_t i, c; v->on = 0; v->rng = 0x1234567u * (k + 1u); for (c = 0; c < ONE_COMBS; c++) { for (i = 0; i < ONE_COMB_LEN; i++) v->buf[c][i] = 0; v->lp[c] = 0.0f; } v->w = 0; v->hz = 110.0f; }
    o->age_clock = 0; o->lfo1_ph = o->lfo2_ph = 0.0f; o->out_gain = 1.0f; o->last_note = -1;
    one_preset_load(o, 0);
}
void one_panic(one_t *o) { uint32_t k; for (k = 0; k < ONE_VOICES; k++) o->v[k].on = 0; }

void one_note(one_t *o, int32_t note, uint32_t vel, int on)
{
    uint32_t i, pick = 0;
    if (note < 0 || note > 127) return;
    if (!on) { for (i = 0; i < ONE_VOICES; i++) if (o->v[i].on && o->v[i].note == note && o->v[i].gate) o->v[i].gate = 0; return; }
    for (i = 0; i < ONE_VOICES; i++) if (!o->v[i].on) { pick = i; break; }
    if (i == ONE_VOICES) { for (i = 0; i < ONE_VOICES; i++) if (o->v[i].age < o->v[pick].age) pick = i; }
    {
        one_voice_t *v = &o->v[pick];
        float hz = note_hz(o, note);
        v->hz_target = hz;
        if (!v->on || o->p[6][0] == 0u) v->hz = hz; else if (o->last_note >= 0) v->hz = note_hz(o, o->last_note);
        v->note = note; v->on = 1; v->gate = 1; v->stage = 0; v->age = ++o->age_clock;
        v->age_f = (float)vel * (1.0f / 127.0f);
        v->pitch_env = 1.0f; v->fenv = 1.0f;
        if (v->env < 0.01f) { v->ph = 0.0f; v->nph = v->nph2 = 0.0f; v->flt[0] = v->flt[1] = v->flt[2] = v->flt[3] = 0.0f; }
        o->last_note = note;
    }
}

/* the macros resolve to the engine's working values each block: a macro at 64 leaves its targets where the pages
 * set them; below or above, it bends them together */
typedef struct {
    float shape, nz, burst, penv, rdec, damp, spread, rmix, cut, res, fold, envamt, att, dec, sus, rel;
    float l1rate, l1pitch, l2rate, l2filt, glide, unison, sub, level;
} work_t;
static void resolve(const one_t *o, work_t *w)
{
    float tone = p01(o->p[0][0]) - 0.5f, body = p01(o->p[0][1]) - 0.5f, color = p01(o->p[0][2]) - 0.5f, motion = p01(o->p[0][3]) - 0.5f;
    w->shape = clampf(p01(o->p[1][0]) + tone * 0.6f, 0.0f, 1.0f);
    w->nz = clampf(p01(o->p[1][1]) + color * 0.3f, 0.0f, 1.0f);
    w->burst = p01(o->p[1][2]);
    w->penv = p01(o->p[1][3]);
    w->rdec = clampf(p01(o->p[2][0]) + body * 0.5f, 0.0f, 1.0f);
    w->damp = clampf(p01(o->p[2][1]) - tone * 0.5f, 0.0f, 1.0f);
    w->spread = clampf(p01(o->p[2][2]) + color * 0.4f, 0.0f, 1.0f);
    w->rmix = clampf(p01(o->p[2][3]) + body * 0.4f, 0.0f, 1.0f);
    w->cut = clampf(p01(o->p[3][0]) + tone * 0.7f, 0.0f, 1.0f);
    w->res = p01(o->p[3][1]);
    w->fold = clampf(p01(o->p[3][2]) + color * 0.6f, 0.0f, 1.0f);
    w->envamt = p01(o->p[3][3]);
    w->att = p01(o->p[4][0]); w->dec = p01(o->p[4][1]); w->sus = p01(o->p[4][2]); w->rel = clampf(p01(o->p[4][3]) + body * 0.3f, 0.0f, 1.0f);
    w->l1rate = p01(o->p[5][0]); w->l1pitch = clampf(p01(o->p[5][1]) + motion * 0.4f, 0.0f, 1.0f);
    w->l2rate = clampf(p01(o->p[5][2]) + motion * 0.5f, 0.0f, 1.0f); w->l2filt = clampf(p01(o->p[5][3]) + motion * 0.6f, 0.0f, 1.0f);
    w->glide = p01(o->p[6][0]); w->unison = p01(o->p[6][1]); w->sub = p01(o->p[6][2]); w->level = p01(o->p[6][3]);
}

void one_render(one_t *o, float *out, uint32_t n)
{
    work_t w; uint32_t i, k, c;
    float sr = o->host.sr;
    float att_c, dec_c, rel_c, pe_c, glide_c, lfo1, lfo2, l1inc, l2inc, fenv_c, ratio[ONE_COMBS];
    resolve(o, &w);
    att_c = 1.0f - decay_coef(o, 0.001f * o->host.exp2f_(w.att * 11.0f));     /* 1 ms .. 2 s */
    dec_c = 1.0f - decay_coef(o, 0.005f * o->host.exp2f_(w.dec * 10.0f));     /* 5 ms .. 5 s */
    rel_c = 1.0f - decay_coef(o, 0.005f * o->host.exp2f_(w.rel * 10.0f));
    pe_c = decay_coef(o, 0.005f + w.penv * 0.12f);
    fenv_c = 1.0f - decay_coef(o, 0.02f * o->host.exp2f_(w.dec * 7.0f));       /* the filter envelope: 20 ms .. 2.5 s */
    glide_c = w.glide > 0.0f ? o->host.exp2f_(-4.0f - w.glide * 7.0f) : 1.0f;
    l1inc = (0.05f + w.l1rate * w.l1rate * 14.0f) / sr; l2inc = (0.03f + w.l2rate * w.l2rate * 8.0f) / sr;
    lfo1 = o->host.sinf_(TWO_PI * o->lfo1_ph); lfo2 = o->host.sinf_(TWO_PI * o->lfo2_ph);
    o->lfo1_ph += l1inc * (float)n; if (o->lfo1_ph >= 1.0f) o->lfo1_ph -= 1.0f;
    o->lfo2_ph += l2inc * (float)n; if (o->lfo2_ph >= 1.0f) o->lfo2_ph -= 1.0f;
    /* the combs' tunings: SPREAD 0 = unison (a thick single string), mid = a fifth and an octave (a chord), high = inharmonic (a bar) */
    ratio[0] = 1.0f;
    if (w.spread < 0.5f) { float s = w.spread * 2.0f; ratio[1] = 1.0f + s * 0.5f; ratio[2] = 1.0f + s * 1.0f; }
    else { float s = (w.spread - 0.5f) * 2.0f; ratio[1] = 1.5f + s * 0.83f; ratio[2] = 2.0f + s * 1.36f; }
    for (k = 0; k < ONE_VOICES; k++) {
        one_voice_t *v = &o->v[k];
        float vel, fb, dampc, dampk, g = 0.1f, cutoff_hz, rk, drive, folda, excite_gain, shape_k, dt_sub;
        uint32_t clen[ONE_COMBS] = {64u, 64u, 64u};
        if (!v->on) continue;
        vel = 0.4f + 0.6f * v->age_f;
        fb = 0.6f + w.rdec * 0.395f;                                   /* comb feedback: 0.6 (a tick) .. 0.995 (sings) */
        dampc = 0.05f + w.damp * 0.6f;                                 /* the loop's lowpass: bright .. dull */
        dampk = 1.0f - dampc;
        excite_gain = 0.5f + 0.5f * vel;
        shape_k = w.shape;
        folda = 1.0f + w.fold * w.fold * 7.0f;
        drive = 1.0f + w.res * 0.5f;
        dt_sub = v->hz * 0.5f / sr;
        for (i = 0; i < n; i++) {
            float dt, pd, ex, nz, comb_in, res_out = 0.0f, s, fcut, lp_in;
            /* envelopes */
            if (v->gate) {
                if (v->stage == 0) { v->env += (1.05f - v->env) * att_c; if (v->env >= 1.0f) { v->env = 1.0f; v->stage = 1; } }
                else v->env += (w.sus - v->env) * dec_c;
            } else { v->env -= v->env * rel_c; if (v->env < 0.0004f) { v->on = 0; break; } }
            v->fenv -= v->fenv * fenv_c;
            v->pitch_env *= pe_c;
            if (glide_c < 1.0f) v->hz += (v->hz_target - v->hz) * glide_c; else v->hz = v->hz_target;
            /* the exciter: a phase-distorted sine (SHAPE bends the phase: sine .. saw-like .. buzz), a pitch sweep, noise */
            dt = v->hz * (1.0f + v->pitch_env * w.penv * 2.0f) * (1.0f + lfo1 * w.l1pitch * w.l1pitch * 0.06f) / sr;
            if (dt > 0.45f) dt = 0.45f;
            v->ph += dt; if (v->ph >= 1.0f) v->ph -= 1.0f;
            pd = v->ph < shape_k * 0.5f + 0.01f ? v->ph / (shape_k * 0.5f + 0.01f) * 0.5f : 0.5f + (v->ph - shape_k * 0.5f - 0.01f) / (0.99f - shape_k * 0.5f) * 0.5f;
            ex = o->host.sinf_(TWO_PI * pd);
            nz = noise(&v->rng);
            v->nph += (nz - v->nph) * (0.1f + w.nz * 0.5f);              /* the noise's colour: dark .. white */
            v->nph2 += (v->nph - v->nph2) * 0.02f;                          /* and no rumble: a band, not a slope */
            ex = ex * (1.0f - w.nz * 0.6f) + (v->nph - v->nph2) * w.nz * 1.5f;
            if (w.burst > 0.0f) ex += (nz - v->nph2) * w.burst * v->fenv * v->fenv * 2.0f;   /* BURST: the hit at the start, from the filter env */
            ex *= excite_gain * (0.35f + 0.65f * v->env);
            /* the resonator: three waveguide combs, fed by the exciter, mixed against it */
            comb_in = ex;
            if ((i & 31u) == 0u)                                              /* the combs' lengths follow the pitch, every 32 samples */
                for (c = 0; c < ONE_COMBS; c++) {
                    float len = sr / (v->hz * (1.0f + v->pitch_env * w.penv * 2.0f) * ratio[c] * (1.0f + (float)c * w.unison * 0.004f));
                    if (len < 4.0f) len = 4.0f;
                    if (len > (float)(ONE_COMB_LEN - 2)) len = (float)(ONE_COMB_LEN - 2);
                    clen[c] = (uint32_t)len;
                }
            for (c = 0; c < ONE_COMBS; c++) {
                float y, nw;
                uint32_t rd = (v->w - clen[c]) & (ONE_COMB_LEN - 1u);
                y = (float)v->buf[c][rd] * (1.0f / 32768.0f);
                v->lp[c] += (y - v->lp[c]) * dampk;
                nw = comb_in * 0.22f + v->lp[c] * fb;
                if (nw > 0.9f || nw < -0.9f) nw = soft(nw);                  /* the loop saturates gently, never hard */
                v->buf[c][v->w] = (int16_t)(nw * 32767.0f);
                res_out += y * (c == 0 ? 0.6f : 0.35f);
            }
            v->w = (v->w + 1u) & (ONE_COMB_LEN - 1u);
            s = ex * (1.0f - w.rmix) * 0.7f + res_out * w.rmix * 1.6f;
            /* the folder, then a four-pole ladder */
            if (w.fold > 0.0f) s = o->host.sinf_(s * folda * 1.5707963f) * (0.6f + 0.4f * (1.0f - w.fold)) + s * (1.0f - w.fold) * 0.5f;
            if ((i & 15u) == 0u) {                                              /* the cutoff every 16 samples */
                fcut = w.cut * (0.75f + 0.25f * vel) + v->fenv * w.envamt * 0.8f + lfo2 * w.l2filt * 0.3f;
                cutoff_hz = 30.0f * o->host.exp2f_(clampf(fcut, 0.0f, 1.0f) * 9.0f);   /* 30 Hz .. 15 kHz */
                g = cutoff_hz / sr * 4.5f; if (g > 0.95f) g = 0.95f;
            }
            rk = w.res * 3.6f;
            lp_in = soft(s * drive - v->flt[3] * rk);
            v->flt[0] += (lp_in - v->flt[0]) * g; v->flt[1] += (v->flt[0] - v->flt[1]) * g;
            v->flt[2] += (v->flt[1] - v->flt[2]) * g; v->flt[3] += (v->flt[2] - v->flt[3]) * g;
            s = v->flt[3] * (1.0f + w.res * 0.8f);
            /* the sub */
            v->sub_ph += dt_sub; if (v->sub_ph >= 1.0f) v->sub_ph -= 1.0f;
            s += o->host.sinf_(TWO_PI * v->sub_ph) * w.sub * 0.7f * v->env;
            out[i] += soft(s * 0.9f) * v->env * w.level * 0.6f * o->out_gain;
        }
    }
}

const char *one_param_text(const one_t *o, uint32_t page, uint32_t k, char *buf)
{
    (void)buf;
    if (page == 2u && k == 2u) { uint8_t s = o->p[2][2]; return s < 20u ? "UNISON" : s < 50u ? "WIDE" : s < 70u ? "CHORD" : s < 100u ? "BELL" : "BAR"; }
    if (page == 6u && k == 0u && o->p[6][0] == 0u) return "OFF";
    if (page == 3u && k == 2u && o->p[3][2] == 0u) return "OFF";
    if (page == 1u && k == 0u) { uint8_t s = o->p[1][0]; return s < 25u ? "SINE" : s < 60u ? "BENT" : s < 95u ? "SAWISH" : "BUZZ"; }
    return 0;
}
