/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * HOPPER CUT: the slicer. One bank (mono 16-bit, 22.05 kHz, up to 4 s): SAMPLE it from the groovebox's own output
 * (the ARP button), load it over USB (tools/sfa_sample.py), or use the eight built-in sounds.
 *   CHROMATIC (CHOPS = 1): every key plays the window START..END at the key's pitch (C4 = as recorded).
 *   CHOP (CHOPS 2..16): the window cut into slices: EQUAL, or at TRANSIENTS (CHOPMODE, SENS); white key n = slice n.
 *   Every slice its own S.PITCH / S.REV / S.DECAY / S.LEVEL (page 4 edits the slice you hit last).
 *   page 1: START END CHOPS PITCH   page 2: REV LOOP FILTER RES   page 3: ATTACK DECAY GATE ROLL
 *   page 4: the slice             page 5: CHOPMODE SENS */
#include "fastmath.h"
#include "engine.h"
#include "plat.h"

#define BANK_MAX 92000u               /* 4.17 s at 22050; 184 KB in the pool */
#define BANK_SR 22050.0f
PLAT_POOL static int16_t bank[BANK_MAX];
static uint32_t bank_len;

static const char *const NAMES1[NPARAM] = {"START", "END", "CHOPS", "PITCH"};
static const char *const NAMES2[NPARAM] = {"REV", "LOOP", "FILTER", "RES"};
static const char *const NAMES3[NPARAM] = {"ATTACK", "DECAY", "GATE", "ROLL"};
static const char *const NAMES4[NPARAM] = {"S.PITCH", "S.REV", "S.DECAY", "S.LEVEL"};
static const char *const NAMES5[NPARAM] = {"CHOPMODE", "SENS", "-", "-"};   /* MANUAL: hold a slice key, turn K1 to move its start */
static uint8_t par1[NPARAM] = {0, 127, 0, 64};
static uint8_t par2[NPARAM] = {0, 0, 127, 0};
static uint8_t par3[NPARAM] = {0, 127, 0, 0};
static uint8_t par4[NPARAM] = {64, 0, 127, 100};      /* a view of the selected slice's params */
static uint8_t par5[NPARAM] = {0, 64, 0, 0};
#define NSLICES 16
static uint8_t slice_par[NSLICES][NPARAM];            /* S.PITCH S.REV S.DECAY S.LEVEL per slice (saved by the app) */
static uint32_t slice_pt[NSLICES + 1];                /* slice boundaries (samples into the bank), transient mode */
static uint8_t nslice_pt;
static volatile int8_t last_slice = 0;
static uint8_t shown_slice = 0;
typedef struct { float pos, inc, env, dcoef, att, aenv; svf_t flt; uint32_t start, end, roll, roll_n; int32_t note; uint8_t on, loop, gate; float vel; } voice_t;
#define NV 4
static voice_t v[NV];
#define NTAIL 2
static voice_t tail[NTAIL]; static uint8_t tail_i;
static volatile int32_t pend_note = -1;
static volatile uint32_t pend_vel;
static uint32_t rng = 0xC0FFEEu;

/* recording: the app feeds the master mix (hop_record_feed) while hop_rec is on */
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
#define HOP_LO 0xC5000u
#define HOP_HI 0xDC000u
static int load_own(void)
{
    const uint8_t *h = plat_data(HOP_LO);
    uint32_t len, i;
    const int16_t *src;
    if (!h || h[0] != 'S' || h[1] != 'F' || h[2] != 'A' || h[3] != 'S') return 0;
    len = (uint32_t)h[4] | (uint32_t)h[5] << 8 | (uint32_t)h[6] << 16;
    if (!len || len > REC_MAX || len * 2u + 8u > HOP_HI - HOP_LO) return 0;
    src = (const int16_t *)(h + 8);
    for (i = 0; i < len; i++) bank[i] = src[i];
    bank_len = len;
    return 1;
}

static uint8_t own_dirty;                /* a SAMPLE not yet in flash */
int hop_save_needed(void) { return own_dirty; }
int hop_save(void)                       /* main loop: write the bank to the app's own region. 0 = ok */
{
    uint8_t hdr[8] = {'S', 'F', 'A', 'S', 0, 0, 0, 0};
    uint32_t len = bank_len > REC_MAX ? REC_MAX : bank_len, off, bytes = 8u + len * 2u, w;
    hdr[4] = (uint8_t)len; hdr[5] = (uint8_t)(len >> 8); hdr[6] = (uint8_t)(len >> 16);
    for (off = 0; off < bytes; off += 4096u)
        if (plat_data_erase(HOP_LO + off)) return -1;
    if (plat_data_write(HOP_LO, hdr, 8)) return -1;
    for (w = 0; w < len * 2u; w += 2048u) {
        uint32_t n = len * 2u - w; if (n > 2048u) n = 2048u;
        if (plat_data_write(HOP_LO + 8u + w, (const uint8_t *)bank + w, n)) return -1;
    }
    own_dirty = 0;
    return 0;
}

/* the user bank in the resource region (tools/sfa_sample.py): "SFA1" + len24 + PCM = one sample, or
 * "SFA2" + count + 3 pad + count x { off u32, len u32, name[12] } + PCM = up to 16 named samples, all 22.05 kHz 16-bit */
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t res_count(void)
{
    uint32_t rlen = 0; const uint8_t *h = plat_res(&rlen);
    if (!h || rlen < 8u || h[0] != 'S' || h[1] != 'F' || h[2] != 'A') return 0;
    if (h[3] == '1') return 1;
    if (h[3] == '2') return h[4] > 16u ? 16u : h[4];
    return 0;
}
static const char *res_name(uint32_t i, char *buf)
{
    uint32_t rlen = 0; const uint8_t *h = plat_res(&rlen);
    if (!h || h[3] != '2' || i >= res_count()) return "USER";
    { uint32_t k; const uint8_t *e = h + 8u + i * 20u; for (k = 0; k < 12u && e[8 + k]; k++) buf[k] = (char)e[8 + k]; buf[k] = 0; }
    return buf;
}
static int load_res(uint32_t which)
{
    uint32_t rlen = 0, len, i, off;
    const uint8_t *hdr = plat_res(&rlen);
    const int16_t *src;
    if (!hdr || rlen < 8u || hdr[0] != 'S' || hdr[1] != 'F' || hdr[2] != 'A') return 0;
    if (hdr[3] == '1') { len = (uint32_t)hdr[4] | (uint32_t)hdr[5] << 8 | (uint32_t)hdr[6] << 16; off = 8u; }
    else if (hdr[3] == '2') {
        const uint8_t *e;
        if (which >= res_count()) return 0;
        e = hdr + 8u + which * 20u; off = u32(e); len = u32(e + 4);
    } else return 0;
    if (len > BANK_MAX) len = BANK_MAX;
    if (!len || off + len * 2u > rlen || off & 1u) return 0;
    src = (const int16_t *)(hdr + off);
    for (i = 0; i < len; i++) bank[i] = src[i];
    bank_len = len;
    return 1;
}
static uint8_t source;                      /* 0 built-in, 1 own, 2.. the user bank's samples */
static uint32_t sources(void) { return 2u + res_count(); }
static int load_source(uint32_t s)
{
    if (s == 0u) { gen_builtin(); return 1; }
    if (s == 1u) return load_own();
    return load_res(s - 2u);
}
/* the app: ALGO browses the sources; loads it and chops it afresh */
const char *hop_source_name(char *buf)
{
    if (source == 0u) return "BUILT-IN";
    if (source == 1u) return "OWN";
    return res_name(source - 2u, buf);
}
int hop_source(int32_t step)
{
    uint32_t n = sources(), tries = n;
    while (tries--) {
        source = (uint8_t)(((int32_t)source + step + (int32_t)n) % (int32_t)n);
        if (load_source(source)) { nslice_pt = 0; return 1; }
        if (step == 0) step = 1;
    }
    return 0;
}
int hop_source_index(void) { return source; }
void hop_source_set(int s) { source = (uint8_t)s; if (!load_source(source)) { source = 0; gen_builtin(); } nslice_pt = 0; }

static void f_init(void)
{
    uint32_t i;
    for (i = 0; i < NV; i++) v[i].on = 0;
    rec_on = 0;
    if (load_own()) source = 1; else if (load_res(0)) source = 2; else { gen_builtin(); source = 0; }
    {   /* a silent bank (a bad transfer, an empty region) is no use: the built-in one instead */
        int32_t pk = 0;
        for (i = 0; i < bank_len; i += 7u) { int32_t a = bank[i] < 0 ? -bank[i] : bank[i]; if (a > pk) pk = a; }
        if (pk < 200) gen_builtin();
    }
}
/* for the app's waveform: the bank's peak in a window */
uint32_t hop_bank_len(void) { return bank_len; }
int32_t hop_peak(uint32_t from, uint32_t to)
{
    int32_t pk = 0; uint32_t i, step = (to - from) / 24u + 1u;
    if (to > bank_len) to = bank_len;
    for (i = from; i < to; i += step) { int32_t a = bank[i] < 0 ? -bank[i] : bank[i]; if (a > pk) pk = a; }
    return pk;
}
uint32_t hop_window(uint32_t *a, uint32_t *b)
{
    float s0 = p01(par1[0]), s1 = p01(par1[1]);
    if (s1 < s0 + 0.02f) s1 = s0 + 0.02f;
    if (s1 > 1.0f) s1 = 1.0f;
    *a = (uint32_t)(s0 * (float)bank_len); *b = (uint32_t)(s1 * (float)bank_len);
    return (par1[2] >> 3) + 1u;
}
float hop_playhead(void)                  /* 0..1 of the bank, the loudest voice; -1 none */
{
    uint32_t i; int best = -1;
    for (i = 0; i < NV; i++) if (v[i].on && (best < 0 || v[i].env > v[best].env)) best = (int)i;
    return best < 0 ? -1.0f : v[best].pos / (float)bank_len;
}
static void f_panic(void) { uint32_t i; for (i = 0; i < NV; i++) v[i].on = 0; rec_on = 0; }

static volatile int32_t pend_off = -1;
static void f_note(int32_t note, uint32_t vel, int on)
{
    if (!on) { pend_off = note; return; }
    pend_vel = vel;
    pend_note = note;
}

int hop_chop_mode(void) { return par1[2] >> 3 != 0; }
uint8_t *hop_slice_blob(uint32_t *size) { *size = sizeof slice_par; return &slice_par[0][0]; }
void hop_slices_default(void) { uint32_t i; for (i = 0; i < NSLICES; i++) { slice_par[i][0] = 64; slice_par[i][1] = 0; slice_par[i][2] = 127; slice_par[i][3] = 100; } }
int hop_last_slice(void) { return last_slice; }
int hop_slice_active(uint32_t idx) { uint32_t i; for (i = 0; i < NV; i++) if (v[i].on && (uint32_t)(v[i].note & 15) == idx) return 1; return 0; }
void hop_sync(void)                                     /* main loop, every frame: page 4 is the slice's */
{
    uint32_t i;
    if ((uint8_t)last_slice != shown_slice) { shown_slice = (uint8_t)last_slice; for (i = 0; i < NPARAM; i++) par4[i] = slice_par[shown_slice][i]; }
    else for (i = 0; i < NPARAM; i++) slice_par[shown_slice][i] = par4[i];
}
void hop_rechop(void) { nslice_pt = 0; }                /* the app: after a new sample or a SENS change */
void hop_chopmode_set(int m) { par5[0] = (uint8_t)(m <= 0 ? 0 : m == 1 ? 64 : 127); }
void hop_chop_on(int chops) { if (chops < 2) chops = 2; if (chops > 16) chops = 16; par1[2] = (uint8_t)((chops - 1) * 8); }
int hop_chops(void) { return (par1[2] >> 3) + 1; }
int hop_recording(void) { return rec_on; }
uint32_t hop_bank_seconds_x10(void) { return bank_len * 10u / 22050u; }

/* ---- SAMPLE: the app calls these */
void hop_record_start(void) { rec_pos = 0; rec_acc = 0; rec_tick = 0; rec_on = 1; }
void hop_record_stop(void)
{
    if (!rec_on) return;
    rec_on = 0;
    if (rec_pos > 2205u) { bank_len = rec_pos; own_dirty = 1; nslice_pt = 0; source = 1; }   /* at least 0.1 s, else keep the old bank */
}
void hop_record_feed(const float *mix, uint32_t n)                    /* audio irq: the master mix, halved in rate */
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
    if (rec_pos >= REC_MAX) hop_record_stop();
}

/* transient chop: the window's loudest onsets become the slice points (energy jumps, at least 60 ms apart) */
static void find_transients(uint32_t a, uint32_t b, uint32_t nc)
{
    enum { W = 256 };
    uint32_t i, k, n = (b - a) / W, cnt = 0;
    static float e[400]; static uint32_t cand[64]; static float cand_s[64];
    float prev = 0.0f, sens = 1.5f + (1.0f - p01(par5[1])) * 6.0f;
    if (n > 400u) n = 400u;
    for (i = 0; i < n; i++) {
        float acc = 0.0f;
        for (k = 0; k < W; k++) { float x = (float)bank[a + i * W + k] * (1.0f / 32768.0f); acc += x * x; }
        e[i] = acc;
    }
    for (i = 1; i < n && cnt < 64u; i++) {                           /* jumps above the running level */
        if (e[i] > prev * sens + 0.002f && (cnt == 0u || i - cand[cnt - 1u] >= 5u)) { cand[cnt] = i; cand_s[cnt] = e[i] - prev; cnt++; }
        prev = prev * 0.7f + e[i] * 0.3f;
    }
    /* keep the strongest nc-1 (plus the start), in order */
    while (cnt > nc - 1u) {
        uint32_t weakest = 0; for (i = 1; i < cnt; i++) if (cand_s[i] < cand_s[weakest]) weakest = i;
        for (i = weakest; i + 1u < cnt; i++) { cand[i] = cand[i + 1]; cand_s[i] = cand_s[i + 1]; }
        cnt--;
    }
    slice_pt[0] = a;
    for (i = 0; i < cnt; i++) slice_pt[i + 1] = a + cand[i] * W;
    nslice_pt = (uint8_t)(cnt + 1u);
    slice_pt[nslice_pt] = b;
}
int hop_chopmode(void) { return par5[0] / 43u > 2u ? 2 : (int)(par5[0] / 43u); }   /* 0 EQUAL, 1 TRANSIENT, 2 MANUAL */
uint32_t hop_slice_points(const uint32_t **pts) { *pts = slice_pt; return hop_chopmode() ? nslice_pt : 0u; }
static void equal_points(uint32_t a, uint32_t b, uint32_t nc)
{
    uint32_t i;
    for (i = 0; i <= nc && i <= NSLICES; i++) slice_pt[i] = a + (b - a) * i / nc;
    nslice_pt = (uint8_t)(nc > NSLICES ? NSLICES : nc);
}
/* MANUAL: move slice idx's start (1..n-1) by delta samples, kept between its neighbours */
void hop_slice_nudge(uint32_t idx, int32_t delta)
{
    int32_t v;
    if (hop_chopmode() != 2 || idx == 0u || idx >= nslice_pt) return;
    v = (int32_t)slice_pt[idx] + delta;
    if (v < (int32_t)slice_pt[idx - 1] + 220) v = (int32_t)slice_pt[idx - 1] + 220;
    if (v > (int32_t)slice_pt[idx + 1] - 220) v = (int32_t)slice_pt[idx + 1] - 220;
    slice_pt[idx] = (uint32_t)v;
}

static void start_voice(voice_t *w, int32_t note, float vel)
{
    uint32_t chops = par1[2] >> 3;
    float s0 = p01(par1[0]), s1 = p01(par1[1]);
    float semi = ((float)par1[3] - 64.0f) * (48.0f / 127.0f);
    float dec = p01(par3[1]), att = p01(par3[0]);
    uint32_t a, b, len;
    const uint8_t *sp = 0;
    if (s1 < s0 + 0.02f) s1 = s0 + 0.02f;
    if (s1 > 1.0f) s1 = 1.0f;
    a = (uint32_t)(s0 * (float)bank_len);
    b = (uint32_t)(s1 * (float)bank_len);
    len = b - a;
    if (chops >= 1u) {
        uint32_t nc = chops + 1u, idx = (uint32_t)note % nc;
        if (hop_chopmode()) {                                            /* TRANSIENT or MANUAL: the slice points */
            if (!nslice_pt || slice_pt[0] != a || slice_pt[nslice_pt] != b) { if (hop_chopmode() == 1) find_transients(a, b, nc); else equal_points(a, b, nc); }
            if (idx >= nslice_pt) idx = nslice_pt - 1u;
            b = slice_pt[idx + 1]; a = slice_pt[idx];
        } else { a += len * idx / nc; b = a + len / nc; }
        sp = slice_par[idx & 15u];
        last_slice = (int8_t)(idx & 15u);
        semi += ((float)sp[0] - 64.0f) * (24.0f / 127.0f);
        w->inc = fm_exp2f(semi * (1.0f / 12.0f)) * (BANK_SR / SR);
    } else {
        w->inc = fm_exp2f(((float)(note - 60) + semi) * (1.0f / 12.0f)) * (BANK_SR / SR);
    }
    w->start = a; w->end = b;
    w->loop = par2[1] >= 64u;
    if ((par2[0] >= 64u) ^ (sp && sp[1] >= 64u)) { w->inc = -w->inc; w->pos = (float)b - 2.0f; } else w->pos = (float)a;
    w->env = 1.0f;
    if (sp) { dec = dec * p01(sp[2]); vel *= p01(sp[3]) * 1.27f; }
    w->dcoef = dec > 0.99f ? 1.0f : decay_coef(0.02f + dec * dec * 2.0f);
    w->aenv = 0.0f;                                                 /* always a ramp in: 1 ms at least, no click at a slice edge */
    w->att = 1.0f / (0.001f + att * att * 1.5f) / SR;
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
            w->pos = w->inc > 0 ? (float)w->start : (float)w->end - 2.0f; w->env = 1.0f; w->aenv = 0.0f;
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
        if (v[pick].on) { tail[tail_i] = v[pick]; tail[tail_i].dcoef = 0.97f; tail[tail_i].roll = 0; tail[tail_i].loop = 0; tail_i = (uint8_t)((tail_i + 1u) % NTAIL); }
        start_voice(&v[pick], note, (float)pend_vel * (1.0f / 127.0f));
    }
    for (i = 0; i < NV; i++) run_voice(&v[i], out, n, &fc);
    for (i = 0; i < NTAIL; i++) run_voice(&tail[i], out, n, &fc);
}

static const char *f_text(uint8_t page, uint32_t p, char *buf)
{
    static const char *const ROLLS[4] = {"OFF", "1/16", "1/32", "1/64"};
    (void)buf;
    if (page == 2u && p == 2u) return par3[2] >= 64u ? "GATE" : "1-SHOT";
    if (page == 4u && p == 0u) return hop_chopmode() == 2 ? "MANUAL" : hop_chopmode() == 1 ? "TRANSNT" : "EQUAL";
    if (page == 3u && p == 1u) return par4[1] >= 64u ? "REV" : "FWD";
    if (page == 2u && p == 3u) return ROLLS[par3[3] >> 5];
    return 0;
}
static const char *const TITLES[5] = {"CUT", "PLAYBACK", "SHAPE", "SLICE", "CHOP MODE"};
const engine_t hopper = {"HOPPER", f_init, f_note, f_render, f_panic, 5, {NAMES1, NAMES2, NAMES3, NAMES4, NAMES5}, {par1, par2, par3, par4, par5}, f_text, TITLES, "hopper", 1};
