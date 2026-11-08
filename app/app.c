/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * EARTH CREATURE BEATS: four tracks rendered together, one in focus on the keys, the Beaver sequencing them.
 *   FX OTTER   SEL CAT   ENV FOX   LFO HOPPER   press: that track; again: next page; HOLD: mute / unmute
 *   SEQ BEAVER (the pattern)   EDIT MIXER   GLO tap tempo   HOME stop   SAVE keep everything
 *   K1..K4 always the four things on the screen   PRESET knob: page   ALGO knob: preset / kit / 808 / pattern
 *   SELECT knob: track (on every page)
 *   PLAY run   REC arm the focused track (recording writes the armed tracks; hold REC 1 s: clear its steps)
 *   ARP SAMPLE: Hopper records the output until ARP again
 *   Beaver page: white keys toggle the focused track's steps; HOLD a step + K1..K4 = its NOTE VEL LENGTH PROB;
 *                black keys: COPY PASTE CLEAR CHAIN . MUTE1 MUTE2 MUTE3 MUTE4 . SOLO
 *   Mixer page: K1..K4 levels (PRESET: pans, FX); white keys 1-4 mute, 9-12 solo
 *   MIDI in: ch 1 Otter, ch 2 Cat (36..51 = pads), ch 3 Fox, ch 4 Hopper; recorded onto armed tracks
 * Part of fm1-base's unity build (device/unity.c, host/plat_host.c), after the platform. */
#include "fastmath.h"
#include "engine/engine.h"

extern const engine_t otter, cat, fox, hopper;
void otter_preset(int32_t step); void otter_preset_set(int i); int otter_preset_index(void); const char *otter_preset_name(void);
int otter_arp_on(void); int otter_arp_rate(void); void otter_arp_tick(void);
void cat_kit(int32_t step); void cat_kit_set(int k); int cat_kit_index(void); const char *cat_kit_name(void);
const char *cat_pad_name(int pad); int cat_last_pad(void); uint8_t *cat_pad_blob(uint32_t *size); void cat_pads_default(void); void cat_sync(void);
void fox_preset(int32_t step); void fox_preset_set(int i); int fox_preset_index(void); const char *fox_preset_name(void); int fox_mode(void); void fox_mode_set_params(void);
int hop_chop_mode(void); int hop_chops(void); int hop_chopmode(void); int hop_recording(void); void hop_record_start(void); void hop_record_stop(void);
void hop_record_feed(const float *mix, uint32_t n); int hop_save_needed(void); int hop_save(void); uint32_t hop_bank_seconds_x10(void);
uint32_t hop_bank_len(void); int32_t hop_peak(uint32_t from, uint32_t to); uint32_t hop_window(uint32_t *a, uint32_t *b); float hop_playhead(void);
uint32_t hop_slice_points(const uint32_t **pts); uint8_t *hop_slice_blob(uint32_t *size); void hop_slices_default(void); int hop_last_slice(void);
void hop_sync(void); void hop_rechop(void); void hop_slice_nudge(uint32_t idx, int32_t delta);

#define NENG 4
static const engine_t *const ENG[NENG] = {&otter, &cat, &fox, &hopper};
enum { E_OTTER, E_CAT, E_FOX, E_HOP };
enum { PG_ENGINE, PG_SEQ, PG_MIX };

/* the palette: a dark neutral ground, light text, one bright colour per creature, red for recording */
#define C_BG      RGB(10, 12, 16)
#define C_PANEL   RGB(24, 28, 38)
#define C_RULE    RGB(44, 50, 64)
#define C_MUTED   RGB(78, 86, 104)
#define C_SUB     RGB(158, 168, 186)
#define C_TXT     RGB(240, 243, 248)
#define C_REC     RGB(255, 64, 96)
#define C_BRAND   RGB(255, 92, 170)
static const uint16_t ENG_RGB[5] = {RGB(72, 196, 255), RGB(255, 184, 28), RGB(255, 106, 70), RGB(132, 222, 92), RGB(214, 150, 96)};
static const uint16_t ENG_DARK[5] = {RGB(22, 62, 84), RGB(84, 60, 10), RGB(84, 34, 22), RGB(40, 74, 30), RGB(70, 48, 30)};
#define C_BEAVER ENG_RGB[4]

#include "icons.c"

#define SET_MAGIC 0x45434232u            /* "ECB2" */
typedef struct {
    uint32_t magic;
    int8_t octave;
    uint8_t focus;
    uint8_t page[NENG];
    uint8_t level[NENG], pan[NENG];
    uint8_t delay_mix, delay_time, delay_fb, crush, drive, mixpage;
    uint8_t otter_preset, fox_preset, cat_kit, pattern;
    uint8_t chain, rsv[3];
    uint8_t param[NENG][NPAGES][NPARAM];
    uint8_t pads[4 * 16 * 4];
    uint8_t slices[16 * 4];
    uint8_t pat[4][780];                  /* pattern_t x 4 */
} settings_t;
static settings_t set;
static uint8_t set_dirty;
static uint8_t body;

#define PAT ((pattern_t *)set.pat[set.pattern])
#include "seq.c"
static void chain_advance(void) { set.pattern = (uint8_t)((set.pattern + 1u) % 4u); }

/* ------------------------------------------------------------------ audio --- */
#define DLY_LEN 11025u
PLAT_POOL static int16_t dly[DLY_LEN];
static uint32_t dly_w;
static float dly_lp, dly_prev, dc_x[2], dc_y[2], lim_env = 1.0f;
static uint8_t dly_tick;
static float mix[NENG][APP_BLOCK];
static float mixout[APP_BLOCK];
static float gain_now;
static uint32_t arp_phase;
static float crush_hold[2]; static uint32_t crush_cnt;
static volatile float meter[NENG];       /* for the mixer's meters */

static void note_in(uint8_t e, int32_t note, uint32_t vel, int on)
{
    ENG[e]->note(note, vel, on);
    if (on) seq_record(PAT, e, note, vel);
}

static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xF0u, ch = (pkt >> 8) & 0x0Fu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        int on = st == 0x90u && d2;
        if (st != 0x90u && st != 0x80u) continue;
        if (ch < NENG) note_in((uint8_t)ch, ch == E_CAT ? (int32_t)d1 - 36 : (int32_t)d1, d2, on);
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, e;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;
    float gl[NENG], gr[NENG], dmix = p01(set.delay_mix) * 0.8f, dfb = p01(set.delay_fb) * 0.8f;
    float mdrive = 1.0f + p01(set.drive) * p01(set.drive) * 6.0f, dnorm = 1.0f / fm_tanhf(mdrive * 0.7f);
    uint32_t crush_div = 1u + (uint32_t)(p01(set.crush) * p01(set.crush) * 24.0f);
    float crush_q = set.crush ? fm_exp2f(2.0f + p01(set.crush) * 9.0f) : 0.0f;
    uint32_t dtime = 1100u + (uint32_t)(p01(set.delay_time) * (float)(DLY_LEN - 1200u));
    midi_in();
    seq_tick(PAT, n);
    if (otter_arp_on()) {
        static const uint8_t NUM[4] = {2, 1, 4, 1}, DEN[4] = {1, 1, 3, 2};
        uint32_t r = (uint32_t)otter_arp_rate(), len = seq_step_samples(PAT) * NUM[r] / DEN[r];
        arp_phase += n;
        if (arp_phase >= len) { arp_phase -= len; otter_arp_tick(); }
    } else arp_phase = 0;
    for (e = 0; e < NENG; e++) {
        float lvl = lane_audible(PAT, (uint8_t)e) ? p01(set.level[e]) * 1.27f : 0.0f, pan = p01(set.pan[e]), pk = 0.0f;
        gl[e] = lvl * fm_sinf((1.0f - pan) * 1.5707963f); gr[e] = lvl * fm_sinf(pan * 1.5707963f);
        for (i = 0; i < n; i++) mix[e][i] = 0.0f;
        ENG[e]->render(mix[e], n);
        for (i = 0; i < n; i += 4) { float a = fm_fabsf(mix[e][i]) * lvl; if (a > pk) pk = a; }
        meter[e] = pk > meter[e] ? pk : meter[e] * 0.85f;
    }
    for (i = 0; i < n; i++) {
        float l = 0.0f, r = 0.0f, mono, d, y[2], a, pk;
        for (e = 0; e < NENG; e++) { l += mix[e][i] * gl[e]; r += mix[e][i] * gr[e]; }
        mono = (l + r) * 0.5f;
        if (dly_tick ^= 1u) {
            uint32_t rd = (dly_w + DLY_LEN - dtime) % DLY_LEN;
            float w;
            d = (float)dly[rd] * (1.0f / 32768.0f);
            dly_lp += (d - dly_lp) * 0.5f;
            w = (dly_prev + mono) * 0.5f + dly_lp * dfb;
            if (w > 1.0f) w = 1.0f; else if (w < -1.0f) w = -1.0f;
            dly[dly_w] = (int16_t)(w * 32767.0f);
            dly_w = (dly_w + 1u) % DLY_LEN;
        }
        dly_prev = mono;
        l += dly_lp * dmix; r += dly_lp * dmix;
        if (set.drive) { l = fm_tanhf(l * 0.7f * mdrive) * dnorm; r = fm_tanhf(r * 0.7f * mdrive) * dnorm; }
        if (set.crush) {
            if (++crush_cnt >= crush_div) { crush_cnt = 0; crush_hold[0] = (float)(int32_t)(l * crush_q) / crush_q; crush_hold[1] = (float)(int32_t)(r * crush_q) / crush_q; }
            l = crush_hold[0]; r = crush_hold[1];
        }
        y[0] = l - dc_x[0] + 0.995f * dc_y[0]; dc_x[0] = l; dc_y[0] = y[0];
        y[1] = r - dc_x[1] + 0.995f * dc_y[1]; dc_x[1] = r; dc_y[1] = y[1];
        a = fm_fabsf(y[0]); pk = fm_fabsf(y[1]); if (pk > a) a = pk;
        if (a > lim_env) lim_env += (a - lim_env) * 0.5f; else lim_env += (a - lim_env) * 0.0005f;
        if (lim_env > 0.9f) { float g = 0.9f / lim_env; y[0] *= g; y[1] *= g; }
        if (!(y[0] == y[0]) || !(y[1] == y[1])) {
            for (e = 0; e < NENG; e++) ENG[e]->init();
            dc_x[0] = dc_x[1] = dc_y[0] = dc_y[1] = dly_lp = dly_prev = 0.0f; lim_env = 1.0f; y[0] = y[1] = 0.0f;
        }
        mixout[i] = (y[0] + y[1]) * 0.5f;
        gain_now += (gain - gain_now) * 0.004f;
        lr[2 * i] = (int32_t)(fm_tanhf(y[0] * 0.9f) * 1.05f * gain_now);
        lr[2 * i + 1] = (int32_t)(fm_tanhf(y[1] * 0.9f) * 1.05f * gain_now);
    }
    hop_record_feed(mixout, n);
}

void app_panic(void) { uint32_t e; seq_stop(); for (e = 0; e < NENG; e++) ENG[e]->panic(); }

/* --------------------------------------------------------------- settings --- */
static uint8_t *page_param(uint8_t e, uint8_t pg) { return pg < ENG[e]->npages ? ENG[e]->params[pg] : ENG[e]->params[0]; }
static const char *const *page_names(uint8_t e, uint8_t pg) { return pg < ENG[e]->npages ? ENG[e]->names[pg] : ENG[e]->names[0]; }
static void params_to_set(void)
{
    uint32_t e, g, p, sz;
    uint8_t *b;
    set.otter_preset = (uint8_t)otter_preset_index(); set.fox_preset = (uint8_t)fox_preset_index(); set.cat_kit = (uint8_t)cat_kit_index();
    set.chain = seq.chain;
    for (e = 0; e < NENG; e++) for (g = 0; g < ENG[e]->npages; g++) for (p = 0; p < NPARAM; p++) set.param[e][g][p] = ENG[e]->params[g][p];
    b = cat_pad_blob(&sz); for (p = 0; p < sz && p < sizeof set.pads; p++) set.pads[p] = b[p];
    b = hop_slice_blob(&sz); for (p = 0; p < sz && p < sizeof set.slices; p++) set.slices[p] = b[p];
}
static void params_from_set(void)
{
    uint32_t e, g, p, sz;
    uint8_t *b;
    for (e = 0; e < NENG; e++) for (g = 0; g < ENG[e]->npages; g++) for (p = 0; p < NPARAM; p++) ENG[e]->params[g][p] = set.param[e][g][p];
    b = cat_pad_blob(&sz); for (p = 0; p < sz && p < sizeof set.pads; p++) b[p] = set.pads[p];
    b = hop_slice_blob(&sz); for (p = 0; p < sz && p < sizeof set.slices; p++) b[p] = set.slices[p];
    seq.chain = set.chain;
}

void app_init(void)
{
    uint32_t e;
    _Static_assert(sizeof(pattern_t) <= 780u, "the pattern fits");
    _Static_assert(sizeof(settings_t) <= 3900u, "the settings fit a store object");
    for (e = 0; e < NENG; e++) ENG[e]->init();
    seq.rng = 0x1234567u;
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
        && set.octave >= -2 && set.octave <= 2 && set.focus < NENG && set.pattern < 4u
        && PAT->bpm >= 40u && PAT->steps >= 1u && PAT->steps <= NSTEP) {
        for (e = 0; e < NENG; e++) if (set.page[e] >= ENG[e]->npages) set.page[e] = 0;
        otter_preset_set(set.otter_preset); fox_preset_set(set.fox_preset); cat_kit_set(set.cat_kit);
        params_from_set();
        fox_mode_set_params();
    } else {
        uint32_t k;
        set.magic = SET_MAGIC; set.octave = 0; set.focus = E_CAT;
        for (e = 0; e < NENG; e++) { set.page[e] = 0; set.level[e] = 100; set.pan[e] = 64; }
        set.level[E_CAT] = 110;
        set.delay_mix = 0; set.delay_time = 60; set.delay_fb = 50; set.crush = 0; set.drive = 0; set.mixpage = 0;
        set.otter_preset = 0; set.fox_preset = 0; set.cat_kit = 0; set.pattern = 0; set.chain = 0;
        cat_pads_default(); hop_slices_default();
        for (k = 0; k < 4u; k++) pattern_defaults((pattern_t *)set.pat[k]);
        params_to_set();
    }
    cat.init(); hopper.init();
    seq_stop();
}

/* ----------------------------------------------------------------- backup --- */
int app_backup_playing(void) { return seq.playing; }
void app_backup_flush(void) { params_to_set(); plat_store_save(OBJ_SETTINGS, &set, sizeof set); set_dirty = 0; }
void app_backup_hold(int on) { (void)on; }
void app_backup_restored(void) { app_init(); }

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    int32_t key_note[NKEYS];
    uint8_t key_eng[NKEYS];
    uint8_t redraw;
    int32_t last_note;
    uint32_t btn_t[NB];                   /* when each button went down (0 = up / consumed) */
    uint8_t btn_long[NB];                 /* the hold already acted */
    int32_t shown_step;
    uint32_t tap_t[4]; uint8_t tap_n;
    int32_t held_step;                    /* the Beaver: the step key held (-1 none) */
    uint8_t step_edited;                  /* a knob moved while it was held: no toggle on release */
    int32_t held_slice;                   /* the Hopper: the slice key held (-1 none) */
} ui;

static void text_cat(char *d, const char *s) { while (*d) d++; while (*s) *d++ = *s++; *d = 0; }
static void int_text(int32_t v, char *b)
{
    char t[12]; int n = 0; uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do t[n++] = (char)('0' + u % 10u); while ((u /= 10u) && n < 11);
    if (v < 0) *b++ = '-';
    while (n) *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++) ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++) ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1100u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0; text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) { text_cat(t, " "); text_cat(t, ui.msg[1]); }
        ann_say(t, 0, ANN_NOW);
    }
}

static const char *body_name(void) { return body == PG_SEQ ? "BEAVER" : body == PG_MIX ? (set.mixpage == 0 ? "MIXER" : set.mixpage == 1 ? "PANS" : "FX") : ENG[set.focus]->name; }

static void speak(void)
{
    if (ann_on() && ann_where()) {
        char t[64] = APP_NAME ". ";
        text_cat(t, body_name());
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

static int32_t white_index(uint32_t k)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS;
    uint32_t w;
    for (w = 0; w < 16u; w++) if (WK[w] == k) return (int32_t)w;
    return -1;
}
static int32_t black_index(uint32_t k)
{
    static const uint8_t BK[11] = PLAT_BLACK_KEYS;
    uint32_t b;
    for (b = 0; b < 11u; b++) if (BK[b] == k) return (int32_t)b;
    return -1;
}
static int32_t key_to_note(uint32_t k, uint8_t e)
{
    if (e == E_CAT) return white_index(k);
    if (e == E_HOP && hop_chop_mode()) return white_index(k);
    return (e == E_FOX ? 29 : 53) + (int32_t)k + 12 * set.octave;
}

static void page_set(uint8_t pg)
{
    char t[8] = "PAGE 1";
    pg %= ENG[set.focus]->npages;
    set.page[set.focus] = pg;
    set_dirty = 1; ui.redraw = 1;
    t[5] = (char)('1' + pg);
    app_say(ENG[set.focus]->name, t);
}
static void focus(uint8_t e)
{
    if (body == PG_ENGINE && e == set.focus) { page_set((uint8_t)(set.page[e] + 1u)); return; }
    body = PG_ENGINE; set.focus = e; set_dirty = 1; ui.redraw = 1;
    app_say(ENG[e]->name, "");
}
static void mute_toggle(uint8_t e)
{
    PAT->mute ^= (uint8_t)(1u << e);
    if (PAT->mute >> e & 1u) ENG[e]->panic();
    set_dirty = 1; ui.redraw = 1;
    app_say(ENG[e]->name, PAT->mute >> e & 1u ? "MUTED" : "ON");
}
static void solo_toggle(uint8_t e)
{
    seq.solo = (uint8_t)(seq.solo == (1u << e) ? 0u : 1u << e);
    ui.redraw = 1;
    app_say(ENG[e]->name, seq.solo ? "SOLO" : "SOLO OFF");
}

static uint8_t *knob_target(uint32_t p, const char **name)
{
    static const char *const SEQN[4] = {"TEMPO", "SWING", "STEPS", "GATE"};
    static const char *const STEPN[4] = {"NOTE", "VEL", "LENGTH", "PROB"};
    static const char *const PADN[4] = {"PAD", "VEL", "LENGTH", "PROB"};
    static const char *const MIXN[4] = {"OTTER", "CAT", "FOX", "HOPPER"};
    static const char *const MIX2[4] = {"DELAY", "DTIME", "DFEEDB", "CRUSH"};
    static const char *const PANN[4] = {"PAN OTT", "PAN CAT", "PAN FOX", "PAN HOP"};
    if (body == PG_SEQ) {
        if (ui.held_step >= 0) { *name = (set.focus == E_CAT ? PADN : STEPN)[p]; return seq_step_param(PAT, set.focus, (uint32_t)ui.held_step, p); }
        *name = SEQN[p]; return p == 0 ? &PAT->bpm : p == 1 ? &PAT->swing : p == 2 ? &PAT->steps : &PAT->gate;
    }
    if (body == PG_MIX) {
        if (set.mixpage == 1u) { *name = PANN[p]; return &set.pan[p]; }
        if (set.mixpage == 2u) { *name = MIX2[p]; return p == 0 ? &set.delay_mix : p == 1 ? &set.delay_time : p == 2 ? &set.delay_fb : &set.crush; }
        *name = MIXN[p]; return &set.level[p];
    }
    *name = page_names(set.focus, set.page[set.focus])[p];
    return &page_param(set.focus, set.page[set.focus])[p];
}

static void knob(uint32_t p, int32_t e)
{
    const char *name;
    uint8_t *v = knob_target(p, &name);
    char t[12];
    if (body == PG_ENGINE && set.focus == E_HOP && ui.held_slice > 0 && p == 0) {   /* MANUAL chop: move the held slice's start */
        hop_slice_nudge((uint32_t)ui.held_slice, e * 440);
        app_say("SLICE START", e > 0 ? ">" : "<");
        set_dirty = 1; ui.redraw = 1; return;
    }
    if (body == PG_SEQ && ui.held_step >= 0) {
        ui.step_edited = 1;
        if (p == 0) {                                                       /* NOTE / PAD: one per detent */
            if (set.focus == E_CAT) { uint16_t *m = &PAT->lane[E_CAT].mask[ui.held_step]; int32_t pd = cat_last_pad(); pd = clamp(pd + e, 0, 15); *m = (uint16_t)(1u << pd); int_text(pd + 1, t); }
            else { int32_t lo = set.focus == E_HOP ? 0 : 1, hi = set.focus == E_HOP ? 15 : 119; int32_t nv = lane_rest(set.focus, *v) ? (set.focus == E_HOP ? 0 : 48) : (int32_t)*v; *v = (uint8_t)clamp(nv + e, lo, hi); int_text(*v, t); }
        } else if (p == 2) { *v = (uint8_t)clamp((int32_t)*v + e, 0, 16); if (*v) { int_text((int32_t)*v, t); text_cat(t, "/4"); } else { t[0] = 0; text_cat(t, "GATE"); } }
        else { *v = (uint8_t)clamp((int32_t)*v + e * 4, 0, 127); if (p == 3 && *v >= 127u) { t[0] = 0; text_cat(t, "ALWAYS"); } else int_text(*v, t); }
        set_dirty = 1; app_say(name, t); return;
    }
    if (body == PG_SEQ && p == 0) *v = (uint8_t)clamp((int32_t)*v + e, 40, 240);
    else if (body == PG_SEQ && p == 2) *v = (uint8_t)clamp((int32_t)*v + e, 1, NSTEP);
    else *v = (uint8_t)clamp((int32_t)*v + e * 3, 0, 127);
    if (body == PG_ENGINE && set.focus == E_HOP && set.page[E_HOP] == 4u) hop_rechop();
    set_dirty = 1;
    int_text(*v, t);
    app_say(name, t);
}

static void tap_tempo(void)
{
    uint32_t now = plat_ms(), i, sum = 0;
    if (ui.tap_n && now - ui.tap_t[ui.tap_n - 1u] > 2000u) ui.tap_n = 0;
    if (ui.tap_n < 4u) ui.tap_t[ui.tap_n++] = now;
    else { for (i = 0; i < 3u; i++) ui.tap_t[i] = ui.tap_t[i + 1]; ui.tap_t[3] = now; }
    if (ui.tap_n >= 2u) {
        char t[8];
        for (i = 1; i < ui.tap_n; i++) sum += ui.tap_t[i] - ui.tap_t[i - 1];
        PAT->bpm = (uint8_t)clamp((int32_t)(60000u * (ui.tap_n - 1u) / sum), 40, 240);
        set_dirty = 1; int_text(PAT->bpm, t); app_say("TAP", t);
    } else app_say("TAP", "...");
}

static void beaver_black(int32_t b)
{
    char t[4] = {'A', 0};
    switch (b) {
    case 0: pattern_copy(PAT); app_say("COPIED", ""); break;
    case 1: if (pattern_paste(PAT)) { set_dirty = 1; app_say("PASTED", ""); } else app_say("NOTHING", "COPIED"); break;
    case 2: seq_clear_lane(PAT, set.focus); set_dirty = 1; app_say(ENG[set.focus]->name, "CLEARED"); break;
    case 3: seq.chain = (uint8_t)!seq.chain; set_dirty = 1; app_say("CHAIN", seq.chain ? "A>B>C>D" : "OFF"); break;
    case 5: case 6: case 7: case 8: mute_toggle((uint8_t)(b - 5)); break;
    case 10: solo_toggle(set.focus); break;
    default: t[0] = (char)('A' + set.pattern); app_say("PATTERN", t); break;
    }
    ui.redraw = 1;
}

static void engine_button(uint32_t k, uint8_t e, uint32_t btn, uint32_t now)
{
    uint32_t bit = 1u << k;
    if ((btn & bit) && !(ui.btn & bit)) { ui.btn_t[k] = now; ui.btn_long[k] = 0; }
    else if ((btn & bit) && ui.btn_t[k] && !ui.btn_long[k] && now - ui.btn_t[k] >= 450u) { ui.btn_long[k] = 1; mute_toggle(e); }
    else if (!(btn & bit) && (ui.btn & bit)) { if (!ui.btn_long[k]) focus(e); ui.btn_t[k] = 0; }
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, now = plat_ms();
    int32_t e;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            int32_t note = key_to_note(k, set.focus), w = white_index(k), b = black_index(k);
            uint32_t v = touch_vel(k);
            ui.key_note[k] = -1;
            if (body == PG_SEQ) {                                      /* the Beaver's keys */
                if (w >= 0) { ui.held_step = w; ui.step_edited = 0; }
                else if (b >= 0) beaver_black(b);
                ui.redraw = 1;
                continue;
            }
            if (body == PG_MIX) {                                      /* the mixer's keys: mute 1-4, solo 9-12 */
                if (w >= 0 && w < 4) mute_toggle((uint8_t)w);
                else if (w >= 8 && w < 12) solo_toggle((uint8_t)(w - 8));
                continue;
            }
            if (set.focus == E_HOP && hop_chop_mode() && w >= 0) ui.held_slice = w;
            ui.key_note[k] = note;
            ui.key_eng[k] = set.focus;
            if (note >= 0) {
                note_in(set.focus, note, v, 1);
                ui.last_note = note;
                if (set.focus != E_CAT) plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB);
            }
        } else if (!(keys & bit) && (ui.keys & bit)) {
            int32_t note = ui.key_note[k], w = white_index(k);
            if (body == PG_SEQ && w >= 0 && w == ui.held_step) {
                if (!ui.step_edited) { seq_toggle(PAT, set.focus, (uint32_t)w, ui.last_note, cat_last_pad(), hop_last_slice()); set_dirty = 1; }
                ui.held_step = -1; ui.redraw = 1;
            }
            if (w == ui.held_slice) ui.held_slice = -1;
            if (note >= 0) {
                ENG[ui.key_eng[k]]->note(note, 0, 0);
                if (ui.key_eng[k] != E_CAT) plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB);
            }
        }
    }
    /* the engine buttons: press / hold */
    engine_button(B_FX, E_OTTER, btn, now); engine_button(B_SEL, E_CAT, btn, now);
    engine_button(B_ENV, E_FOX, btn, now); engine_button(B_LFO, E_HOP, btn, now);
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (k == B_FX || k == B_SEL || k == B_ENV || k == B_LFO) continue;
        if (!(btn & bit) || (ui.btn & bit)) continue;
        if (k == B_SEQ) { body = body == PG_SEQ ? PG_ENGINE : PG_SEQ; ui.held_step = -1; ui.redraw = 1; app_say(body_name(), ""); }
        else if (k == B_EDIT) { body = body == PG_MIX ? PG_ENGINE : PG_MIX; ui.redraw = 1; app_say(body_name(), ""); }
        else if (k == B_GLO) tap_tempo();
        else if (k == B_PLAY) { if (seq.playing) seq_stop(); else seq_start(); ui.redraw = 1; app_say(seq.playing ? "PLAY" : "STOP", ""); }
        else if (k == B_REC) ui.btn_t[B_REC] = now;
        else if (k == B_ARP) {
            if (hop_recording()) { hop_record_stop(); app_say("SAMPLED", ""); set_dirty = 1; }
            else { hop_record_start(); body = PG_ENGINE; set.focus = E_HOP; app_say("SAMPLING", "ARP STOPS"); }
            ui.redraw = 1;
        }
        else if (k == B_HOME) { seq_stop(); seq.rec = 0; seq.armed = 0; body = PG_ENGINE; ui.redraw = 1; app_say("HOME", ""); }
        else if (k == B_OCTDN || k == B_OCTUP) {
            int32_t o = clamp(set.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
            char t[4] = {o < 0 ? '-' : '+', (char)('0' + (o < 0 ? -o : o)), 0};
            if (o != set.octave) { otter.panic(); fox.panic(); set.octave = (int8_t)o; set_dirty = 1; app_say("OCTAVE", t); }
        } else if (k == B_SAVE) {
            int rc;
            params_to_set();
            rc = plat_store_save(OBJ_SETTINGS, &set, sizeof set);
            if (rc == 0 && hop_save_needed()) { app_panic(); rc = hop_save(); }
            if (rc == 0) { set_dirty = 0; app_say("SAVED", ""); } else app_say("SAVE", "FAILED");
        }
    }
    if (ui.btn_t[B_REC]) {                                             /* REC: a press arms the focused track; a hold clears it */
        if (btn >> B_REC & 1u) {
            if (now - ui.btn_t[B_REC] >= 1000u) { ui.btn_t[B_REC] = 0; seq_clear_lane(PAT, set.focus); set_dirty = 1; app_say(ENG[set.focus]->name, "CLEARED"); }
        } else {
            ui.btn_t[B_REC] = 0;
            seq.armed ^= (uint8_t)(1u << set.focus);
            seq.rec = seq.armed != 0u;
            ui.redraw = 1;
            app_say(ENG[set.focus]->name, seq.armed >> set.focus & 1u ? "ARMED" : "DISARMED");
        }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0) continue;
        if (k >= EN_K1 && k < EN_K1 + NPARAM) knob(k - EN_K1, e);
        else if (k == EN_PRESET) {
            if (body == PG_MIX) { set.mixpage = (uint8_t)((set.mixpage + 3u + (e > 0 ? 1u : 2u)) % 3u); ui.redraw = 1; app_say(body_name(), ""); }
            else if (body == PG_ENGINE) page_set((uint8_t)(set.page[set.focus] + (e > 0 ? 1u : ENG[set.focus]->npages - 1u)));
        } else if (k == EN_ALGO) {
            if (body == PG_SEQ) {
                char t[2] = {'A', 0};
                set.pattern = (uint8_t)(((int32_t)set.pattern + e % 4 + 4) % 4);
                t[0] = (char)('A' + set.pattern);
                if (seq.playing) { seq_stop(); seq_start(); }
                app_say("PATTERN", t);
            } else {
                body = PG_ENGINE;
                if (set.focus == E_OTTER) { otter.panic(); otter_preset(e); app_say("PRESET", otter_preset_name()); }
                else if (set.focus == E_CAT) { cat_kit(e); app_say("KIT", cat_kit_name()); }
                else if (set.focus == E_FOX) { fox.panic(); fox_preset(e); app_say(fox_mode() ? "BASS" : "808", fox_preset_name()); }
                else app_say("HOPPER", hop_chop_mode() ? "CHOP" : "KEYS");
            }
            set_dirty = 1; ui.redraw = 1;
        } else if (k == EN_SELECT) {
            if (body != PG_SEQ && body != PG_MIX) body = PG_ENGINE;
            set.focus = (uint8_t)((set.focus + NENG + (e > 0 ? 1u : NENG - 1u)) % NENG);
            ui.held_step = -1;
            set_dirty = 1; app_say(body == PG_ENGINE ? ENG[set.focus]->name : "TRACK", body == PG_ENGINE ? "" : ENG[set.focus]->name);
        }
    }
    if (keys != ui.keys || btn != ui.btn) ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    cat_sync(); hop_sync();
    plat_glow(1);
    plat_leds((body == PG_ENGINE ? 1u << (B_FX + set.focus) : 0u) | (set_dirty ? 1u << B_SAVE : 0u) | (seq.playing ? 1u << B_PLAY : 0u)
              | (seq.rec ? 1u << B_REC : 0u) | (body == PG_SEQ ? 1u << B_SEQ : 0u) | (body == PG_MIX ? 1u << B_EDIT : 0u)
              | (hop_recording() ? 1u << B_ARP : 0u), keys);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------- draw --- */
static void ui_panel(int32_t x, int32_t y, int32_t w, int32_t h) { cv_rect(x, y, w, h, C_RULE); cv_rect(x + 1, y + 1, w - 2, h - 2, C_PANEL); }

static void bar_h(int32_t x, int32_t y, int32_t bh, const char *name, int32_t v, int32_t lo, int32_t hi, uint16_t col, const char *txt)
{
    int32_t h = (v - lo) * (bh - 4) / (hi - lo);
    char t[8];
    ui_panel(x, y, 52, bh);
    cv_rect(x + 2, y + bh - 2 - h, 48, h, col);
    cv_text(x + (52 - text_w(&FONT_XS, name)) / 2, y + bh + 3, &FONT_XS, name, C_SUB);
    if (!txt) { int_text(v, t); txt = t; }
    cv_text(x + (52 - text_w(&FONT_XS, txt)) / 2, y + bh + 15, &FONT_XS, txt, C_TXT);
}
#define bar(x, y, name, v, lo, hi, col, txt) bar_h(x, y, 56, name, v, lo, hi, col, txt)
#define BX(i) (8 + (int32_t)(i) * 57)

static void pan_bar(int32_t x, int32_t y, const char *name, int32_t v, uint16_t col)
{
    char t[8];
    int32_t c = x + 26, p = (v - 64) * 23 / 64;
    ui_panel(x, y, 52, 56);
    cv_rect(c, y + 4, 1, 48, C_MUTED);
    if (p >= 0) cv_rect(c, y + 20, p + 1, 16, col); else cv_rect(c + p, y + 20, -p + 1, 16, col);
    cv_text(x + (52 - text_w(&FONT_XS, name)) / 2, y + 59, &FONT_XS, name, C_SUB);
    if (v == 64) { t[0] = 'C'; t[1] = 0; } else { t[0] = v < 64 ? 'L' : 'R'; int_text(v < 64 ? 64 - v : v - 64, t + 1); }
    cv_text(x + (52 - text_w(&FONT_XS, t)) / 2, y + 71, &FONT_XS, t, C_TXT);
}

static void draw(void)
{
    char t[16];
    const engine_t *cur = ENG[set.focus];
    uint32_t i, now = plat_ms();
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
    uint16_t accent = body == PG_SEQ ? C_BEAVER : body == PG_MIX ? C_TXT : ENG_RGB[set.focus];
    int32_t st = seq.step;

    /* header: ECB, the five creatures as tabs with their track state, pattern, tempo, transport */
    cv_begin(240, 36, C_BG);
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 9, &FONT_B, ui.msg[0], C_TXT);
        cv_text(x + 8, 9, &FONT_B, ui.msg[1], accent);
    } else {
        const char *tr = seq.rec ? "REC" : seq.playing ? "PLAY" : "STOP";
        cv_text(5, 9, &FONT_B, "ECB", C_BRAND);
        for (i = 0; i < 5u; i++) {
            int32_t x = 38 + (int32_t)i * 27;
            int on = i < NENG ? body == PG_ENGINE && i == set.focus : body == PG_SEQ;
            int muted = i < NENG && !lane_audible(PAT, (uint8_t)i), armed = i < NENG && (seq.armed >> i & 1u);
            cv_rect(x, 3, 25, 30, on ? ENG_RGB[i] : C_PANEL);
            icon(x + 4, 6, ICONS[i], on ? C_BG : muted ? C_MUTED : ENG_RGB[i], 1);
            if (i < NENG) cv_rect(x + 4, 25, 17, 3, armed ? C_REC : muted ? C_MUTED : on ? C_BG : ENG_DARK[i]);   /* the track's state line */
        }
        int_text(PAT->bpm, t);
        cv_text(235 - text_w(&FONT_XS, t), 3, &FONT_XS, t, seq.playing ? C_TXT : C_SUB);
        cv_text(235 - text_w(&FONT_XS, tr), 18, &FONT_XS, tr, seq.rec ? C_REC : seq.playing ? ENG_RGB[3] : C_MUTED);
        if (seq.chain) { t[0] = 0; text_cat(t, "CH "); t[3] = (char)('A' + set.pattern); t[4] = 0; }
        else { t[0] = (char)('A' + set.pattern); t[1] = 0; }
        cv_text(235 - text_w(&FONT_XS, "120") - 8 - text_w(&FONT_XS, t), 3, &FONT_XS, t, C_BEAVER);
    }
    cv_blit(0, 0);

    /* body */
    cv_begin(240, 128, C_BG);
    if (body == PG_ENGINE) {
        int hop_wave = set.focus == E_HOP && set.page[E_HOP] < 3u;
        icon(6, 0, ICONS[set.focus], accent, 2);
        cv_text(42, 6, &FONT_B, cur->name, accent);
        for (i = 0; i < cur->npages; i++) cv_rect(42 + (int32_t)i * 9, 24, 6, 3, set.page[set.focus] == i ? accent : C_RULE);
        {
            const char *a = "", *b = "";
            if (set.focus == E_OTTER) { a = otter_preset_name(); t[0] = set.octave < 0 ? '-' : '+'; t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave)); t[2] = 0; b = t; }
            else if (set.focus == E_CAT) { a = cat_kit_name(); b = cat_pad_name(cat_last_pad()); }
            else if (set.focus == E_FOX) { a = fox_mode() ? "BASS" : "808"; b = fox_preset_name(); }
            else {
                static char s[8];
                a = hop_recording() ? "SAMPLING" : hop_chop_mode() ? (hop_chopmode() == 2 ? "MANUAL" : hop_chopmode() == 1 ? "TRANSIENT" : "CHOP") : "KEYS";
                int_text((int32_t)hop_bank_seconds_x10() / 10, s); text_cat(s, "."); int_text((int32_t)hop_bank_seconds_x10() % 10, s + 2); text_cat(s, "S");
                b = s;
            }
            cv_text(232 - text_w(&FONT_XS, b), 10, &FONT_XS, b, set.focus == E_HOP && hop_recording() ? C_REC : C_TXT);
            cv_text(232 - text_w(&FONT_XS, b) - 8 - text_w(&FONT_XS, a), 10, &FONT_XS, a, C_SUB);
        }
        if (hop_wave) {
            uint32_t a, b, nc = hop_window(&a, &b), len = hop_bank_len(), c;
            const uint32_t *pts; uint32_t npts = hop_slice_points(&pts);
            float ph = hop_playhead();
            ui_panel(8, 30, 224, 28);
            for (c = 0; c < 110u; c++) {
                uint32_t f0 = len * c / 110u, f1 = len * (c + 1u) / 110u;
                int32_t h = hop_peak(f0, f1) * 12 / 32768, in = f0 >= a && f1 <= b;
                if (h < 1) h = 1;
                cv_rect(10 + (int32_t)c * 2, 44 - h, 2, h * 2, in ? accent : C_MUTED);
            }
            if (npts && nc > 1u) {
                for (c = 1; c < npts; c++) cv_rect(10 + (int32_t)(pts[c] * 220u / (len ? len : 1u)), 31, 1, 26, (int32_t)c == ui.held_slice ? C_TXT : C_SUB);
                if (ui.held_slice >= 0 && (uint32_t)ui.held_slice < npts) {         /* the held slice, lit */
                    int32_t x0 = 10 + (int32_t)(pts[ui.held_slice] * 220u / (len ? len : 1u)), x1 = 10 + (int32_t)(pts[ui.held_slice + 1] * 220u / (len ? len : 1u));
                    cv_rect(x0, 31, 1, 26, C_TXT); cv_rect(x0, 55, x1 - x0, 2, C_TXT);
                }
            } else for (c = 1; c < nc; c++) cv_rect(10 + (int32_t)((a + (b - a) * c / nc) * 220u / (len ? len : 1u)), 31, 1, 26, C_SUB);
            if (ph >= 0.0f) cv_rect(10 + (int32_t)(ph * 219.0f), 31, 1, 26, C_TXT);
        }
        for (i = 0; i < NPARAM; i++) {
            const char *nm, *txt = 0;
            uint8_t *pp = knob_target(i, &nm);
            int sw = set.focus == E_HOP && set.page[E_HOP] == 1u && i < 2u;
            int32_t v = *pp;
            if (sw) { txt = v >= 64 ? "ON" : "OFF"; v = v >= 64 ? 127 : 0; }
            else if (set.focus == E_HOP && set.page[E_HOP] == 0u && i == 2u) { int_text(hop_chops(), t); txt = t; }
            else if (cur->param_text) txt = cur->param_text(set.page[set.focus], i, t);
            if (hop_wave) bar_h(BX(i), 62, 34, nm, v, 0, 127, accent, txt);
            else bar(BX(i), 34, nm, v, 0, 127, accent, txt);
        }
    } else if (body == PG_SEQ) {
        uint32_t e, s, shown = PAT->steps > 16u ? 32u : 16u, cw = shown > 16u ? 6u : 13u;
        icon(6, 0, ICONS[4], accent, 2);
        cv_text(42, 6, &FONT_B, "BEAVER", accent);
        for (i = 0; i < 4u; i++) {
            char l[2] = {(char)('A' + i), 0};
            cv_rect(104 + (int32_t)i * 18, 5, 16, 16, i == set.pattern ? accent : C_PANEL);
            cv_text(104 + (int32_t)i * 18 + 5, 7, &FONT_XS, l, i == set.pattern ? C_BG : C_SUB);
        }
        cv_text(232 - text_w(&FONT_XS, ENG[set.focus]->name), 8, &FONT_XS, ENG[set.focus]->name, ENG_RGB[set.focus]);
        for (e = 0; e < NENG; e++) {
            int32_t y = 30 + (int32_t)e * 11;
            int muted = !lane_audible(PAT, (uint8_t)e), armed = seq.armed >> e & 1u;
            cv_rect(8, y + 1, 12, 8, e == set.focus ? ENG_RGB[e] : muted ? C_MUTED : ENG_DARK[e]);
            if (armed) cv_rect(8, y + 1, 3, 8, C_REC);
            for (s = 0; s < shown; s++) {
                int has = seq_lane_has(PAT, (uint8_t)e, s), in = s < PAT->steps, on = seq.playing && (int32_t)s == st;
                uint16_t c = on && has ? C_TXT : !in ? C_BG : has ? (muted ? C_MUTED : ENG_RGB[e]) : (s & 3u) ? C_PANEL : C_RULE;
                if ((int32_t)s == ui.held_step && e == set.focus) c = C_TXT;
                cv_rect(26 + (int32_t)(s * cw), y + 1, (int32_t)cw - 2, 8, c);
            }
        }
        for (i = 0; i < NPARAM; i++) {
            const char *nm, *txt = 0; uint8_t *pp = knob_target(i, &nm); int32_t v = *pp, lo = 0, hi = 127;
            if (ui.held_step >= 0) {
                if (i == 0) { if (set.focus == E_CAT) { int_text(cat_last_pad() + 1, t); txt = t; v = cat_last_pad() * 8; } else if (lane_rest(set.focus, (uint8_t)v)) { txt = "REST"; v = 0; } }
                else if (i == 2) { if (!v) txt = "GATE"; hi = 16; }
                else if (i == 3 && v >= 127) txt = "ALWAYS";
            } else if (i == 0) { lo = 40; hi = 240; } else if (i == 2) { hi = NSTEP; }
            bar_h(BX(i), 78, 20, nm, v, lo, hi, ui.held_step >= 0 ? ENG_RGB[set.focus] : accent, txt);
        }
    } else {
        cv_text(8, 4, &FONT_B, body_name(), accent);
        cv_text(232 - text_w(&FONT_XS, "KEYS 1-4 MUTE  9-12 SOLO"), 8, &FONT_XS, "KEYS 1-4 MUTE  9-12 SOLO", C_SUB);
        if (set.mixpage == 1u) { for (i = 0; i < NENG; i++) pan_bar(BX(i), 34, ENG[i]->name, set.pan[i], ENG_RGB[i]); }
        else if (set.mixpage == 2u) {
            bar(BX(0), 34, "DELAY", set.delay_mix, 0, 127, C_TXT, set.delay_mix ? 0 : "DRY");
            bar(BX(1), 34, "DTIME", set.delay_time, 0, 127, C_TXT, 0);
            bar(BX(2), 34, "DFEEDB", set.delay_fb, 0, 127, C_TXT, 0);
            bar(BX(3), 34, "CRUSH", set.crush, 0, 127, C_TXT, set.crush ? 0 : "OFF");
        } else for (i = 0; i < NENG; i++) {
            int muted = !lane_audible(PAT, (uint8_t)i);
            int32_t m = (int32_t)(meter[i] * 52.0f); if (m > 52) m = 52;
            bar(BX(i), 34, ENG[i]->name, set.level[i], 0, 127, muted ? C_MUTED : ENG_RGB[i], muted ? (seq.solo && !(seq.solo >> i & 1u) ? "-" : "MUTE") : seq.solo >> i & 1u ? "SOLO" : 0);
            cv_rect(BX(i) + 48, 34 + 54 - m, 2, m, C_TXT);                         /* the meter */
        }
    }
    cv_blit(0, 36);

    /* bottom: the step strip, the keys, the legend */
    cv_begin(240, 76, C_BG);
    {
        uint32_t shown = PAT->steps > 16u ? 32u : 16u, cw = shown > 16u ? 7u : 14u;
        for (i = 0; i < shown; i++) {
            int in = i < PAT->steps, on = seq.playing && (int32_t)i == st, has = seq_lane_has(PAT, set.focus, i);
            cv_rect(5 + (int32_t)(i * cw), 2, (int32_t)cw - 2, 5, on ? C_TXT : !in ? C_BG : has ? ENG_RGB[set.focus] : (i & 3u) ? C_PANEL : C_RULE);
        }
    }
    for (i = 0; i < 16u; i++) {
        int held = ui.keys >> WK[i] & 1u;
        int pad = body == PG_ENGINE && (set.focus == E_CAT || (set.focus == E_HOP && hop_chop_mode() && i < (uint32_t)hop_chops()));
        int mixkey = body == PG_MIX && (i < 4u || (i >= 8u && i < 12u));
        uint16_t c = held ? ENG_RGB[set.focus] : pad ? C_SUB : mixkey ? ENG_DARK[i & 3u] : C_MUTED;
        if (body == PG_SEQ) c = held ? C_TXT : i < PAT->steps ? (seq_lane_has(PAT, set.focus, i) ? ENG_RGB[set.focus] : C_MUTED) : C_PANEL;
        cv_rect(4 + (int32_t)i * 14, 24, 13, 30, c);
        if (pad || mixkey) { int_text((int32_t)(mixkey ? (i & 3u) : i) + 1, t); cv_text(5 + (int32_t)i * 14 + (i < 9u || mixkey ? 2 : 0), 42, &FONT_XS, t, held ? C_BG : C_TXT); }
    }
    for (i = 0; i < 11u; i++) {
        uint16_t c = ui.keys >> BK[i] & 1u ? C_TXT : C_RULE;
        if (body == PG_SEQ) { if (i <= 3u) c = C_BEAVER; else if (i >= 5u && i <= 8u) c = (PAT->mute >> (i - 5u) & 1u) ? C_REC : ENG_DARK[i - 5u]; else if (i == 10u) c = seq.solo ? ENG_RGB[3] : C_RULE; if (ui.keys >> BK[i] & 1u) c = C_TXT; }
        cv_rect(4 + (int32_t)AT[i] * 14 + 8, 10, 11, 18, c);
    }
    cv_text(4, 61, &FONT_XS, body == PG_SEQ ? "COPY PASTE CLR CHAIN  M1-4  SOLO" : body == PG_MIX ? "PRESET KNOB: LEVELS PANS FX" : "HOLD=MUTE REC=ARM ALGO=PRESET", C_SUB);
    int_text((int32_t)(plat_master() * 100u / 4096u), t); text_cat(t, "%");
    cv_text(236 - text_w(&FONT_XS, t), 61, &FONT_XS, t, C_TXT);
    cv_blit(0, 164);
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BG);
    ui.redraw = 1; ui.held_step = -1; ui.held_slice = -1;
}

void app_frame(void)
{
    static uint32_t last_pct, last_meter; static int last_pad = -2, last_slice = -2;
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u;
    input();
    speak();
    if (pct != last_pct || (ui.msg_until && now >= ui.msg_until)) { last_pct = pct; if (now >= ui.msg_until) ui.msg_until = 0; ui.redraw = 1; }
    if (seq.step != ui.shown_step) { ui.shown_step = seq.step; ui.redraw = 1; }
    if (cat_last_pad() != last_pad) { last_pad = cat_last_pad(); ui.redraw = 1; }
    if (hop_last_slice() != last_slice) { last_slice = hop_last_slice(); ui.redraw = 1; }
    if (body == PG_MIX && set.mixpage == 0u && now - last_meter > 80u) { last_meter = now; ui.redraw = 1; }
    if (ui.redraw) { ui.redraw = 0; draw(); }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "focus")) *got = set.focus;
    else if (!strcmp(what, "octave")) *got = set.octave;
    else if (!strcmp(what, "dirty")) *got = set_dirty;
    else if (!strcmp(what, "note")) *got = ui.last_note;
    else if (!strcmp(what, "page")) *got = set.page[set.focus];
    else if (!strcmp(what, "body")) *got = body;
    else if (!strcmp(what, "playing")) *got = seq.playing;
    else if (!strcmp(what, "rec")) *got = seq.rec;
    else if (!strcmp(what, "armed")) *got = seq.armed;
    else if (!strcmp(what, "mute")) *got = PAT->mute;
    else if (!strcmp(what, "solo")) *got = seq.solo;
    else if (!strcmp(what, "chain")) *got = seq.chain;
    else if (!strcmp(what, "step")) *got = seq.step;
    else if (!strcmp(what, "bpm")) *got = PAT->bpm;
    else if (!strcmp(what, "pattern")) *got = set.pattern;
    else if (!strcmp(what, "fox_mode")) *got = fox_mode();
    else if (!strcmp(what, "hop_rec")) *got = hop_recording();
    else if (!strcmp(what, "hop_secs")) *got = (int)hop_bank_seconds_x10();
    else if (!strcmp(what, "hop_peak")) *got = (int)hop_peak(0, hop_bank_len());
    else if (!strcmp(what, "kit")) *got = cat_kit_index();
    else if (!strcmp(what, "mixpage")) *got = set.mixpage;
    else if (!strcmp(what, "cat_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += PAT->lane[E_CAT].mask[i] != 0; *got = (int)n; }
    else if (!strcmp(what, "otter_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += PAT->lane[E_OTTER].note[i] != 0; *got = (int)n; }
    else if (!strncmp(what, "step_", 5)) { const char *w = what + 5; uint32_t st = (uint32_t)atoi(w); const char *c = w; while (*c >= '0' && *c <= '9') c++; *got = *seq_step_param(PAT, set.focus, st, !strcmp(c, "vel") ? 1 : !strcmp(c, "len") ? 2 : !strcmp(c, "prob") ? 3 : 0); }
    else if (!strncmp(what, "param", 5)) { const char *nm; *got = *knob_target((uint32_t)atoi(what + 5) % NPARAM, &nm); }
    else return 0;
    return 1;
}
#endif
