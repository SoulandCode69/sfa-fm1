/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * FOX CUT & SAMPLE: one sample bank (mono 16-bit, 22.05 kHz, up to 1.3 s in RAM), played on the keys.
 *   CHROMATIC: every key plays the window START..END at the key's pitch (C4 = as recorded).
 *   CHOP: the window is cut into CHOPS equal slices; white key n plays slice n.
 *   page 1: K1 START  window start     K2 END   window end      K3 CHOPS  1 = chromatic, 2..16 slices   K4 PITCH
 *   page 2: K1 REV    play backwards   K2 LOOP  loop the window  K3 GAIN   level                       K4 (spare)
 * Loading: the bank is the firmware's resource region (fm1.toml resources = samples.bin, written over USB after an
 * install; tools/sfa_sample.py makes it from a WAV). Header "SFA1", 24-bit sample count, then int16 mono 22.05 kHz.
 * Without it (not written yet, another build's) a synthesised "mrrp" growl is generated so Fox always plays.
 * Cat Kit borrows eight slice voices from here (fox_slice, fox_slices_render). */
#include "fastmath.h"
#include "engine.h"
#include "plat.h"

#define BANK_MAX 46000u               /* samples: 2.08 s at 22050; 92 KB in the pool */
#define BANK_SR 22050.0f
PLAT_POOL static int16_t bank[BANK_MAX];
static uint32_t bank_len;

static const char *const NAMES[NPARAM] = {"START", "END", "CHOPS", "PITCH"};
static const char *const NAMES2[NPARAM] = {"REV", "LOOP", "GAIN", "-"};
static uint8_t par[NPARAM] = {0, 127, 0, 64};
static uint8_t par2[NPARAM] = {0, 0, 90, 0};

typedef struct { float pos, inc, env; uint32_t start, end; uint8_t on, loop; float vel; } voice_t;
#define NV 4
static voice_t v[NV];
#define NSLICE 8
static voice_t slice_v[NSLICE];        /* Cat Kit's sample pads: 8 slices of the bank */
static volatile uint8_t slice_trig[NSLICE];
static volatile uint8_t slice_vel[NSLICE];
static float slice_rate;
static volatile int32_t pend_note = -1;
static volatile uint32_t pend_vel;
static uint32_t rng = 0xC0FFEEu;

static void gen_mrrp(void)              /* a growly "mrrp": pitch-swept pulse through a swept formant, ~0.5 s */
{
    uint32_t i, n = 11025u;
    float ph = 0, lp = 0, bp = 0;
    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float hz = 70.0f + 90.0f * fm_sinf(t * 3.1415926f) + 25.0f * fm_sinf(t * 90.0f);   /* rrr: fast trill */
        float env = t < 0.05f ? t / 0.05f : (1.0f - t) * (1.0f - t) * 1.1f;
        float cut = 0.08f + 0.5f * fm_sinf(t * 3.1415926f);
        float s;
        ph += hz / BANK_SR;
        if (ph >= 1.0f) ph -= 1.0f;
        s = ph < 0.3f ? 1.0f : -0.4f;                    /* pulse */
        s += noise(&rng) * 0.15f;
        lp += (s - lp) * cut;
        bp += (lp - bp) * 0.3f;
        s = fm_tanhf((lp - bp * 0.5f) * 2.5f) * env * 0.9f;
        bank[i] = (int16_t)(s * 32000.0f);
    }
    bank_len = n;
}

static void f_init(void)
{
    uint32_t rlen = 0, i;
    const uint8_t *hdr = plat_res(&rlen);
    for (i = 0; i < NV; i++) v[i].on = 0;
    for (i = 0; i < NSLICE; i++) slice_v[i].on = 0;
    if (hdr && rlen >= 8u && hdr[0] == 'S' && hdr[1] == 'F' && hdr[2] == 'A' && hdr[3] == '1') {
        uint32_t len = (uint32_t)hdr[4] | (uint32_t)hdr[5] << 8 | (uint32_t)hdr[6] << 16;
        const int16_t *src = (const int16_t *)(hdr + 8);
        if (len > BANK_MAX) len = BANK_MAX;
        if (len && len * 2u + 8u <= rlen) {
            for (i = 0; i < len; i++) bank[i] = src[i];
            bank_len = len;
            return;
        }
    }
    gen_mrrp();
}

static void f_panic(void) { f_init(); }

static void f_note(int32_t note, uint32_t vel, int on)
{
    if (!on) return;                                       /* one-shots: a release does nothing */
    pend_vel = vel;
    pend_note = note;
}

static void start_voice(voice_t *w, int32_t note, float vel)
{
    uint32_t chops = par[2] >> 3;                          /* 0..15 -> 1..16 */
    float s0 = p01(par[0]), s1 = p01(par[1]);
    float semi = ((float)par[3] - 64.0f) * (48.0f / 127.0f);
    uint32_t a, b, len;
    if (s1 < s0 + 0.02f) s1 = s0 + 0.02f;
    if (s1 > 1.0f) s1 = 1.0f;
    a = (uint32_t)(s0 * (float)bank_len);
    b = (uint32_t)(s1 * (float)bank_len);
    len = b - a;
    if (chops >= 1u) {                                     /* CHOP: note = white key index 0..15 */
        uint32_t nc = chops + 1u, idx = (uint32_t)note % nc;
        a += len * idx / nc;
        b = a + len / nc;
        w->inc = fm_exp2f(semi * (1.0f / 12.0f)) * (BANK_SR / SR);
    } else {                                               /* CHROMATIC: C4 = as recorded */
        w->inc = fm_exp2f(((float)(note - 60) + semi) * (1.0f / 12.0f)) * (BANK_SR / SR);
    }
    w->start = a;
    w->end = b;
    w->loop = par2[1] >= 64u;
    if (par2[0] >= 64u) { w->inc = -w->inc; w->pos = (float)b - 2.0f; }
    else w->pos = (float)a;
    w->env = 1.0f;
    w->vel = vel * p01(par2[2]) * 1.4f;
    w->on = 1;
}

static inline float read_bank(float pos)
{
    uint32_t i = (uint32_t)pos;
    float f = pos - (float)i;
    float a = (float)bank[i], b = (float)bank[i + 1u < bank_len ? i + 1u : i];
    return (a + (b - a) * f) * (1.0f / 32768.0f);
}

static void run_voice(voice_t *w, float *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n && w->on; i++) {
        float s = read_bank(w->pos) * w->env * w->vel;
        out[i] += s;
        w->pos += w->inc;
        if (w->pos + 1.0f >= (float)w->end || w->pos < (float)w->start) {
            if (w->loop) {
                w->pos = w->inc > 0 ? (float)w->start : (float)w->end - 2.0f;
            } else {
                w->env *= 0.97f;                          /* a short fade at the window's end */
                if (w->env < 0.01f) w->on = 0;
                w->pos = w->inc > 0 ? (float)w->end - 1.5f : (float)w->start;
            }
        }
    }
}

int fox_chop_mode(void) { return par[2] >> 3 != 0; }
int fox_chops(void) { return (par[2] >> 3) + 1; }   /* 1 = chromatic */

static void f_render(float *out, uint32_t n)
{
    uint32_t i;
    int32_t note = pend_note;
    if (note >= 0) {
        uint32_t oldest = 0;
        pend_note = -1;
        for (i = 0; i < NV; i++) if (!v[i].on) { oldest = i; break; }
        start_voice(&v[oldest], note, (float)pend_vel * (1.0f / 127.0f));
    }
    for (i = 0; i < NV; i++) run_voice(&v[i], out, n);
}

/* Cat Kit's sample pads: the bank in 8 equal slices, each a one-shot voice of its own (main loop starts, render runs) */
void fox_slice(int slot, float rate, float vel)
{
    slot &= 7;
    slice_vel[slot] = (uint8_t)(vel * 127.0f);
    slice_rate = rate;
    slice_trig[slot] = 1;
}
void fox_slices_panic(void) { uint32_t i; for (i = 0; i < NSLICE; i++) slice_v[i].on = 0; }
void fox_slices_render(float *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < NSLICE; i++) {
        voice_t *w = &slice_v[i];
        if (slice_trig[i]) {
            uint32_t len = bank_len / NSLICE;
            slice_trig[i] = 0;
            w->start = len * i; w->end = w->start + len; w->pos = (float)w->start;
            w->inc = slice_rate * (BANK_SR / SR); w->loop = 0; w->env = 1.0f;
            w->vel = (float)slice_vel[i] * (1.0f / 127.0f) * 1.2f; w->on = 1;
        }
        run_voice(w, out, n);
    }
}

const engine_t fox = {"FOX", NAMES, par, f_init, f_note, f_render, f_panic, 2, NAMES2, par2, 0};
