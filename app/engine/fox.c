/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * FOX CUT & SAMPLE: one bank (mono 16-bit, 22.05 kHz, up to 4 s), played on the keys, 4 voices + the Cat slices.
 *   CHROMATIC (CHOPS = 1): every key plays the window START..END at the key's pitch (C4 = as recorded).
 *   CHOP (CHOPS 2..16):    the window cut into equal slices; white key n plays slice n.
 *   SAMPLE (the app's ARP button): records the groovebox's own output into the bank, up to 2 s, until ARP again.
 *   page 1: K1 START  K2 END  K3 CHOPS  K4 PITCH (semitones -24..+24)
 *   page 2: K1 REV    K2 LOOP  K3 FILTER (lowpass)  K4 DECAY (one-shot length; 127 = whole)
 * The bank comes from, in order: the app's own flash sample (saved with SAVE after a SAMPLE), the firmware's
 * resource region (tools/sfa_sample.py WAV -> samples.bin), or four built-in sounds spread across the bank. */
#include "fastmath.h"
#include "engine.h"
#include "plat.h"

#define BANK_MAX 92000u               /* 4.17 s at 22050; 184 KB in the pool */
#define BANK_SR 22050.0f
PLAT_POOL static int16_t bank[BANK_MAX];
static uint32_t bank_len;

static const char *const NAMES1[NPARAM] = {"START", "END", "CHOPS", "PITCH"};
static const char *const NAMES2[NPARAM] = {"REV", "LOOP", "FILTER", "DECAY"};
static uint8_t par1[NPARAM] = {0, 127, 0, 64};
static uint8_t par2[NPARAM] = {0, 0, 127, 127};

typedef struct { float pos, inc, env, dcoef, lp; uint32_t start, end; uint8_t on, loop; float vel; } voice_t;
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

/* ---- the built-in bank: four sounds, a second each: mrrp growl, "ah" vowel, a noise hit, a sine pluck */
static void gen_builtin(void)
{
    uint32_t i, n = 11025u, s;
    float ph = 0, lp = 0, bp = 0, f1 = 0, f2 = 0;
    for (s = 0; s < 4u; s++) {
        int16_t *o = bank + s * n * 2u;
        for (i = 0; i < n * 2u; i++) o[i] = 0;
        for (i = 0; i < n; i++) {
            float t = (float)i / (float)n, x = 0.0f, env;
            if (s == 0u) {                                               /* mrrp */
                float hz = 70.0f + 90.0f * fm_sinf(t * 3.1415926f) + 25.0f * fm_sinf(t * 90.0f);
                env = t < 0.05f ? t / 0.05f : (1.0f - t) * (1.0f - t) * 1.1f;
                ph += hz / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                x = (ph < 0.3f ? 1.0f : -0.4f) + noise(&rng) * 0.15f;
                lp += (x - lp) * (0.08f + 0.5f * fm_sinf(t * 3.1415926f)); bp += (lp - bp) * 0.3f;
                x = fm_tanhf((lp - bp * 0.5f) * 2.5f) * env * 0.9f;
            } else if (s == 1u) {                                        /* "ah": a saw through two formants */
                float hz = 110.0f * (1.0f + 0.01f * fm_sinf(t * 40.0f));
                env = t < 0.1f ? t / 0.1f : t > 0.7f ? (1.0f - t) / 0.3f : 1.0f;
                ph += hz / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                x = 2.0f * ph - 1.0f;
                f1 += (x - f1) * 0.11f; f2 += (f1 - f2) * 0.11f;         /* ~700 Hz */
                lp += (x - lp) * 0.3f; bp += (lp - bp) * 0.3f;            /* ~1.2 kHz */
                x = (f2 * 1.2f + bp * 0.8f) * env * 0.8f;
            } else if (s == 2u) {                                        /* noise hit */
                env = fm_exp2f(-t * 12.0f);
                x = noise(&rng);
                lp += (x - lp) * (0.1f + 0.6f * env);
                x = fm_tanhf(lp * 3.0f * env) * 0.9f;
            } else {                                                     /* sine pluck with a bit of 2nd harmonic */
                env = fm_exp2f(-t * 6.0f);
                ph += 220.0f / BANK_SR; if (ph >= 1.0f) ph -= 1.0f;
                x = (fm_sinf(ph * 6.2831853f) + 0.3f * fm_sinf(ph * 2.0f * 6.2831853f) * env) * env * 0.8f;
            }
            o[i] = (int16_t)(x * 30000.0f);
        }
    }
    bank_len = n * 8u;                                                   /* 4 s: four sounds, a second each, a second of room */
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

static void f_init(void)
{
    uint32_t i;
    for (i = 0; i < NV; i++) v[i].on = 0;
    rec_on = 0;
    if (!load_own() && !load_res()) gen_builtin();
}
static void f_panic(void) { uint32_t i; for (i = 0; i < NV; i++) v[i].on = 0; rec_on = 0; }

static void f_note(int32_t note, uint32_t vel, int on)
{
    if (!on) return;
    pend_vel = vel;
    pend_note = note;
}

int fox_chop_mode(void) { return par1[2] >> 3 != 0; }
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
            bank[rec_pos++] = (int16_t)(x * 32000.0f);
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
    float dec = p01(par2[3]);
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
    w->lp = 0;
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

static void run_voice(voice_t *w, float *out, uint32_t n, float fcoef)
{
    uint32_t i;
    for (i = 0; i < n && w->on; i++) {
        float s = read_bank(w->pos) * w->env * w->vel;
        w->lp += (s - w->lp) * fcoef;
        out[i] += w->lp;
        w->env *= w->dcoef;
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
    int32_t note = pend_note;
    float fl = p01(par2[2]);
    float fcoef = 0.02f + fl * fl * 0.98f;
    if (note >= 0) {
        uint32_t pick = 0;
        pend_note = -1;
        for (i = 0; i < NV; i++) if (!v[i].on) { pick = i; break; }
        if (i == NV) { for (i = 1; i < NV; i++) if (v[i].env < v[pick].env) pick = i; }
        start_voice(&v[pick], note, (float)pend_vel * (1.0f / 127.0f));
    }
    for (i = 0; i < NV; i++) run_voice(&v[i], out, n, fcoef);
}

const engine_t fox = {"FOX", f_init, f_note, f_render, f_panic, 2, {NAMES1, NAMES2, 0}, {par1, par2, 0}, 0};
