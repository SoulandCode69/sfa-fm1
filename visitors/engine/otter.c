/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * OTTER SYNTH: a 4-voice polysynth with presets and an arpeggiator.
 *   two band-limited oscillators (PolyBLEP saw / pulse / square / triangle) or 2-op FM (index scaled down with pitch),
 *   unison detune with slow analog drift per voice, a sub, a driven 2-pole resonant lowpass with key tracking,
 *   its own envelope and velocity; ADSR on the amp; LFO to pitch and filter; glide; parameters smoothed (no zipper)
 *   page 1: TONE RES WAVE DETUNE      page 2: ATTACK DECAY SUSTAIN RELEASE      page 3: FENV FDECAY VEL>F SUB
 *   page 4: LFO VIBRATO WOBBLE GLIDE  page 5: ARP ARPRATE ARPOCT MONO
 *   the ALGO knob browses 16 presets (the app calls otter_preset) */
#include "fastmath.h"
#include "engine.h"
#include "../../base/include/plat.h"

static const char *const NAMES[5][NPARAM] = {
    {"TONE", "RES", "WAVE", "DETUNE"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"}, {"FENV", "FDECAY", "VEL>F", "SUB"},
    {"LFO", "VIBRATO", "WOBBLE", "GLIDE"}, {"ARP", "ARPRATE", "ARPOCT", "MONO"}};
static uint8_t par[5][NPARAM] = {{78, 30, 0, 40}, {3, 55, 70, 30}, {50, 50, 40, 30}, {40, 10, 0, 0}, {0, 64, 0, 0}};
static const char *const WAVES[6] = {"SAW", "PULSE", "SQUARE", "TRI", "FM", "SUPER"};
static const char *const ARPS[5] = {"OFF", "UP", "DOWN", "UP-DN", "RANDOM"};
static const char *const RATES[4] = {"1/8", "1/16", "1/8T", "1/32"};

/* presets: name, then the 5 pages */
typedef struct { const char *name; uint8_t p[5][NPARAM]; } preset_t;
#define NPRESET 48
/* WAVE: 0-20 SAW, 21-41 PULSE, 42-62 SQUARE, 63-83 TRI, 84-104 FM, 105-127 SUPER */
static const preset_t PRESETS[NPRESET] = {
    {"AURORA PAD", {{55, 25, 115, 90}, {70, 80, 100, 100}, {30, 70, 10, 10}, {30, 8, 0, 0}, {0, 64, 0, 0}}},
    {"LIGHT BEINGS", {{70, 20, 64, 75}, {60, 90, 100, 110}, {40, 80, 20, 0}, {25, 12, 0, 0}, {0, 64, 0, 0}}},
    {"CATHEDRAL", {{45, 30, 50, 60}, {90, 100, 100, 120}, {25, 90, 10, 30}, {15, 6, 0, 0}, {0, 64, 0, 0}}},
    {"CRYSTAL KEYS", {{95, 10, 85, 25}, {0, 60, 30, 45}, {70, 55, 70, 0}, {40, 6, 0, 0}, {0, 64, 0, 0}}},
    {"ICE PIANO", {{85, 15, 95, 15}, {0, 65, 20, 40}, {60, 60, 80, 10}, {30, 0, 0, 0}, {0, 64, 0, 0}}},
    {"TALL CHOIR", {{50, 20, 115, 85}, {50, 75, 100, 90}, {35, 65, 15, 0}, {35, 14, 0, 0}, {0, 64, 0, 0}}},
    {"HALO LEAD", {{90, 30, 115, 40}, {2, 55, 85, 40}, {60, 45, 50, 0}, {55, 20, 0, 25}, {0, 64, 0, 127}}},
    {"BEACON", {{100, 55, 0, 30}, {0, 45, 70, 35}, {90, 40, 70, 0}, {40, 14, 0, 20}, {0, 64, 0, 127}}},
    {"NORTHERN ARP", {{85, 25, 115, 45}, {0, 35, 0, 25}, {90, 30, 70, 0}, {40, 8, 0, 0}, {32, 64, 32, 0}}},
    {"STARFIELD ARP", {{95, 20, 90, 70}, {5, 50, 30, 60}, {60, 50, 50, 0}, {50, 10, 0, 0}, {96, 64, 64, 0}}},
    {"GLASS BELLS", {{105, 15, 95, 35}, {0, 70, 10, 70}, {70, 70, 60, 0}, {40, 8, 0, 0}, {0, 64, 0, 0}}},
    {"SOLAR WIND", {{40, 45, 0, 110}, {80, 100, 100, 100}, {40, 90, 20, 40}, {20, 6, 30, 0}, {0, 64, 0, 0}}},
    {"PLEIADES", {{65, 20, 115, 95}, {45, 80, 100, 90}, {45, 70, 20, 0}, {30, 16, 0, 0}, {0, 64, 0, 0}}},
    {"VELVET", {{35, 30, 64, 60}, {40, 70, 100, 80}, {30, 70, 20, 30}, {25, 10, 0, 0}, {0, 64, 0, 0}}},
    {"WARM ORGAN", {{90, 5, 50, 25}, {0, 55, 95, 15}, {15, 50, 15, 25}, {70, 15, 0, 0}, {0, 64, 0, 0}}},
    {"ORACLE", {{60, 40, 115, 60}, {20, 60, 80, 60}, {60, 55, 40, 0}, {45, 55, 0, 0}, {0, 64, 0, 0}}},
    {"MESSAGE", {{110, 35, 25, 20}, {0, 40, 70, 25}, {80, 40, 60, 0}, {60, 25, 0, 30}, {0, 64, 0, 127}}},
    {"SIGNAL LEAD", {{120, 65, 0, 10}, {1, 40, 70, 20}, {110, 35, 80, 0}, {40, 12, 0, 30}, {0, 64, 0, 127}}},
    {"FLUTE OF LIGHT", {{60, 10, 64, 5}, {25, 40, 90, 30}, {20, 50, 30, 0}, {50, 30, 0, 10}, {0, 64, 0, 127}}},
    {"WHISPER", {{30, 25, 64, 40}, {60, 80, 100, 100}, {20, 70, 10, 0}, {20, 20, 0, 0}, {0, 64, 0, 0}}},
    {"ELDER KEYS", {{75, 25, 95, 30}, {0, 65, 30, 40}, {55, 55, 70, 10}, {50, 8, 0, 0}, {0, 64, 0, 0}}},
    {"HARP OF VEGA", {{95, 0, 95, 15}, {0, 35, 0, 20}, {80, 30, 60, 0}, {0, 0, 0, 0}, {0, 64, 0, 0}}},
    {"TEMPLE", {{50, 35, 50, 70}, {70, 90, 100, 110}, {30, 80, 20, 40}, {20, 10, 10, 0}, {0, 64, 0, 0}}},
    {"CHORD STAR", {{80, 30, 115, 60}, {0, 35, 0, 20}, {100, 30, 80, 20}, {30, 0, 0, 0}, {0, 64, 0, 0}}},
    {"RAVE BEINGS", {{95, 45, 115, 95}, {0, 30, 0, 15}, {110, 25, 90, 10}, {30, 0, 0, 0}, {0, 64, 0, 0}}},
    {"BLUE SHIFT", {{105, 55, 25, 35}, {0, 40, 70, 20}, {80, 35, 60, 0}, {50, 20, 0, 20}, {0, 64, 0, 127}}},
    {"ARP ASCENT", {{85, 20, 95, 30}, {0, 35, 0, 25}, {90, 30, 70, 0}, {40, 0, 0, 0}, {32, 96, 32, 0}}},
    {"ARP DESCENT", {{75, 60, 0, 20}, {0, 25, 0, 10}, {110, 25, 90, 20}, {40, 0, 0, 0}, {64, 64, 32, 0}}},
    {"ARP DRIFT", {{90, 15, 115, 85}, {5, 60, 30, 70}, {60, 50, 50, 0}, {50, 10, 0, 0}, {127, 32, 64, 0}}},
    {"ARP PULSE", {{100, 40, 25, 0}, {0, 25, 0, 5}, {0, 50, 0, 0}, {0, 0, 0, 0}, {32, 96, 0, 127}}},
    {"MOONLIGHT", {{55, 15, 95, 45}, {10, 70, 60, 60}, {40, 60, 40, 0}, {40, 10, 0, 0}, {0, 64, 0, 0}}},
    {"GOLDEN PAD", {{65, 25, 115, 80}, {55, 80, 100, 95}, {40, 70, 20, 20}, {30, 10, 0, 0}, {0, 64, 0, 0}}},
    {"NORDIC BASS", {{50, 65, 90, 80}, {0, 40, 60, 15}, {80, 35, 70, 60}, {90, 0, 110, 0}, {0, 64, 0, 127}}},
    {"DEEP WELL", {{30, 35, 0, 95}, {50, 80, 100, 90}, {30, 80, 20, 40}, {30, 6, 20, 0}, {0, 64, 0, 0}}},
    {"DRONE OF AGES", {{45, 60, 0, 110}, {90, 100, 100, 127}, {40, 90, 20, 60}, {25, 6, 30, 0}, {0, 64, 0, 0}}},
    {"SHIMMER", {{105, 20, 115, 100}, {30, 70, 100, 100}, {40, 70, 30, 0}, {60, 10, 0, 0}, {0, 64, 0, 0}}},
    {"EP STARS", {{85, 5, 95, 25}, {0, 70, 20, 45}, {60, 60, 70, 10}, {50, 8, 0, 0}, {0, 64, 0, 0}}},
    {"CLAV OF LIGHT", {{110, 35, 25, 10}, {0, 45, 10, 15}, {90, 35, 80, 0}, {30, 0, 0, 0}, {0, 64, 0, 0}}},
    {"DUST", {{60, 25, 64, 70}, {40, 75, 100, 90}, {35, 65, 15, 0}, {35, 14, 10, 0}, {0, 64, 0, 0}}},
    {"GLIDE LEAD", {{95, 45, 0, 20}, {0, 40, 80, 30}, {80, 35, 60, 0}, {40, 16, 0, 70}, {0, 64, 0, 127}}},
    {"WOBBLE PAD", {{50, 40, 115, 80}, {40, 70, 100, 80}, {40, 70, 20, 0}, {70, 10, 80, 0}, {0, 64, 0, 0}}},
    {"SUB CHOIR", {{40, 15, 115, 70}, {40, 70, 100, 80}, {30, 70, 10, 90}, {20, 8, 0, 0}, {0, 64, 0, 0}}},
    {"CHIP STARS", {{127, 0, 50, 0}, {0, 25, 0, 5}, {0, 50, 0, 0}, {0, 0, 0, 0}, {96, 96, 64, 127}}},
    {"PIANO HOUSE", {{90, 10, 95, 20}, {0, 65, 30, 35}, {55, 55, 70, 0}, {40, 0, 0, 0}, {0, 64, 0, 0}}},
    {"TAPE WARM", {{45, 20, 64, 80}, {45, 80, 100, 85}, {25, 70, 15, 0}, {55, 24, 10, 0}, {0, 64, 0, 0}}},
    {"BELL PAD", {{95, 15, 95, 90}, {20, 80, 60, 90}, {60, 70, 50, 0}, {40, 6, 0, 0}, {0, 64, 0, 0}}},
    {"ANTHEM", {{85, 30, 115, 85}, {10, 60, 90, 70}, {60, 50, 50, 0}, {40, 10, 0, 0}, {0, 64, 0, 0}}},
    {"FAREWELL", {{50, 25, 95, 60}, {80, 90, 100, 127}, {30, 80, 10, 20}, {20, 8, 0, 0}, {0, 64, 0, 0}}},
};
/* a trim per preset (dB), from tools/audit.py: every preset peaks about the same */
static const int8_t GAIN_DB[NPRESET] = {6, 4, 0, 10, 10, 6, 10, 9, 10, 10, 6, 1, 5, 4, 5, 7, 10, 8, 10, 6, 6, 10, 0, 8, 9, 10, 10, 10, 10, 10, 7, 4, 8, 2, -1, 4, 8, 6, 5, 10, 4, 3, 7, 10, 5, 2, 6, 0};
static float pgain = 1.0f;
static uint8_t preset_i;
void otter_preset(int32_t step)
{
    uint32_t g, p;
    preset_i = (uint8_t)(((int32_t)preset_i + step + NPRESET) % NPRESET);
    for (g = 0; g < 5u; g++) for (p = 0; p < NPARAM; p++) par[g][p] = PRESETS[preset_i].p[g][p];
}
const char *otter_preset_name(void) { return PRESETS[preset_i].name; }
const char *otter_preset_name_at(int32_t off) { return PRESETS[((int32_t)preset_i + off + NPRESET * 4) % NPRESET].name; }
void otter_preset_set(int i) { preset_i = (uint8_t)(i % NPRESET); }
int otter_preset_index(void) { return preset_i; }

#define NV 4
typedef struct {
    int32_t note;
    float vel, hz, hz_target, drift, ph1, ph2, ph3, phs, env, fenv;
    svf_t flt;
    uint8_t stage;                        /* 0 off, 1 attack, 2 decay, 3 sustain, 4 release */
    uint32_t age;
} voice_t;
static voice_t v[NV];
static volatile int32_t pend_note[8];
static volatile uint8_t pend_vel[8], pend_on[8], pend_n;
static uint32_t clock_, rng = 0x9E3779B9u;
static float lfo_ph, last_hz = 220.0f;
static float sm_cut, sm_res, sm_det;      /* smoothed knobs */
#define ENS_LEN 2048u
PLAT_POOL static int16_t ens[ENS_LEN];        /* the ensemble: three modulated taps over the voices' sum */
static uint32_t ens_w; static float ens_ph;
static float vsum[256];

/* the arpeggiator: the held notes, stepped by the app's clock (otter_arp_tick, from the sequencer's tempo) */
static int32_t held[8]; static uint8_t held_vel[8]; static uint8_t nheld;
static int32_t arp_note = -1; static uint8_t arp_i, arp_dir;
static void voice_on(int32_t note, uint32_t vel);
static void voice_off(int32_t note);

static void o_init(void) { uint32_t i; for (i = 0; i < NV; i++) { v[i].note = -1; v[i].stage = 0; v[i].env = 0; v[i].flt.ic1 = v[i].flt.ic2 = 0; } pend_n = 0; nheld = 0; arp_note = -1; }
static void o_panic(void) { o_init(); }

static void o_note(int32_t note, uint32_t vel, int on)     /* queued: the render owns the voices */
{
    uint8_t n = pend_n;
    if (n >= 8u) return;
    pend_note[n] = note; pend_vel[n] = (uint8_t)vel; pend_on[n] = (uint8_t)on;
    pend_n = (uint8_t)(n + 1u);
}

static void held_add(int32_t note, uint8_t vel)
{
    uint32_t i, j;
    for (i = 0; i < nheld; i++) if (held[i] == note) return;
    if (nheld >= 8u) return;
    for (i = 0; i < nheld && held[i] < note; i++) ;                 /* keep them sorted */
    for (j = nheld; j > i; j--) { held[j] = held[j - 1]; held_vel[j] = held_vel[j - 1]; }
    held[i] = note; held_vel[i] = vel; nheld++;
}
static void held_del(int32_t note)
{
    uint32_t i, j;
    for (i = 0; i < nheld; i++) if (held[i] == note) {
        for (j = i; j + 1u < nheld; j++) { held[j] = held[j + 1]; held_vel[j] = held_vel[j + 1]; }
        nheld--; return;
    }
}

int otter_arp_on(void) { return par[4][0] >> 5 != 0; }
void otter_arp_set(int on) { par[4][0] = (uint8_t)(on ? (par[4][0] >= 32u ? par[4][0] : 64u) : 0u); }
int otter_arp_rate(void) { return par[4][1] >> 5; }                 /* 0 1/8, 1 1/16, 2 1/8T, 3 1/32 */
void otter_arp_tick(void)                                           /* audio irq, from the app's clock */
{
    uint32_t mode = par[4][0] >> 5, octs = (par[4][2] >> 5) + 1u, total;
    if (arp_note >= 0) { voice_off(arp_note); arp_note = -1; }
    if (!mode || !nheld) return;
    total = nheld * octs;
    if (mode == 4u) { arp_i = (uint8_t)((rng = rng * 1664525u + 1013904223u) >> 24) % total; }
    else if (mode == 1u) arp_i = (uint8_t)((arp_i + 1u) % total);
    else if (mode == 2u) arp_i = (uint8_t)((arp_i + total - 1u) % total);
    else {                                                           /* up-down */
        if (total < 2u) arp_i = 0;
        else if (arp_dir == 0u) { if (++arp_i >= total - 1u) arp_dir = 1; }
        else { if (arp_i-- <= 1u) arp_dir = 0; }
    }
    if (arp_i >= total) arp_i = 0;
    arp_note = held[arp_i % nheld] + 12 * (int32_t)(arp_i / nheld);
    voice_on(arp_note, held_vel[arp_i % nheld]);
}

static void voice_on(int32_t note, uint32_t vel)
{
    uint32_t i, pick = NV, oldest = 0;
    int mono = par[4][3] >= 64u;
    if (mono) { pick = 0; for (i = 1; i < NV; i++) v[i].stage = v[i].stage ? 4 : 0; }
    for (i = 0; i < NV && pick == NV; i++) if (v[i].note == note && v[i].stage && v[i].stage < 4u) pick = i;
    for (i = 0; i < NV && pick == NV; i++) if (!v[i].stage) pick = i;
    for (i = 0; i < NV && pick == NV; i++) if (v[i].stage == 4u) pick = i;
    if (pick == NV) { for (i = 1; i < NV; i++) if (v[i].age < v[oldest].age) oldest = i; pick = oldest; }
    v[pick].note = note;
    v[pick].vel = (float)vel * (1.0f / 127.0f);
    v[pick].hz_target = note_hz(note);
    if (!(par[3][3] && (mono ? v[pick].stage && v[pick].stage < 4u : 1))) v[pick].hz = v[pick].hz_target;
    else if (v[pick].env < 0.01f) v[pick].hz = last_hz;
    last_hz = v[pick].hz_target;
    v[pick].stage = 1;
    v[pick].age = ++clock_;
    v[pick].drift = ((float)(int32_t)(rng = rng * 1664525u + 1013904223u) * (1.0f / 2147483648.0f));
    if (v[pick].env < 0.01f) { v[pick].ph1 = 0; v[pick].ph2 = 0.37f; v[pick].ph3 = 0.71f; v[pick].phs = 0; v[pick].env = 0; v[pick].fenv = 1.0f; }
    else v[pick].fenv = 1.0f;
}

static void voice_off(int32_t note)
{
    uint32_t i;
    for (i = 0; i < NV; i++) if (v[i].note == note && v[i].stage && v[i].stage < 4u) v[i].stage = 4;
}

static void o_render(float *out, uint32_t n)
{
    uint32_t i, k;
    pgain = fm_db2lin((float)GAIN_DB[preset_i]);
    uint8_t wave = (uint8_t)(par[0][2] / 21u); if (wave > 5u) wave = 5;
    float att = fm_exp2f(-5.0f - p01(par[1][0]) * 8.0f);
    float dec = fm_exp2f(-5.5f - p01(par[1][1]) * 7.5f);
    float sus = p01(par[1][2]) * p01(par[1][2]);
    float rel = fm_exp2f(-5.0f - p01(par[1][3]) * 8.0f);
    float fenv_amt = p01(par[2][0]) * 3.0f, fdec = fm_exp2f(-6.0f - p01(par[2][1]) * 7.0f);
    float velf = p01(par[2][2]), sub = p01(par[2][3]) * 0.7f;
    float lfo_inc = (0.1f + p01(par[3][0]) * p01(par[3][0]) * 12.0f) / SR;
    float vib = p01(par[3][1]) * p01(par[3][1]) * 0.06f, wob = p01(par[3][2]);
    float glide = par[3][3] ? 0.02f * fm_exp2f(-p01(par[3][3]) * 7.0f) : 1.0f;
    float lfo = fm_sinf(lfo_ph * 6.2831853f);
    float drive, spread, fmidx0;
    svfc_t fc;
    lfo_ph += lfo_inc * (float)n; if (lfo_ph >= 1.0f) lfo_ph -= 1.0f;
    sm_cut += (p01(par[0][0]) - sm_cut) * 0.3f;                      /* knobs smoothed over a few blocks */
    sm_res += (p01(par[0][1]) - sm_res) * 0.3f;
    sm_det += (p01(par[0][3]) - sm_det) * 0.3f;
    drive = 1.0f + sm_res * 1.2f;
    spread = fm_exp2f(sm_det * sm_det * 0.3f / 12.0f);
    fmidx0 = sm_det * 5.0f;
    while (pend_n) {
        pend_n--;
        if (otter_arp_on()) {
            if (pend_on[pend_n]) held_add(pend_note[pend_n], pend_vel[pend_n]); else held_del(pend_note[pend_n]);
        } else if (pend_on[pend_n]) voice_on(pend_note[pend_n], pend_vel[pend_n]);
        else voice_off(pend_note[pend_n]);
    }
    for (k = 0; k < NV; k++) {
        voice_t *w = &v[k];
        float dt1, dt2, dts, fbase, tilt, hz, fmidx, vl;
        if (!w->stage) continue;
        w->hz += (w->hz_target - w->hz) * (glide >= 1.0f ? 1.0f : glide * (float)n * 0.1f);
        /* drift: a slow wander of a few cents, different each voice */
        w->drift += (((float)(int32_t)(rng = rng * 1664525u + 1013904223u) * (1.0f / 2147483648.0f)) - w->drift * 0.02f) * 0.02f;
        hz = w->hz * (1.0f + lfo * vib + w->drift * 0.002f);
        dt1 = hz * spread * (1.0f / SR);
        dt2 = hz / spread * (1.0f / SR);
        dts = hz * (0.5f / SR);
        vl = 0.35f + 0.65f * w->vel;
        fbase = w->hz * (0.5f + sm_cut * sm_cut * 40.0f) * (1.0f + lfo * wob * 0.8f) * (1.0f - velf + velf * (0.3f + 0.7f * w->vel));
        fmidx = fmidx0 * (w->hz < 440.0f ? 1.0f : fm_exp2f(-0.5f * (float)(w->note - 69) / 12.0f));   /* less FM up high */
        tilt = fm_exp2f((60.0f - (float)w->note) * 0.15f / 12.0f);
        if (tilt > 1.6f) tilt = 1.6f;
        for (i = 0; i < n; i++) {
            float s, f, fhz, o1, o2;
            (void)fhz;
            if (w->stage == 1u) { w->env += (1.05f - w->env) * att; if (w->env >= 1.0f) { w->env = 1.0f; w->stage = 2; } }
            else if (w->stage == 2u) { w->env += (sus - w->env) * dec; if (w->env <= sus + 0.001f) w->stage = 3; }
            else if (w->stage == 4u) { w->env -= w->env * rel; if (w->env < 0.0005f) { w->stage = 0; w->env = 0; break; } }
            w->fenv -= w->fenv * fdec;                               /* the filter envelope: a decay from 1 */
            w->ph1 += dt1; if (w->ph1 >= 1.0f) w->ph1 -= 1.0f;
            w->ph2 += dt2; if (w->ph2 >= 1.0f) w->ph2 -= 1.0f;
            w->phs += dts; if (w->phs >= 1.0f) w->phs -= 1.0f;
            if (wave == 0u) {
                o1 = 2.0f * w->ph1 - 1.0f - polyblep(w->ph1, dt1);
                o2 = 2.0f * w->ph2 - 1.0f - polyblep(w->ph2, dt2);
            } else if (wave < 3u) {
                float pw = wave == 1u ? 0.25f : 0.5f, t1 = w->ph1 + pw, t2 = w->ph2 + pw;
                if (t1 >= 1.0f) t1 -= 1.0f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                o1 = (w->ph1 < pw ? 1.0f : -1.0f) + polyblep(w->ph1, dt1) - polyblep(t1, dt1);
                o2 = (w->ph2 < pw ? 1.0f : -1.0f) + polyblep(w->ph2, dt2) - polyblep(t2, dt2);
            } else if (wave == 3u) {
                o1 = 4.0f * fm_fabsf(w->ph1 - 0.5f) - 1.0f;
                o2 = 4.0f * fm_fabsf(w->ph2 - 0.5f) - 1.0f;
            } else if (wave == 4u) {
                float m = fm_sinf(w->ph2 * 2.0f * 6.2831853f) * fmidx * (0.15f + 0.85f * w->fenv);
                o1 = fm_sinf(w->ph1 * 6.2831853f + m);
                o2 = fm_sinf(w->ph2 * 6.2831853f + m * 0.6f);
            } else {                                                  /* SUPER: three saws, the third wider */
                float dt3 = dt1 * spread;
                w->ph3 += dt3; if (w->ph3 >= 1.0f) w->ph3 -= 1.0f;
                o1 = (2.0f * w->ph1 - 1.0f - polyblep(w->ph1, dt1)) * 0.7f + (2.0f * w->ph3 - 1.0f - polyblep(w->ph3, dt3)) * 0.5f;
                o2 = 2.0f * w->ph2 - 1.0f - polyblep(w->ph2, dt2);
            }
            s = (o1 + o2) * 0.5f + (w->phs < 0.5f ? sub : -sub) * (wave == 4u ? 0.5f : 1.0f);
            if ((i & 15u) == 0u) {                                    /* the cutoff follows the envelopes, updated every 16 samples */
                fhz = fbase * (0.2f + 0.8f * w->env) * (1.0f + fenv_amt * w->fenv);
                svf_coef(&fc, fhz, sm_res * 0.98f);
            }
            s = fm_soft(s * drive);
            f = svf_lp(&w->flt, &fc, s);
            if (sm_cut > 0.7f) f += s * (sm_cut - 0.7f) * 0.6f;      /* presence: some of the raw wave at high TONE */
            vsum[i] += f * (0.9f + sm_res * 0.3f) * w->env * vl * tilt * pgain * (wave == 4u ? 1.2f : 0.95f);
        }
    }
    {                                                                /* the ensemble: in with DETUNE past a third */
        float amt = sm_det > 0.35f ? (sm_det - 0.35f) / 0.65f : 0.0f;
        for (i = 0; i < n; i++) {
            float s = vsum[i], wet = 0.0f; uint32_t t;
            vsum[i] = 0.0f;
            if (amt > 0.0f) {
                { float cs = s > 1.0f ? 1.0f : s < -1.0f ? -1.0f : s; ens[ens_w] = (int16_t)(cs * 32767.0f); }
                ens_ph += 0.35f / SR; if (ens_ph >= 1.0f) ens_ph -= 1.0f;
                for (t = 0; t < 3u; t++) {
                    float ph = ens_ph + (float)t * 0.3333f, rd, d; uint32_t ri; int32_t a, b;
                    if (ph >= 1.0f) ph -= 1.0f;
                    rd = 440.0f + 200.0f * fm_sinf(6.2831853f * ph) + (float)t * 90.0f;   /* 10 .. 18 ms */
                    ri = (uint32_t)rd; d = rd - (float)ri;
                    a = ens[(ens_w + ENS_LEN - ri) % ENS_LEN]; b = ens[(ens_w + ENS_LEN - ri - 1u) % ENS_LEN];
                    wet += ((float)a + ((float)b - (float)a) * d) * (1.0f / 32768.0f);
                }
                ens_w = (ens_w + 1u) % ENS_LEN;
                s = s * (1.0f - amt * 0.45f) + wet * 0.4f * amt;
            }
            out[i] += fm_clip(s);
        }
    }
}

static const char *o_text(uint8_t page, uint32_t p, char *buf)
{
    (void)buf;
    if (page == 0u && p == 2u) { uint8_t w = (uint8_t)(par[0][2] / 21u); return WAVES[w > 5u ? 5u : w]; }
    if (page == 4u && p == 0u) return ARPS[par[4][0] >> 5];
    if (page == 4u && p == 1u) return RATES[par[4][1] >> 5];
    if (page == 4u && p == 2u) { static const char *const O[4] = {"1 OCT", "2 OCT", "3 OCT", "4 OCT"}; return O[par[4][2] >> 5]; }
    if (page == 4u && p == 3u) return par[4][3] >= 64u ? "MONO" : "POLY";
    return 0;
}

static const char *const TITLES[5] = {"SOUND", "ENVELOPE", "FILTER ENV", "MODULATION", "ARPEGGIO"};
const engine_t otter = {"NORDICS", o_init, o_note, o_render, o_panic, 5, {NAMES[0], NAMES[1], NAMES[2], NAMES[3], NAMES[4]},
                        {par[0], par[1], par[2], par[3], par[4]}, o_text, TITLES};
