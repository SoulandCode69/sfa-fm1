/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * GREYS: a wavetable / FM synth. Four voices; each a morphing wavetable oscillator in a three-voice unison, an FM
 * modulator at a fixed ratio, a sub, a filter with drive, an ADSR, an LFO to morph / pitch / filter, glide, and a
 * chorus on the way out. Transmissions, glassy keys, moving pads, evolving textures: the Greys speak in timbre.
 *   page 1 OSC:    MORPH FM RATIO DETUNE     page 2 ENV:   ATTACK DECAY SUSTAIN RELEASE
 *   page 3 FILTER: CUTOFF RES ENVAMT DRIVE   page 4 MOD:   LFORATE L>MORPH L>PITCH L>FILT
 *   page 5 SPACE:  CHORUS SUB GLIDE LEVEL
 * The ALGO knob browses 24 presets (greys_preset). This file also stands in for the Hopper's API: the app's slicer
 * hooks all answer "nothing here", so the Ship and the UI need no other changes. */
#include "fastmath.h"
#include "engine.h"
#include "../../base/include/plat.h"

#define NV 4
#define NPRE 24
#define WT_N 256
#define NWT 6
static const char *const NAMES[5][NPARAM] = {
    {"MORPH", "FM", "RATIO", "DETUNE"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"}, {"CUTOFF", "RES", "ENVAMT", "DRIVE"},
    {"LFORATE", "L>MORPH", "L>PITCH", "L>FILT"}, {"CHORUS", "SUB", "GLIDE", "LEVEL"}};
static const char *const TITLES[5] = {"OSCILLATOR", "ENVELOPE", "FILTER", "MODULATION", "SPACE"};
static uint8_t par[5][NPARAM];
static const float RATIOS[8] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f};

typedef struct { const char *name; uint8_t p[5][NPARAM]; } preset_t;
static const preset_t PRESETS[NPRE] = {
    {"TRANSMISSION", {{40, 60, 64, 30}, {5, 50, 90, 40}, {80, 30, 40, 10}, {60, 70, 0, 20}, {50, 0, 0, 100}}},
    {"GLASS KEYS",   {{70, 45, 80, 15}, {0, 55, 20, 35}, {100, 20, 60, 0}, {30, 10, 0, 0}, {40, 0, 0, 100}}},
    {"SAUCER PAD",   {{55, 20, 40, 60}, {60, 70, 100, 80}, {60, 30, 20, 0}, {25, 50, 8, 30}, {80, 20, 0, 100}}},
    {"PROBE",        {{90, 90, 110, 10}, {0, 30, 0, 20}, {110, 40, 80, 20}, {90, 0, 60, 0}, {20, 0, 0, 100}}},
    {"ABDUCTION",    {{30, 70, 20, 80}, {40, 80, 90, 90}, {40, 60, 40, 40}, {15, 80, 20, 60}, {90, 40, 20, 100}}},
    {"HIVE",         {{64, 50, 64, 100}, {20, 60, 80, 60}, {70, 50, 30, 30}, {70, 30, 0, 40}, {70, 10, 0, 100}}},
    {"COLD STAR",    {{100, 10, 64, 40}, {70, 90, 100, 100}, {90, 10, 10, 0}, {10, 30, 4, 10}, {100, 0, 0, 100}}},
    {"SIGNAL",       {{20, 100, 90, 0}, {0, 40, 60, 25}, {127, 0, 0, 30}, {120, 0, 100, 0}, {0, 0, 0, 100}}},
    {"GREY CHOIR",   {{50, 15, 40, 70}, {50, 70, 100, 70}, {55, 40, 20, 0}, {20, 40, 6, 20}, {90, 0, 0, 100}}},
    {"ORBIT",        {{80, 35, 70, 50}, {30, 60, 70, 60}, {70, 35, 40, 10}, {40, 60, 0, 50}, {60, 0, 0, 100}}},
    {"CRYO BASS",    {{35, 30, 20, 20}, {0, 50, 70, 30}, {50, 40, 60, 50}, {0, 0, 0, 0}, {10, 100, 20, 100}}},
    {"BEAM",         {{110, 80, 100, 5}, {0, 20, 10, 10}, {120, 50, 90, 10}, {0, 0, 0, 0}, {0, 0, 50, 100}}},
    {"NEBULA",       {{45, 25, 50, 90}, {80, 100, 100, 110}, {45, 50, 30, 0}, {8, 70, 10, 40}, {110, 20, 0, 100}}},
    {"IMPLANT",      {{75, 65, 120, 20}, {0, 35, 30, 30}, {90, 60, 70, 30}, {100, 20, 0, 70}, {30, 0, 0, 100}}},
    {"TELEPATH",     {{60, 40, 64, 45}, {25, 65, 85, 55}, {65, 45, 35, 0}, {50, 90, 2, 0}, {60, 0, 10, 100}}},
    {"REACTOR",      {{25, 85, 40, 60}, {10, 50, 80, 40}, {60, 70, 50, 90}, {80, 0, 0, 100}, {40, 60, 0, 100}}},
    {"HALO",         {{95, 5, 64, 55}, {90, 90, 100, 120}, {80, 20, 20, 0}, {12, 20, 3, 15}, {120, 0, 0, 100}}},
    {"SPECIMEN",     {{15, 55, 80, 35}, {0, 45, 40, 30}, {85, 35, 70, 20}, {60, 40, 0, 60}, {30, 30, 0, 100}}},
    {"VISITOR LEAD", {{85, 30, 64, 25}, {0, 50, 80, 30}, {100, 40, 50, 20}, {45, 15, 0, 20}, {40, 0, 30, 100}}},
    {"DEEP SPACE",   {{40, 0, 64, 110}, {100, 110, 100, 127}, {35, 30, 10, 0}, {6, 60, 12, 30}, {127, 30, 0, 100}}},
    {"TRACTOR",      {{65, 75, 30, 15}, {0, 60, 60, 40}, {55, 55, 80, 60}, {30, 0, 0, 90}, {20, 70, 10, 100}}},
    {"GLYPH",        {{105, 50, 90, 0}, {0, 25, 0, 15}, {127, 30, 40, 0}, {0, 0, 0, 0}, {0, 0, 0, 100}}},
    {"MOTHERSHIP",   {{50, 45, 50, 75}, {40, 80, 90, 90}, {50, 60, 40, 50}, {20, 50, 6, 50}, {100, 50, 0, 100}}},
    {"ARRIVAL",      {{70, 20, 64, 85}, {110, 100, 100, 100}, {75, 25, 30, 0}, {10, 90, 5, 40}, {115, 10, 0, 100}}},
};
static uint8_t preset_i;
static void preset_load(void) { uint32_t a, b; for (a = 0; a < 5u; a++) for (b = 0; b < NPARAM; b++) par[a][b] = PRESETS[preset_i].p[a][b]; }
void greys_preset(int32_t step) { preset_i = (uint8_t)(((int32_t)preset_i + step + NPRE) % NPRE); preset_load(); }
void greys_preset_set(int i) { preset_i = (uint8_t)(i % NPRE); }
int greys_preset_index(void) { return preset_i; }
const char *greys_preset_name(void) { return PRESETS[preset_i].name; }
const char *greys_preset_name_at(int32_t off) { return PRESETS[((int32_t)preset_i + off + NPRE * 4) % NPRE].name; }

/* the wavetables, built once: sine, triangle, saw, square, a formant pulse, glass (odd partials, bright) */
PLAT_POOL static float wt[NWT][WT_N + 1];
static void wt_build(void)
{
    uint32_t i, h;
    for (i = 0; i <= WT_N; i++) {
        float ph = (float)i / WT_N, s = 0.0f, tr = 0.0f, sq = 0.0f, fo = 0.0f, gl = 0.0f;
        for (h = 1; h <= 16u; h++) {
            float v = fm_sinf(6.2831853f * ph * (float)h);
            s += v / (float)h;
            if (h & 1u) { sq += v / (float)h; tr += (h & 2u ? -1.0f : 1.0f) * v / (float)(h * h); }
            fo += v * (h >= 3u && h <= 6u ? 1.0f : 0.25f) / (float)h;
            if (h == 1u || h == 3u || h == 5u || h == 7u || h == 11u) gl += v / (float)(1u + (h >> 2));
        }
        wt[0][i] = fm_sinf(6.2831853f * ph); wt[1][i] = tr * 0.81f; wt[2][i] = s * 0.55f; wt[3][i] = sq * 0.7f; wt[4][i] = fo * 0.6f; wt[5][i] = gl * 0.45f;
    }
}
static inline float wt_read(float morph, float ph)
{
    float m = morph * (NWT - 1); uint32_t a = (uint32_t)m; float f = m - (float)a, pos = ph * WT_N; uint32_t k = (uint32_t)pos; float g = pos - (float)k;
    float x0, x1;
    if (a >= NWT - 1u) { a = NWT - 2u; f = 1.0f; }
    x0 = wt[a][k] + (wt[a][k + 1] - wt[a][k]) * g;
    x1 = wt[a + 1][k] + (wt[a + 1][k + 1] - wt[a + 1][k]) * g;
    return x0 + (x1 - x0) * f;
}

typedef struct { float ph[3], mph, sph, hz, hz_target, env, fenv, vel; int32_t note; uint8_t on, stage, gate; uint32_t age; svf_t flt; svfc_t fc; } voice_t;
static voice_t v[NV];
static uint32_t age_clock;
static float lfo_ph, pgain = 1.0f;
#define CH_LEN 2048u
PLAT_POOL static int16_t chorus[CH_LEN];
static uint32_t ch_w; static float ch_ph;
static int32_t last_note = -1;
static float vel_last = 0.8f;

static void g_init(void) { uint32_t i; wt_build(); for (i = 0; i < NV; i++) { v[i].on = 0; v[i].hz = 110.0f; } preset_load(); }
static void g_panic(void) { uint32_t i; for (i = 0; i < NV; i++) v[i].on = 0; }

static void g_note(int32_t note, uint32_t vel, int on)
{
    uint32_t i, pick = 0;
    if (note < 0) return;
    if (!on) { for (i = 0; i < NV; i++) if (v[i].on && v[i].note == note && v[i].gate) v[i].gate = 0; return; }
    vel_last = (float)vel * (1.0f / 127.0f);
    for (i = 0; i < NV; i++) if (!v[i].on) { pick = i; break; }
    if (i == NV) { for (i = 0; i < NV; i++) if (v[i].age < v[pick].age) pick = i; }
    {
        voice_t *w = &v[pick];
        float hz = note_hz(note);
        w->hz_target = hz;
        if (!w->on || par[4][2] == 0u) w->hz = hz;
        else if (last_note >= 0) w->hz = note_hz(last_note);
        w->note = note; w->on = 1; w->gate = 1; w->stage = 0; w->age = ++age_clock; w->vel = vel_last;
        if (!w->env) { w->ph[0] = 0.0f; w->ph[1] = 0.33f; w->ph[2] = 0.67f; w->mph = 0.0f; w->sph = 0.0f; w->flt.ic1 = w->flt.ic2 = 0.0f; }
        last_note = note;
    }
}

static void g_render(float *out, uint32_t n)
{
    uint32_t i, k;
    float morph0 = p01(par[0][0]), fm = p01(par[0][1]) * p01(par[0][1]) * 6.0f, ratio = RATIOS[par[0][2] >> 4], det = p01(par[0][3]) * 0.012f;
    float att = fm_exp2f(-5.0f - p01(par[1][0]) * 9.0f), dec = fm_exp2f(-5.5f - p01(par[1][1]) * 8.5f), sus = p01(par[1][2]), rel = fm_exp2f(-5.0f - p01(par[1][3]) * 9.0f);
    float cut = p01(par[2][0]), res = p01(par[2][1]) * 0.9f, envamt = p01(par[2][2]), drive = 1.0f + p01(par[2][3]) * 4.0f;
    float lrate = 0.05f + p01(par[3][0]) * p01(par[3][0]) * 12.0f, lmorph = p01(par[3][1]) * 0.5f, lpitch = p01(par[3][2]) * p01(par[3][2]) * 0.06f, lfilt = p01(par[3][3]);
    float chmix = p01(par[4][0]) * 0.6f, sub = p01(par[4][1]) * 0.7f, glide = par[4][2] ? fm_exp2f(-4.0f - p01(par[4][2]) * 7.0f) : 1.0f, level = p01(par[4][3]) * 1.1f * pgain;
    float lfo = fm_sinf(6.2831853f * lfo_ph), morph = morph0 + lfo * lmorph, pitchmod = fm_exp2f(lfo * lpitch);
    if (morph < 0.0f) morph = 0.0f; else if (morph > 1.0f) morph = 1.0f;
    lfo_ph += lrate * (float)n / SR; if (lfo_ph >= 1.0f) lfo_ph -= 1.0f;
    for (k = 0; k < NV; k++) {
        voice_t *w = &v[k];
        if (!w->on) continue;
        {
            float fc = 40.0f * fm_exp2f((cut * (0.7f + 0.3f * w->vel) + w->fenv * envamt + lfo * lfilt * 0.5f) * 8.6f);   /* 40 Hz .. ~16 kHz, velocity opens it */
            svf_coef(&w->fc, fc, res);
        }
    }
    for (i = 0; i < n; i++) {
        float s = 0.0f;
        for (k = 0; k < NV; k++) {
            voice_t *w = &v[k];
            float dt, x, mod, u;
            uint32_t j;
            if (!w->on) continue;
            /* the envelope: attack to 1, decay to sustain, release on gate off */
            if (w->gate) {
                if (w->stage == 0) { w->env += (1.05f - w->env) * att; if (w->env >= 1.0f) { w->env = 1.0f; w->stage = 1; } }
                else w->env += (sus - w->env) * dec;
            } else { w->env -= w->env * rel; if (w->env < 0.0005f) { w->on = 0; continue; } }
            w->fenv = w->stage == 0 ? w->env : w->fenv + (0.0f - w->fenv) * dec * 0.5f;
            if (glide < 1.0f) w->hz += (w->hz_target - w->hz) * glide; else w->hz = w->hz_target;
            dt = w->hz * pitchmod / SR;
            if (dt > 0.45f) dt = 0.45f;
            w->mph += dt * ratio; if (w->mph >= 1.0f) w->mph -= 1.0f;
            mod = fm_sinf(6.2831853f * w->mph) * fm * (0.3f + 0.7f * w->env) * 0.08f;   /* FM: the modulator pushes the table's phase */
            x = 0.0f;
            for (j = 0; j < 3u; j++) {
                float d = j == 0u ? dt : j == 1u ? dt * (1.0f + det) : dt * (1.0f - det), p;
                w->ph[j] += d; if (w->ph[j] >= 1.0f) w->ph[j] -= 1.0f;
                p = w->ph[j] + mod; p -= (float)(int32_t)p; if (p < 0.0f) p += 1.0f;
                x += wt_read(morph, p) * (j == 0u ? 0.5f : 0.35f);
            }
            w->sph += dt * 0.5f; if (w->sph >= 1.0f) w->sph -= 1.0f;
            x += fm_sinf(6.2831853f * w->sph) * sub;
            u = svf_lp(&w->flt, &w->fc, fm_soft(x * drive) * (1.0f / (0.6f + drive * 0.4f)));
            s += u * w->env * (0.5f + 0.5f * w->vel);
        }
        s *= level;
        if (chmix > 0.0f) {                                              /* the chorus: a 7..19 ms modulated tap */
            float rd, d; uint32_t ri; int32_t a, b;
            ch_ph += 0.6f / SR; if (ch_ph >= 1.0f) ch_ph -= 1.0f;
            rd = 300.0f + 260.0f * (1.0f + fm_sinf(6.2831853f * ch_ph));
            ri = (uint32_t)rd; d = rd - (float)ri;
            a = chorus[(ch_w + CH_LEN - ri) % CH_LEN]; b = chorus[(ch_w + CH_LEN - ri - 1u) % CH_LEN];
            { float cs = s; if (cs > 1.0f) cs = 1.0f; else if (cs < -1.0f) cs = -1.0f; chorus[ch_w] = (int16_t)(cs * 32767.0f); }
            ch_w = (ch_w + 1u) % CH_LEN;
            s += ((float)a + ((float)b - (float)a) * d) * (1.0f / 32768.0f) * chmix;
        }
        out[i] += s;
    }
}

static const char *g_text(uint8_t page, uint32_t p, char *buf)
{
    static const char *const WN[NWT] = {"SINE", "TRI", "SAW", "SQUARE", "FORMANT", "GLASS"};
    if (page == 0u && p == 0u) { uint32_t m = par[0][0] * (NWT - 1u) / 127u; return WN[m >= NWT ? NWT - 1u : m]; }
    if (page == 0u && p == 2u) { float r = RATIOS[par[0][2] >> 4]; buf[0] = (char)('0' + (int)r); buf[1] = '.'; buf[2] = (char)('0' + (int)(r * 10.0f) % 10); buf[3] = 0; return buf; }
    if (page == 4u && p == 2u && par[4][2] == 0u) return "OFF";
    if (page == 4u && p == 0u && par[4][0] == 0u) return "DRY";
    return 0;
}

/* a trim per preset (dB) from tools/audit.py */
static const int8_t GAIN_DB[NPRE] = {5, 6, 1, 6, -1, -1, 5, 6, 4, 5, 4, 10, -1, 4, 4, 3, 0, 5, 5, 3, 2, 8, 0, 2};
static void g_render_t(float *out, uint32_t n) { pgain = fm_db2lin((float)GAIN_DB[preset_i]); g_render(out, n); }

const engine_t hopper = {"GREYS", g_init, g_note, g_render_t, g_panic, 5, {NAMES[0], NAMES[1], NAMES[2], NAMES[3], NAMES[4]},
                         {par[0], par[1], par[2], par[3], par[4]}, g_text, TITLES, "greys", 1};

/* ---- the Hopper's API, answered by the Greys: no slicer here */
#include <stdint.h>
const char *hop_source_name(char *buf) { (void)buf; return greys_preset_name(); }
int hop_source(int32_t step) { greys_preset(step); return 1; }
int hop_source_index(void) { return greys_preset_index(); }
void hop_source_set(int s) { greys_preset_set(s); preset_load(); }
int hop_chop_mode(void) { return 0; }
int hop_chops(void) { return 8; }
int hop_chopmode(void) { return 0; }
void hop_chopmode_set(int m) { (void)m; }
void hop_chop_on(int chops) { (void)chops; }
int hop_recording(void) { return 0; }
void hop_record_start(void) {}
void hop_record_stop(void) {}
void hop_record_feed(const float *mix, uint32_t n) { (void)mix; (void)n; }
int hop_save_needed(void) { return 0; }
int hop_save(void) { return 0; }
uint32_t hop_bank_seconds_x10(void) { return 0; }
uint32_t hop_bank_len(void) { return 0; }
int32_t hop_peak(uint32_t from, uint32_t to) { (void)from; (void)to; return 0; }
uint32_t hop_window(uint32_t *a, uint32_t *b) { *a = *b = 0; return 0; }
float hop_playhead(void) { return -1.0f; }
uint32_t hop_slice_points(const uint32_t **pts) { *pts = 0; return 0; }
uint8_t *hop_slice_blob(uint32_t *size) { static uint8_t z[64]; *size = sizeof z; return z; }
void hop_slices_default(void) {}
int hop_last_slice(void) { return 0; }
void hop_sync(void) {}
void hop_rechop(void) {}
void hop_slice_nudge(uint32_t idx, int32_t delta) { (void)idx; (void)delta; }
int hop_slice_active(uint32_t idx) { (void)idx; return 0; }
