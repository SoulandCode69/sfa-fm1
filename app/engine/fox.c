/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * FOX CUT & SAMPLE + FOX BASS. The ALGO knob picks SAMPLER or one of eight bass presets.
 * BASS: a mono synth in the 303 / sub / reese manner: PolyBLEP saw or square + sub, a resonant lowpass with an
 *   envelope (ENVMOD / DECAY), ACCENT from velocity, legato GLIDE, DRIVE, an LFO wobble on the filter.
 *   page 1: CUTOFF RES ENVMOD DECAY     page 2: WAVE SUB GLIDE DRIVE     page 3: LFO WOBBLE ACCENT RELEASE
 * SAMPLER: one bank (mono 16-bit, 22.05 kHz, up to 4 s), played on the keys, 4 voices.
 *   CHROMATIC (CHOPS = 1): every key plays the window START..END at the key's pitch (C4 = as recorded).
 *   CHOP (CHOPS 2..16):    the window cut into equal slices; white key n plays slice n.
 *   SAMPLE (the app's ARP button): records the groovebox's own output into the bank, up to 2 s, until ARP again.
 *   page 1: K1 START  K2 END  K3 CHOPS  K4 PITCH (semitones -24..+24)
 *   page 2: K1 REV    K2 LOOP  K3 FILTER (resonant lowpass)  K4 RES
 *   page 3: K1 ATTACK K2 DECAY (127 = whole)  K3 GATE (key release stops the sound)  K4 ROLL (retrigger 1/16 .. 1/64)
 * The bank comes from, in order: the app's own flash sample (saved with SAVE after a SAMPLE), the firmware's
 * resource region (tools/sfa_sample.py WAV -> samples.bin), or four built-in sounds spread across the bank. */
#include "fastmath.h"
#include "engine.h"
#include "plat.h"

#define BANK_MAX 92000u               /* 4.17 s at 22050; 184 KB in the pool */
#define BANK_SR 22050.0f
PLAT_POOL static int16_t bank[BANK_MAX];
static uint32_t bank_len;

static const char *const SN1[NPARAM] = {"START", "END", "CHOPS", "PITCH"};
static const char *const SN2[NPARAM] = {"REV", "LOOP", "FILTER", "RES"};
static const char *const SN3[NPARAM] = {"ATTACK", "DECAY", "GATE", "ROLL"};
static const char *const BN1[NPARAM] = {"CUTOFF", "RES", "ENVMOD", "DECAY"};
static const char *const BN2[NPARAM] = {"WAVE", "SUB", "GLIDE", "DRIVE"};
static const char *const BN3[NPARAM] = {"LFO", "WOBBLE", "ACCENT", "RELEASE"};
static const char *NAMES1[NPARAM] = {"START", "END", "CHOPS", "PITCH"};   /* the live page names (mode) */
static const char *NAMES2[NPARAM] = {"REV", "LOOP", "FILTER", "RES"};
static const char *NAMES3[NPARAM] = {"ATTACK", "DECAY", "GATE", "ROLL"};
static uint8_t par1[NPARAM] = {0, 127, 0, 64};          /* the live params (mode) */
static uint8_t par2[NPARAM] = {0, 0, 127, 0};
static uint8_t par3[NPARAM] = {0, 127, 0, 0};
static uint8_t samp_par[3][NPARAM] = {{0, 127, 0, 64}, {0, 0, 127, 0}, {0, 127, 0, 0}};   /* each mode's own */
static uint8_t bass_par[3][NPARAM] = {{40, 70, 80, 45}, {0, 60, 20, 30}, {30, 0, 60, 20}};
static uint8_t mode;                                     /* 0 sampler, 1 bass */
static uint8_t preset_i;                                 /* 0 SAMPLER, 1..8 bass presets */
typedef struct { const char *name; uint8_t p[3][NPARAM]; } fpreset_t;
#define NFPRESET 9
static const fpreset_t FPRESETS[NFPRESET] = {
    /*                CUT RES ENV DEC    WAVE SUB GLI DRV   LFO WOB ACC REL */
    {"SAMPLER",     {{0}, {0}, {0}}},
    {"ACID 303",    {{38, 85, 95, 40},  {0, 0, 25, 40},    {30, 0, 90, 15}}},
    {"SQR ACID",    {{42, 80, 90, 45},  {127, 0, 25, 35},  {30, 0, 90, 15}}},
    {"SUB",         {{20, 10, 20, 70},  {127, 110, 10, 10}, {20, 0, 20, 30}}},
    {"REESE",       {{35, 30, 10, 90},  {0, 80, 0, 50},    {12, 10, 20, 40}}},
    {"PLUCK BASS",  {{55, 50, 100, 25}, {0, 60, 0, 30},    {30, 0, 60, 10}}},
    {"WOBBLE BASS", {{30, 75, 30, 80},  {0, 70, 0, 60},    {60, 110, 40, 30}}},
    {"GROWL",       {{45, 60, 60, 60},  {127, 50, 15, 110},{80, 70, 70, 25}}},
    {"DEEP HOUSE",  {{30, 35, 70, 35},  {0, 90, 0, 20},    {20, 0, 40, 20}}},
};
static void mode_apply(void)
{
    uint32_t g, p;
    const char *const *n1 = mode ? BN1 : SN1, *const *n2 = mode ? BN2 : SN2, *const *n3 = mode ? BN3 : SN3;
    uint8_t (*src)[NPARAM] = mode ? bass_par : samp_par;
    for (p = 0; p < NPARAM; p++) { NAMES1[p] = n1[p]; NAMES2[p] = n2[p]; NAMES3[p] = n3[p]; }
    for (g = 0; g < 3u; g++) for (p = 0; p < NPARAM; p++) (g == 0 ? par1 : g == 1 ? par2 : par3)[p] = src[g][p];
}
static void mode_stash(void)                             /* the live params back into the mode's own */
{
    uint32_t g, p;
    uint8_t (*dst)[NPARAM] = mode ? bass_par : samp_par;
    for (g = 0; g < 3u; g++) for (p = 0; p < NPARAM; p++) dst[g][p] = (g == 0 ? par1 : g == 1 ? par2 : par3)[p];
}
void fox_preset(int32_t step)
{
    uint32_t g, p;
    mode_stash();
    preset_i = (uint8_t)(((int32_t)preset_i + step + NFPRESET) % NFPRESET);
    mode = preset_i ? 1u : 0u;
    if (mode) for (g = 0; g < 3u; g++) for (p = 0; p < NPARAM; p++) bass_par[g][p] = FPRESETS[preset_i].p[g][p];
    mode_apply();
}
const char *fox_preset_name(void) { return FPRESETS[preset_i].name; }
int fox_mode(void) { return mode; }
int fox_preset_index(void) { return preset_i; }
void fox_preset_set(int i) { mode_stash(); preset_i = (uint8_t)(i % NFPRESET); mode = preset_i ? 1u : 0u; mode_apply(); }
void fox_mode_set_params(void) { mode_stash(); }        /* after the app restored the live params: keep them as the mode's */

typedef struct { float pos, inc, env, dcoef, att, aenv; svf_t flt; uint32_t start, end, roll, roll_n; int32_t note; uint8_t on, loop, gate; float vel; } voice_t;
#define NV 4
static voice_t v[NV];
static volatile int32_t pend_note = -1;
static volatile uint32_t pend_vel;
static uint32_t rng = 0xC0FFEEu;

/* recording: the app feeds the master mix (fox_record_feed) while fox_rec is on */
static volatile uint8_t rec_on;
static uint32_t rec_pos;
static float rec_acc;
static uint8_t rec_tick;
#define REC_MAX 44100u                 /* 2 s of bank */

/* ---- the built-in bank: eight sounds, half a second each: growl, "ah", "oo", noise hit, pluck, bell, bass hit, chord */
static void gen_builtin(void)
{
    uint32_t i, n = 11025u, k;
    float ph = 0, ph2 = 0, lp = 0, bp = 0, f1 = 0, f2 = 0;
    for (k = 0; k < 8u; k++) {
        int16_t *o = bank + k * n;
        ph = ph2 = lp = bp = f1 = f2 = 0;
        for (i = 0; i < n; i++) {
            float t = (float)i / (float)n, x = 0.0f, env;
            switch (k) {
            case 0:                                                      /* mrrp growl */
                { float hz = 70.0f + 90.0f * fm_sinf(t * 3.1415926f) + 25.0f * fm_sinf(t * 90.0f);
                env = t < 0.05f ? t / 0.05f : (1.0f - t) * (1.0f - t) * 1.1f;
                ph += hz / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                x = (ph < 0.3f ? 1.0f : -0.4f) + noise(&rng) * 0.15f;
                lp += (x - lp) * (0.08f + 0.5f * fm_sinf(t * 3.1415926f)); bp += (lp - bp) * 0.3f;
                x = fm_tanhf((lp - bp * 0.5f) * 2.5f) * env * 0.9f; } break;
            case 1: case 2:                                              /* "ah" / "oo": a saw through two formants */
                { float hz = 110.0f * (1.0f + 0.008f * fm_sinf(t * 40.0f));
                float c1 = k == 1u ? 0.11f : 0.05f, c2 = k == 1u ? 0.3f : 0.12f;
                env = t < 0.1f ? t / 0.1f : t > 0.7f ? (1.0f - t) / 0.3f : 1.0f;
                ph += hz / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                x = 2.0f * ph - 1.0f;
                f1 += (x - f1) * c1; f2 += (f1 - f2) * c1;
                lp += (x - lp) * c2; bp += (lp - bp) * c2;
                x = (f2 * 1.2f + bp * 0.8f) * env * 0.8f; } break;
            case 3:                                                      /* noise hit */
                env = fm_exp2f(-t * 12.0f);
                x = noise(&rng);
                lp += (x - lp) * (0.1f + 0.6f * env);
                x = fm_tanhf(lp * 3.0f * env) * 0.9f; break;
            case 4:                                                      /* pluck */
                env = fm_exp2f(-t * 6.0f);
                ph += 220.0f / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                x = (fm_sinf(ph * 6.2831853f) + 0.3f * fm_sinf(ph * 2.0f * 6.2831853f) * env) * env * 0.8f; break;
            case 5:                                                      /* FM bell */
                env = fm_exp2f(-t * 4.0f);
                ph += 440.0f / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                ph2 += 440.0f * 1.41f / BANK_SR; if (ph2 >= 1.0f) ph2 -= 1.0f;
                x = fm_sinf(ph * 6.2831853f + fm_sinf(ph2 * 6.2831853f) * 2.5f * env) * env * 0.8f; break;
            case 6:                                                      /* bass hit: saw into a closing filter */
                env = fm_exp2f(-t * 5.0f);
                ph += 55.0f / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                x = 2.0f * ph - 1.0f;
                lp += (x - lp) * (0.05f + 0.4f * env); bp += (lp - bp) * (0.05f + 0.4f * env);
                x = fm_tanhf(bp * 2.0f) * env * 0.9f; break;
            default:                                                     /* a minor chord of pulses */
                env = t < 0.05f ? t / 0.05f : fm_exp2f(-(t - 0.05f) * 3.0f);
                ph += 220.0f / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                ph2 += 261.6f / BANK_SR; if (ph2 >= 1.0f) ph2 -= 1.0f;
                f1 += 329.6f / BANK_SR; if (f1 >= 1.0f) f1 -= 1.0f;
                x = ((ph < 0.3f ? 1.0f : -0.43f) + (ph2 < 0.3f ? 1.0f : -0.43f) + (f1 < 0.3f ? 1.0f : -0.43f)) * 0.3f;
                lp += (x - lp) * 0.25f;
                x = lp * env * 0.8f; break;
            }
            o[i] = (int16_t)(x * 30000.0f);
        }
    }
    bank_len = n * 8u;
}

/* ---- the app's own flash sample: data area 0xC5000..0xDC000 (92 KiB), header "SFAS" + len24 */
#define OWN_LO 0xC5000u
#define OWN_HI 0xDC000u
static int load_own(void)
{
    const uint8_t *h = plat_data(OWN_LO);
    uint32_t len, i;
    const int16_t *src;
    if (!h || h[0] != 'S' || h[1] != 'F' || h[2] != 'A' || h[3] != 'S') return 0;
    len = (uint32_t)h[4] | (uint32_t)h[5] << 8 | (uint32_t)h[6] << 16;
    if (!len || len > REC_MAX || len * 2u + 8u > OWN_HI - OWN_LO) return 0;
    src = (const int16_t *)(h + 8);
    for (i = 0; i < len; i++) bank[i] = src[i];
    bank_len = len;
    return 1;
}

static uint8_t own_dirty;                /* a SAMPLE not yet in flash */
int fox_save_needed(void) { return own_dirty; }
int fox_save(void)                       /* main loop: write the bank to the app's own region. 0 = ok */
{
    uint8_t hdr[8] = {'S', 'F', 'A', 'S', 0, 0, 0, 0};
    uint32_t len = bank_len > REC_MAX ? REC_MAX : bank_len, off, bytes = 8u + len * 2u, w;
    hdr[4] = (uint8_t)len; hdr[5] = (uint8_t)(len >> 8); hdr[6] = (uint8_t)(len >> 16);
    for (off = 0; off < bytes; off += 4096u)
        if (plat_data_erase(OWN_LO + off)) return -1;
    if (plat_data_write(OWN_LO, hdr, 8)) return -1;
    for (w = 0; w < len * 2u; w += 2048u) {
        uint32_t n = len * 2u - w; if (n > 2048u) n = 2048u;
        if (plat_data_write(OWN_LO + 8u + w, (const uint8_t *)bank + w, n)) return -1;
    }
    own_dirty = 0;
    return 0;
}

static int load_res(void)
{
    uint32_t rlen = 0, len, i;
    const uint8_t *hdr = plat_res(&rlen);
    const int16_t *src;
    if (!hdr || rlen < 8u || hdr[0] != 'S' || hdr[1] != 'F' || hdr[2] != 'A' || hdr[3] != '1') return 0;
    len = (uint32_t)hdr[4] | (uint32_t)hdr[5] << 8 | (uint32_t)hdr[6] << 16;
    src = (const int16_t *)(hdr + 8);
    if (len > BANK_MAX) len = BANK_MAX;
    if (!len || len * 2u + 8u > rlen) return 0;
    for (i = 0; i < len; i++) bank[i] = src[i];
    bank_len = len;
    return 1;
}

/* ---- the bass synth */
static struct {
    float hz, hz_target, ph, phs, env, aenv, lfo_ph;
    int32_t held[8]; uint8_t nheld;
    int32_t note; uint8_t gate, accent;
    svf_t flt;
    float sm_cut;
} bs;

static void bass_note(int32_t note, uint32_t vel, int on)
{
    uint32_t i, j;
    if (on) {
        for (i = 0; i < bs.nheld; i++) if (bs.held[i] == note) break;
        if (i == bs.nheld && bs.nheld < 8u) bs.held[bs.nheld++] = note;
        bs.hz_target = note_hz(note);
        if (!bs.gate) { bs.hz = bs.hz_target; bs.env = 1.0f; bs.aenv = 0.0f; }   /* a new note: no glide, retrigger */
        else if (bs.nheld == 1u) bs.hz = bs.hz_target;
        bs.gate = 1; bs.note = note;
        bs.accent = vel > 100u;
        if (!bs.gate || bs.nheld <= 1u) { bs.env = 1.0f; }
    } else {
        for (i = 0; i < bs.nheld; i++) if (bs.held[i] == note) { for (j = i; j + 1u < bs.nheld; j++) bs.held[j] = bs.held[j + 1]; bs.nheld--; break; }
        if (bs.nheld) { bs.note = bs.held[bs.nheld - 1]; bs.hz_target = note_hz(bs.note); }   /* back to the last held */
        else bs.gate = 0;
    }
}

static void bass_render(float *out, uint32_t n)
{
    uint32_t i;
    float cut = p01(par1[0]), res = p01(par1[1]) * 0.97f, envmod = p01(par1[2]), dec = decay_coef(0.03f + p01(par1[3]) * p01(par1[3]) * 2.0f);
    int sqr = par2[0] >= 64; float sub = p01(par2[1]) * 0.8f, glide = par2[2] ? 0.03f * fm_exp2f(-p01(par2[2]) * 6.0f) : 1.0f;
    float drive = 1.0f + p01(par2[3]) * p01(par2[3]) * 8.0f, dnorm = 1.0f / fm_tanhf(drive * 0.7f);
    float lfo_inc = (0.2f + p01(par3[0]) * p01(par3[0]) * 14.0f) / SR, wob = p01(par3[1]), acc = p01(par3[2]);
    float rel = decay_coef(0.005f + p01(par3[3]) * 0.4f);
    float lfo, dt, dts, fbase, accg = bs.accent ? 1.0f + acc : 1.0f;
    svfc_t fc;
    bs.sm_cut += (cut - bs.sm_cut) * 0.3f;
    lfo = fm_sinf(bs.lfo_ph * 6.2831853f); bs.lfo_ph += lfo_inc * (float)n; if (bs.lfo_ph >= 1.0f) bs.lfo_ph -= 1.0f;
    bs.hz += (bs.hz_target - bs.hz) * (glide >= 1.0f ? 1.0f : glide * (float)n * 0.1f);
    if (glide >= 1.0f) bs.hz = bs.hz_target;
    dt = bs.hz * (1.0f / SR); dts = dt * 0.5f;
    fbase = 60.0f * fm_exp2f(bs.sm_cut * 7.0f) * (1.0f + lfo * wob * 0.9f);        /* 60 Hz .. 7.7 kHz */
    for (i = 0; i < n; i++) {
        float o, s;
        if (!bs.gate && bs.aenv < 0.0005f) break;
        if (bs.gate) { bs.aenv += (1.0f - bs.aenv) * 0.02f; } else { bs.aenv *= rel; }
        bs.env *= dec;
        bs.ph += dt; if (bs.ph >= 1.0f) bs.ph -= 1.0f;
        bs.phs += dts; if (bs.phs >= 1.0f) bs.phs -= 1.0f;
        if (sqr) { float t2 = bs.ph + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f; o = (bs.ph < 0.5f ? 1.0f : -1.0f) + polyblep(bs.ph, dt) - polyblep(t2, dt); }
        else o = 2.0f * bs.ph - 1.0f - polyblep(bs.ph, dt);
        o = o * 0.8f + fm_sinf(bs.phs * 6.2831853f) * sub;
        if ((i & 15u) == 0u) svf_coef(&fc, fbase * (1.0f + envmod * envmod * 24.0f * bs.env * accg), res);
        s = svf_lp(&bs.flt, &fc, o);
        s = fm_tanhf(s * (1.0f + res * 0.8f) * 0.7f * drive) * dnorm;
        out[i] += s * bs.aenv * (bs.accent ? 1.0f + acc * 0.4f : 1.0f) * 0.42f;
    }
    if (!bs.gate && bs.aenv < 0.0005f) { bs.flt.ic1 = bs.flt.ic2 = 0; }
}

static void f_init(void)
{
    uint32_t i;
    for (i = 0; i < NV; i++) v[i].on = 0;
    rec_on = 0;
    bs.gate = 0; bs.nheld = 0; bs.aenv = 0; bs.hz = bs.hz_target = 55.0f;
    mode_apply();
    if (!load_own() && !load_res()) gen_builtin();
    {   /* a silent bank (a bad transfer, an empty region) is no use: the built-in one instead */
        int32_t pk = 0;
        for (i = 0; i < bank_len; i += 7u) { int32_t a = bank[i] < 0 ? -bank[i] : bank[i]; if (a > pk) pk = a; }
        if (pk < 200) gen_builtin();
    }
}
/* for the app's waveform: the bank's peak in a window */
uint32_t fox_bank_len(void) { return bank_len; }
int32_t fox_peak(uint32_t from, uint32_t to)
{
    int32_t pk = 0; uint32_t i, step = (to - from) / 24u + 1u;
    if (to > bank_len) to = bank_len;
    for (i = from; i < to; i += step) { int32_t a = bank[i] < 0 ? -bank[i] : bank[i]; if (a > pk) pk = a; }
    return pk;
}
uint32_t fox_window(uint32_t *a, uint32_t *b)
{
    float s0 = p01(par1[0]), s1 = p01(par1[1]);
    if (s1 < s0 + 0.02f) s1 = s0 + 0.02f;
    if (s1 > 1.0f) s1 = 1.0f;
    *a = (uint32_t)(s0 * (float)bank_len); *b = (uint32_t)(s1 * (float)bank_len);
    return (par1[2] >> 3) + 1u;
}
float fox_playhead(void)                  /* 0..1 of the bank, the loudest voice; -1 none */
{
    uint32_t i; int best = -1;
    for (i = 0; i < NV; i++) if (v[i].on && (best < 0 || v[i].env > v[best].env)) best = (int)i;
    return best < 0 ? -1.0f : v[best].pos / (float)bank_len;
}
static void f_panic(void) { uint32_t i; for (i = 0; i < NV; i++) v[i].on = 0; rec_on = 0; bs.gate = 0; bs.nheld = 0; bs.aenv = 0; }

static volatile int32_t pend_off = -1;
static volatile int32_t bq_note[8]; static volatile uint8_t bq_vel[8], bq_on[8], bq_n;
static void f_note(int32_t note, uint32_t vel, int on)
{
    if (mode) {                                                     /* bass: queued for the render */
        uint8_t k = bq_n; if (k >= 8u) return;
        bq_note[k] = note; bq_vel[k] = (uint8_t)vel; bq_on[k] = (uint8_t)on; bq_n = (uint8_t)(k + 1u);
        return;
    }
    if (!on) { pend_off = note; return; }
    pend_vel = vel;
    pend_note = note;
}

int fox_chop_mode(void) { return !mode && par1[2] >> 3 != 0; }
int fox_chops(void) { return (par1[2] >> 3) + 1; }
int fox_recording(void) { return rec_on; }
uint32_t fox_bank_seconds_x10(void) { return bank_len * 10u / 22050u; }

/* ---- SAMPLE: the app calls these */
void fox_record_start(void) { rec_pos = 0; rec_acc = 0; rec_tick = 0; rec_on = 1; }
void fox_record_stop(void)
{
    if (!rec_on) return;
    rec_on = 0;
    if (rec_pos > 2205u) { bank_len = rec_pos; own_dirty = 1; }       /* at least 0.1 s, else keep the old bank */
}
void fox_record_feed(const float *mix, uint32_t n)                    /* audio irq: the master mix, halved in rate */
{
    uint32_t i;
    if (!rec_on) return;
    for (i = 0; i < n && rec_pos < REC_MAX; i++) {
        rec_acc += mix[i];
        if (rec_tick ^= 1u) {
            float x = rec_acc * 0.5f;
            if (x > 1.0f) x = 1.0f; else if (x < -1.0f) x = -1.0f;
            bank[rec_pos++] = (int16_t)(x * 24000.0f);
            rec_acc = 0;
        }
    }
    if (rec_pos >= REC_MAX) fox_record_stop();
}

static void start_voice(voice_t *w, int32_t note, float vel)
{
    uint32_t chops = par1[2] >> 3;
    float s0 = p01(par1[0]), s1 = p01(par1[1]);
    float semi = ((float)par1[3] - 64.0f) * (48.0f / 127.0f);
    float dec = p01(par3[1]), att = p01(par3[0]);
    uint32_t a, b, len;
    if (s1 < s0 + 0.02f) s1 = s0 + 0.02f;
    if (s1 > 1.0f) s1 = 1.0f;
    a = (uint32_t)(s0 * (float)bank_len);
    b = (uint32_t)(s1 * (float)bank_len);
    len = b - a;
    if (chops >= 1u) {
        uint32_t nc = chops + 1u, idx = (uint32_t)note % nc;
        a += len * idx / nc;
        b = a + len / nc;
        w->inc = fm_exp2f(semi * (1.0f / 12.0f)) * (BANK_SR / SR);
    } else {
        w->inc = fm_exp2f(((float)(note - 60) + semi) * (1.0f / 12.0f)) * (BANK_SR / SR);
    }
    w->start = a; w->end = b;
    w->loop = par2[1] >= 64u;
    if (par2[0] >= 64u) { w->inc = -w->inc; w->pos = (float)b - 2.0f; } else w->pos = (float)a;
    w->env = 1.0f;
    w->dcoef = dec > 0.99f ? 1.0f : decay_coef(0.02f + dec * dec * 2.0f);
    w->aenv = att > 0.0f ? 0.0f : 1.0f;
    w->att = att > 0.0f ? 1.0f / (0.002f + att * att * 1.5f) / SR : 1.0f;
    w->flt.ic1 = w->flt.ic2 = 0;
    w->note = note;
    w->gate = par3[2] >= 64u;
    w->roll_n = par3[3] >> 5;                                       /* 0 off, 1 = 1/16, 2 = 1/32, 3 = 1/64 of 120 bpm */
    w->roll = w->roll_n ? (uint32_t)(SR * 0.125f / (float)(1u << (w->roll_n - 1u))) : 0;
    w->vel = vel * 1.3f;
    w->on = 1;
}

static inline float read_bank(float pos)
{
    uint32_t i = (uint32_t)pos;
    float f = pos - (float)i;
    float a = (float)bank[i], b = (float)bank[i + 1u < bank_len ? i + 1u : i];
    return (a + (b - a) * f) * (1.0f / 32768.0f);
}

static void run_voice(voice_t *w, float *out, uint32_t n, const svfc_t *fc)
{
    uint32_t i;
    for (i = 0; i < n && w->on; i++) {
        float s = read_bank(w->pos) * w->env * w->vel * w->aenv;
        out[i] += svf_lp(&w->flt, fc, s);
        w->env *= w->dcoef;
        if (w->aenv < 1.0f) { w->aenv += w->att; if (w->aenv > 1.0f) w->aenv = 1.0f; }
        if (w->roll && --w->roll == 0u) {                                  /* ROLL: back to the start */
            w->roll = (uint32_t)(SR * 0.125f / (float)(1u << (w->roll_n - 1u)));
            w->pos = w->inc > 0 ? (float)w->start : (float)w->end - 2.0f; w->env = 1.0f;
        }
        w->pos += w->inc;
        if (w->pos + 1.0f >= (float)w->end || w->pos < (float)w->start) {
            if (w->loop) w->pos = w->inc > 0 ? (float)w->start : (float)w->end - 2.0f;
            else { w->env *= 0.95f; w->pos = w->inc > 0 ? (float)w->end - 1.5f : (float)w->start; }
        }
        if (w->env < 0.002f) w->on = 0;
    }
}

static void f_render(float *out, uint32_t n)
{
    uint32_t i;
    int32_t note = pend_note, off = pend_off;
    while (bq_n) { bq_n--; bass_note(bq_note[bq_n], bq_vel[bq_n], bq_on[bq_n]); }
    if (mode) { bass_render(out, n); return; }
    float fl = p01(par2[2]), fhz = 60.0f * fm_exp2f(fl * 8.0f);     /* 60 Hz .. 15 kHz */
    svfc_t fc;
    svf_coef(&fc, fhz, p01(par2[3]) * 0.95f);
    if (off >= 0) {
        pend_off = -1;
        for (i = 0; i < NV; i++) if (v[i].on && v[i].gate && v[i].note == off) v[i].dcoef = 0.9992f;   /* a 15 ms release */
    }
    if (note >= 0) {
        uint32_t pick = 0;
        pend_note = -1;
        for (i = 0; i < NV; i++) if (!v[i].on) { pick = i; break; }
        if (i == NV) { for (i = 1; i < NV; i++) if (v[i].env < v[pick].env) pick = i; }
        start_voice(&v[pick], note, (float)pend_vel * (1.0f / 127.0f));
    }
    for (i = 0; i < NV; i++) run_voice(&v[i], out, n, &fc);
}

static const char *f_text(uint8_t page, uint32_t p, char *buf)
{
    static const char *const ROLLS[4] = {"OFF", "1/16", "1/32", "1/64"};
    (void)buf;
    if (mode) return page == 1u && p == 0u ? (par2[0] >= 64u ? "SQUARE" : "SAW") : 0;
    if (page == 2u && p == 2u) return par3[2] >= 64u ? "GATE" : "1-SHOT";
    if (page == 2u && p == 3u) return ROLLS[par3[3] >> 5];
    return 0;
}
const engine_t fox = {"FOX", f_init, f_note, f_render, f_panic, 3, {NAMES1, NAMES2, NAMES3, 0, 0}, {par1, par2, par3, 0, 0}, f_text};
