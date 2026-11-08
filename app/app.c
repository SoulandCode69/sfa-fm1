/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * EARTH CREATURE BEATS: four engines rendered together, one in focus on the keys, the Beaver sequencing them.
 *   FX OTTER   SEL CAT   ENV FOX   LFO HOPPER      focus (press again: next page)      SEQ  BEAVER
 *   EDIT       mixer (PRESET knob flips to FX)     GLO  tap tempo (tap four times)
 *   ALGO knob  presets / kits / bass / patterns    PRESET knob  page     SELECT knob  focus
 *   PLAY REC   run / record onto the steps (hold REC 1 s: clear the focused lane)
 *   ARP        SAMPLE: Hopper records the output until ARP again      HOME stop      SAVE keep everything
 *   MIDI in: ch 1 Otter, ch 2 Cat (36..51 = pads), ch 3 Fox, ch 4 Hopper; recorded while REC is lit
 * Part of fm1-base's unity build (device/unity.c, host/plat_host.c), after the platform. */
#include "fastmath.h"
#include "engine/engine.h"

extern const engine_t otter, cat, fox, hopper;
void otter_preset(int32_t step); void otter_preset_set(int i); int otter_preset_index(void); const char *otter_preset_name(void);
int otter_arp_on(void); int otter_arp_rate(void); void otter_arp_tick(void);
void cat_kit(int32_t step); void cat_kit_set(int k); int cat_kit_index(void); const char *cat_kit_name(void);
const char *cat_pad_name(int pad); int cat_last_pad(void); uint8_t *cat_pad_blob(uint32_t *size); void cat_pads_default(void); void cat_sync(void);
void fox_preset(int32_t step); void fox_preset_set(int i); int fox_preset_index(void); const char *fox_preset_name(void); int fox_mode(void); void fox_mode_set_params(void);
int hop_chop_mode(void); int hop_chops(void); int hop_recording(void); void hop_record_start(void); void hop_record_stop(void);
void hop_record_feed(const float *mix, uint32_t n); int hop_save_needed(void); int hop_save(void); uint32_t hop_bank_seconds_x10(void);
uint32_t hop_bank_len(void); int32_t hop_peak(uint32_t from, uint32_t to); uint32_t hop_window(uint32_t *a, uint32_t *b); float hop_playhead(void);
uint32_t hop_slice_points(const uint32_t **pts); uint8_t *hop_slice_blob(uint32_t *size); void hop_slices_default(void); int hop_last_slice(void); void hop_sync(void); void hop_rechop(void);

#define NENG 4
static const engine_t *const ENG[NENG] = {&otter, &cat, &fox, &hopper};
enum { E_OTTER, E_CAT, E_FOX, E_HOP };
enum { PG_ENGINE, PG_SEQ, PG_MIX };
static const uint16_t ENG_RGB[5] = {RGB(90, 200, 255), RGB(255, 170, 40), RGB(255, 90, 60), RGB(130, 230, 120), RGB(170, 120, 80)};
#define C_BEAVER ENG_RGB[4]
#define C_SFA RGB(255, 64, 160)

#include "icons.c"

#define SET_MAGIC 0x45434231u            /* "ECB1" */
typedef struct {
    uint32_t magic;
    int8_t octave;
    uint8_t focus;
    uint8_t page[NENG];
    uint8_t level[NENG], pan[NENG];       /* the mixer: 0..127, pan 64 = centre */
    uint8_t delay_mix, delay_time, delay_fb, crush, drive, mixpage;
    uint8_t otter_preset, fox_preset, cat_kit, pattern;
    uint8_t rsv[4];
    uint8_t param[NENG][NPAGES][NPARAM];
    uint8_t pads[4 * 16 * 4];             /* Cat's per-pad params */
    uint8_t slices[16 * 4];               /* Hopper's per-slice params */
    uint8_t pat[4][320];                  /* pattern_t x 4 slots */
} settings_t;
static settings_t set;
static uint8_t set_dirty;
static uint8_t body;

#include "seq.c"
#define PAT ((pattern_t *)set.pat[set.pattern])

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
        if (ch == 0u) note_in(E_OTTER, (int32_t)d1, d2, on);
        else if (ch == 1u) note_in(E_CAT, (int32_t)d1 - 36, d2, on);
        else if (ch == 2u) note_in(E_FOX, (int32_t)d1, d2, on);
        else if (ch == 3u) note_in(E_HOP, (int32_t)d1, d2, on);
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
        float lvl = p01(set.level[e]) * 1.27f, pan = p01(set.pan[e]);     /* a constant-power pan */
        gl[e] = lvl * fm_sinf((1.0f - pan) * 1.5707963f); gr[e] = lvl * fm_sinf(pan * 1.5707963f);
        for (i = 0; i < n; i++) mix[e][i] = 0.0f;
        ENG[e]->render(mix[e], n);
    }
    for (i = 0; i < n; i++) {
        float l = 0.0f, r = 0.0f, mono, d = 0.0f, y[2], a, pk;
        for (e = 0; e < NENG; e++) { l += mix[e][i] * gl[e]; r += mix[e][i] * gr[e]; }
        mono = (l + r) * 0.5f;
        if (dly_tick ^= 1u) {                                      /* the delay line at half rate, fed the mono sum */
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
        /* the limiter: one gain for both sides, fast attack, slow release, 0.9 ceiling */
        a = fm_fabsf(y[0]); pk = fm_fabsf(y[1]); if (pk > a) a = pk;
        if (a > lim_env) lim_env += (a - lim_env) * 0.5f; else lim_env += (a - lim_env) * 0.0005f;
        if (lim_env > 0.9f) { float g = 0.9f / lim_env; y[0] *= g; y[1] *= g; }
        if (!(y[0] == y[0]) || !(y[1] == y[1])) {               /* a NaN would silence everything for good: reset */
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
    set.otter_preset = (uint8_t)otter_preset_index();
    set.fox_preset = (uint8_t)fox_preset_index();
    set.cat_kit = (uint8_t)cat_kit_index();
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
}

void app_init(void)
{
    uint32_t e;
    _Static_assert(sizeof(pattern_t) <= 320u, "the pattern fits");
    _Static_assert(sizeof(settings_t) <= 3900u, "the settings fit a store object");
    for (e = 0; e < NENG; e++) ENG[e]->init();
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
        && set.octave >= -2 && set.octave <= 2 && set.focus < NENG && set.pattern < 4u
        && PAT->bpm >= 40u && PAT->steps >= 1u && PAT->steps <= NSTEP) {
        for (e = 0; e < NENG; e++) if (set.page[e] >= ENG[e]->npages) set.page[e] = 0;
        otter_preset_set(set.otter_preset);
        fox_preset_set(set.fox_preset);
        cat_kit_set(set.cat_kit);
        params_from_set();
        fox_mode_set_params();
    } else {
        uint32_t k;
        set.magic = SET_MAGIC;
        set.octave = 0;
        set.focus = E_CAT;
        for (e = 0; e < NENG; e++) { set.page[e] = 0; set.level[e] = 100; set.pan[e] = 64; }
        set.level[E_CAT] = 110;
        set.delay_mix = 0; set.delay_time = 60; set.delay_fb = 50; set.crush = 0; set.drive = 0; set.mixpage = 0;
        set.otter_preset = 0; set.fox_preset = 0; set.cat_kit = 0; set.pattern = 0;
        cat_pads_default(); hop_slices_default();
        for (k = 0; k < 4u; k++) pattern_defaults((pattern_t *)set.pat[k]);
        params_to_set();
    }
    cat.init(); hopper.init();                                   /* their pad / slice views pick up the blobs */
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
    uint32_t rec_t;
    int32_t shown_step;
    uint32_t tap_t[4]; uint8_t tap_n;
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
    ui.msg_until = plat_ms() + 1200u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0; text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) { text_cat(t, " "); text_cat(t, ui.msg[1]); }
        ann_say(t, 0, ANN_NOW);
    }
}

static const char *body_name(void) { return body == PG_SEQ ? "BEAVER" : body == PG_MIX ? (set.mixpage ? "FX" : "MIXER") : ENG[set.focus]->name; }

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
static int32_t key_to_note(uint32_t k, uint8_t e)
{
    if (e == E_CAT) return white_index(k);
    if (e == E_HOP && hop_chop_mode()) return white_index(k);
    return (e == E_FOX ? 29 : 53) + (int32_t)k + 12 * set.octave;     /* Fox two octaves down: key 0 = F1 */
}

static void page_set(uint8_t pg)
{
    char t[8] = "PAGE 1";
    pg %= ENG[set.focus]->npages;
    set.page[set.focus] = pg;
    set_dirty = 1;
    ui.redraw = 1;
    t[5] = (char)('1' + pg);
    app_say(ENG[set.focus]->name, t);
}
static void focus(uint8_t e)
{
    if (body == PG_ENGINE && e == set.focus) { page_set((uint8_t)(set.page[e] + 1u)); return; }
    body = PG_ENGINE;
    set.focus = e;
    set_dirty = 1;
    ui.redraw = 1;
    app_say(ENG[e]->name, "");
}

static uint8_t *knob_target(uint32_t p, const char **name)
{
    static const char *const SEQN[4] = {"TEMPO", "SWING", "STEPS", "GATE"};
    static const char *const MIXN[4] = {"OTTER", "CAT", "FOX", "HOPPER"};
    static const char *const MIX2[4] = {"DELAY", "DTIME", "DFEEDB", "CRUSH"};
    static const char *const PANN[4] = {"PAN OTT", "PAN CAT", "PAN FOX", "PAN HOP"};
    if (body == PG_SEQ) { *name = SEQN[p]; return p == 0 ? &PAT->bpm : p == 1 ? &PAT->swing : p == 2 ? &PAT->steps : &PAT->gate; }
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
    char t[8];
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
        set_dirty = 1;
        int_text(PAT->bpm, t);
        app_say("TAP", t);
    } else app_say("TAP", "...");
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            int32_t note = key_to_note(k, set.focus);
            uint32_t v = touch_vel(k);
            if (body == PG_SEQ) {                                      /* the Beaver: a white key toggles that step */
                int32_t w = white_index(k);
                if (w >= 0) { seq_toggle(PAT, set.focus, (uint32_t)w, ui.last_note, cat_last_pad(), hop_last_slice()); set_dirty = 1; ui.redraw = 1; }
                ui.key_note[k] = -1;
                continue;
            }
            ui.key_note[k] = note;
            ui.key_eng[k] = set.focus;
            if (note >= 0) {
                note_in(set.focus, note, v, 1);
                ui.last_note = note;
                if (set.focus != E_CAT)
                    plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB);
            }
        } else if (!(keys & bit) && (ui.keys & bit)) {
            int32_t note = ui.key_note[k];
            if (note >= 0) {
                ENG[ui.key_eng[k]]->note(note, 0, 0);
                if (ui.key_eng[k] != E_CAT)
                    plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB);
            }
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit)) continue;
        if (k == B_FX) focus(E_OTTER);
        else if (k == B_SEL) focus(E_CAT);
        else if (k == B_ENV) focus(E_FOX);
        else if (k == B_LFO) focus(E_HOP);
        else if (k == B_SEQ) { body = body == PG_SEQ ? PG_ENGINE : PG_SEQ; ui.redraw = 1; app_say(body_name(), ""); }
        else if (k == B_EDIT) { body = body == PG_MIX ? PG_ENGINE : PG_MIX; ui.redraw = 1; app_say(body_name(), ""); }
        else if (k == B_GLO) tap_tempo();
        else if (k == B_PLAY) { if (seq.playing) seq_stop(); else seq_start(); ui.redraw = 1; app_say(seq.playing ? "PLAY" : "STOP", ""); }
        else if (k == B_REC) ui.rec_t = plat_ms();
        else if (k == B_ARP) {
            if (hop_recording()) { hop_record_stop(); app_say("SAMPLED", ""); set_dirty = 1; }
            else { hop_record_start(); body = PG_ENGINE; set.focus = E_HOP; app_say("SAMPLING", "ARP STOPS"); }
            ui.redraw = 1;
        }
        else if (k == B_HOME) { seq_stop(); body = PG_ENGINE; ui.redraw = 1; app_say("HOME", ""); }
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
    if (ui.rec_t) {
        if (btn >> B_REC & 1u) {
            if (plat_ms() - ui.rec_t >= 1000u) { ui.rec_t = 0; seq_clear_lane(PAT, set.focus); set_dirty = 1; app_say(ENG[set.focus]->name, "CLEARED"); }
        } else { ui.rec_t = 0; seq.rec = (uint8_t)!seq.rec; ui.redraw = 1; app_say("REC", seq.rec ? "ON" : "OFF"); }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0) continue;
        if (k >= EN_K1 && k < EN_K1 + NPARAM) knob(k - EN_K1, e);
        else if (k == EN_PRESET) {
            if (body == PG_MIX) { set.mixpage = (uint8_t)((set.mixpage + 3u + (e > 0 ? 1u : 2u)) % 3u); ui.redraw = 1; app_say("MIXER", set.mixpage == 0 ? "LEVELS" : set.mixpage == 1 ? "PANS" : "FX"); }
            else if (body == PG_SEQ) { }
            else page_set((uint8_t)(set.page[set.focus] + (e > 0 ? 1u : ENG[set.focus]->npages - 1u)));
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
            }
            set_dirty = 1; ui.redraw = 1;
        } else if (k == EN_SELECT) {
            if (body != PG_SEQ) body = PG_ENGINE;
            set.focus = (uint8_t)((set.focus + NENG + (e > 0 ? 1u : NENG - 1u)) % NENG);
            set_dirty = 1; app_say(body == PG_SEQ ? "LANE" : ENG[set.focus]->name, body == PG_SEQ ? ENG[set.focus]->name : "");
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

static void bar_h(int32_t x, int32_t y, int32_t bh, const char *name, int32_t v, int32_t lo, int32_t hi, uint16_t col, const char *txt)
{
    int32_t h = (v - lo) * (bh - 2) / (hi - lo);
    char t[8];
    cv_rect(x, y, 50, bh, C_LINE);
    cv_rect(x + 1, y + bh - 1 - h, 48, h, col);
    cv_text(x + (50 - text_w(&FONT_XS, name)) / 2, y + bh + 4, &FONT_XS, name, C_GRAY);
    if (!txt) { int_text(v, t); txt = t; }
    cv_text(x + (50 - text_w(&FONT_XS, txt)) / 2, y + bh + 16, &FONT_XS, txt, C_HI);
}
#define bar(x, y, name, v, lo, hi, col, txt) bar_h(x, y, 58, name, v, lo, hi, col, txt)

static void pan_bar(int32_t x, int32_t y, const char *name, int32_t v, uint16_t col)
{
    char t[8];
    int32_t c = x + 25, p = (v - 64) * 23 / 64;
    cv_rect(x, y, 50, 58, C_LINE);
    cv_rect(c - 1, y + 2, 2, 54, C_DIM);
    if (p >= 0) cv_rect(c, y + 20, p + 1, 18, col); else cv_rect(c + p, y + 20, -p + 1, 18, col);
    cv_text(x + (50 - text_w(&FONT_XS, name)) / 2, y + 62, &FONT_XS, name, C_GRAY);
    if (v == 64) { t[0] = 'C'; t[1] = 0; } else { t[0] = v < 64 ? 'L' : 'R'; int_text(v < 64 ? 64 - v : v - 64, t + 1); }
    cv_text(x + (50 - text_w(&FONT_XS, t)) / 2, y + 74, &FONT_XS, t, C_HI);
}

static void draw(void)
{
    char t[16];
    const engine_t *cur = ENG[set.focus];
    uint32_t i, now = plat_ms();
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
    uint16_t accent = body == PG_SEQ ? C_BEAVER : body == PG_MIX ? C_WHITE : ENG_RGB[set.focus];
    int32_t st = seq.step;

    /* header: ECB, the five animals as tabs, the page, tempo and transport */
    cv_begin(240, 36, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 9, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 9, &FONT_B, ui.msg[1], accent);
    } else {
        const char *tr = seq.rec ? "REC" : seq.playing ? "PLAY" : "STOP";
        cv_text(5, 9, &FONT_B, "ECB", C_SFA);
        for (i = 0; i < 5u; i++) {
            int32_t x = 38 + (int32_t)i * 26;
            int on = i < NENG ? body == PG_ENGINE && i == set.focus : body == PG_SEQ;
            cv_rect(x, 4, 24, 28, on ? ENG_RGB[i] : C_LINE);
            icon(x + 4, 10, ICONS[i], on ? C_BLACK : ENG_RGB[i], 1);
        }
        int_text(PAT->bpm, t);
        cv_text(234 - text_w(&FONT_XS, t), 3, &FONT_XS, t, seq.playing ? C_SFA : C_GRAY);
        cv_text(234 - text_w(&FONT_XS, tr), 18, &FONT_XS, tr, seq.rec ? RGB(255, 40, 40) : seq.playing ? C_SFA : C_DIM);
        t[0] = (char)('A' + set.pattern); t[1] = 0;
        cv_text(234 - text_w(&FONT_XS, "120") - 10, 3, &FONT_XS, t, C_BEAVER);
    }
    cv_blit(0, 0);

    /* body */
    cv_begin(240, 128, C_BLACK);
    if (body == PG_ENGINE) {
        int hop_wave = set.focus == E_HOP && set.page[E_HOP] < 3u;
        icon(6, 0, ICONS[set.focus], accent, 2);
        cv_text(42, 6, &FONT_B, cur->name, accent);
        for (i = 0; i < cur->npages; i++)
            cv_rect(42 + (int32_t)i * 10, 24, 6, 4, set.page[set.focus] == i ? accent : C_LINE);
        {   /* the context line: preset / kit + pad / bass + 808 / sample state */
            const char *a = "", *b = "";
            if (set.focus == E_OTTER) { a = otter_preset_name(); t[0] = set.octave < 0 ? '-' : '+'; t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave)); t[2] = 0; b = t; }
            else if (set.focus == E_CAT) { a = cat_kit_name(); b = cat_pad_name(cat_last_pad()); }
            else if (set.focus == E_FOX) { a = fox_mode() ? "BASS" : "808"; b = fox_preset_name(); }
            else {
                static char s[8];
                a = hop_recording() ? "SAMPLING" : hop_chop_mode() ? "CHOP" : "KEYS";
                int_text((int32_t)hop_bank_seconds_x10() / 10, s); text_cat(s, "."); int_text((int32_t)hop_bank_seconds_x10() % 10, s + 2); text_cat(s, "S");
                b = s;
            }
            cv_text(232 - text_w(&FONT_XS, b), 10, &FONT_XS, b, set.focus == E_HOP && hop_recording() ? RGB(255, 40, 40) : C_HI);
            cv_text(232 - text_w(&FONT_XS, b) - 8 - text_w(&FONT_XS, a), 10, &FONT_XS, a, C_GRAY);
        }
        if (hop_wave) {                                           /* the bank as a waveform, the window, slices, playhead */
            uint32_t a, b, nc = hop_window(&a, &b), len = hop_bank_len(), c;
            const uint32_t *pts; uint32_t npts = hop_slice_points(&pts);
            float ph = hop_playhead();
            cv_rect(10, 30, 220, 26, C_LINE);
            for (c = 0; c < 110u; c++) {
                uint32_t f0 = len * c / 110u, f1 = len * (c + 1u) / 110u;
                int32_t h = hop_peak(f0, f1) * 12 / 32768, in = f0 >= a && f1 <= b;
                if (h < 1) h = 1;
                cv_rect(10 + (int32_t)c * 2, 43 - h, 2, h * 2, in ? accent : C_GRAY);
            }
            if (npts && nc > 1u) { for (c = 1; c < npts; c++) cv_rect(10 + (int32_t)(pts[c] * 220u / (len ? len : 1u)), 30, 1, 26, C_WHITE); }
            else for (c = 1; c < nc; c++) cv_rect(10 + (int32_t)((a + (b - a) * c / nc) * 220u / (len ? len : 1u)), 30, 1, 26, C_WHITE);
            if (ph >= 0.0f) cv_rect(10 + (int32_t)(ph * 219.0f), 30, 1, 26, C_WHITE);
        }
        for (i = 0; i < NPARAM; i++) {
            const char *nm, *txt = 0;
            uint8_t *pp = knob_target(i, &nm);
            int sw = set.focus == E_HOP && set.page[E_HOP] == 1u && i < 2u;
            int32_t v = *pp;
            if (sw) { txt = v >= 64 ? "ON" : "OFF"; v = v >= 64 ? 127 : 0; }
            else if (set.focus == E_HOP && set.page[E_HOP] == 0u && i == 2u) { int_text(hop_chops(), t); txt = t; }
            else if (cur->param_text) txt = cur->param_text(set.page[set.focus], i, t);
            if (hop_wave) bar_h(10 + (int32_t)i * 57, 60, 36, nm, v, 0, 127, accent, txt);
            else bar(10 + (int32_t)i * 57, 36, nm, v, 0, 127, accent, txt);
        }
    } else if (body == PG_SEQ) {
        uint32_t e, s, shown = PAT->steps > 16u ? 32u : 16u, cw = shown > 16u ? 6u : 13u;
        icon(6, 0, ICONS[4], accent, 2);
        cv_text(42, 6, &FONT_B, "BEAVER", accent);
        for (i = 0; i < 4u; i++) {
            char l[2] = {(char)('A' + i), 0};
            cv_rect(112 + (int32_t)i * 18, 4, 16, 16, i == set.pattern ? accent : C_LINE);
            cv_text(112 + (int32_t)i * 18 + 5, 6, &FONT_XS, l, i == set.pattern ? C_BLACK : C_GRAY);
        }
        cv_text(232 - text_w(&FONT_XS, ENG[set.focus]->name), 8, &FONT_XS, ENG[set.focus]->name, ENG_RGB[set.focus]);
        for (e = 0; e < NENG; e++) {
            int32_t y = 30 + (int32_t)e * 11;
            cv_rect(10, y + 1, 10, 8, e == set.focus ? ENG_RGB[e] : C_GRAY);              /* the lane's colour, the focused one bright */
            for (s = 0; s < shown; s++) {
                int has = seq_lane_has(PAT, (uint8_t)e, s), in = s < PAT->steps, on = seq.playing && (int32_t)s == st;
                cv_rect(26 + (int32_t)(s * cw), y + 1, (int32_t)cw - 2, 8, on && has ? C_WHITE : !in ? C_BLACK : has ? ENG_RGB[e] : (s & 3u) ? C_LINE : C_DIM);
            }
        }
        bar_h(10, 78, 20, "TEMPO", PAT->bpm, 40, 240, accent, 0);
        bar_h(67, 78, 20, "SWING", PAT->swing, 0, 127, accent, 0);
        bar_h(124, 78, 20, "STEPS", PAT->steps, 0, NSTEP, accent, 0);
        bar_h(181, 78, 20, "GATE", PAT->gate, 0, 127, accent, 0);
    } else {
        cv_text(8, 4, &FONT_B, set.mixpage == 0 ? "MIXER" : set.mixpage == 1 ? "PANS" : "FX", accent);
        cv_text(232 - text_w(&FONT_XS, "PRESET KNOB TURNS THE PAGE"), 8, &FONT_XS, "PRESET KNOB TURNS THE PAGE", C_GRAY);
        if (set.mixpage == 1u) { for (i = 0; i < NENG; i++) pan_bar(10 + (int32_t)i * 57, 36, ENG[i]->name, set.pan[i], ENG_RGB[i]); }
        else if (set.mixpage == 2u) {
            bar(10, 36, "DELAY", set.delay_mix, 0, 127, C_WHITE, set.delay_mix ? 0 : "DRY");
            bar(67, 36, "DTIME", set.delay_time, 0, 127, C_WHITE, 0);
            bar(124, 36, "DFEEDB", set.delay_fb, 0, 127, C_WHITE, 0);
            bar(181, 36, "CRUSH", set.crush, 0, 127, C_WHITE, set.crush ? 0 : "OFF");
        } else for (i = 0; i < NENG; i++) bar(10 + (int32_t)i * 57, 36, ENG[i]->name, set.level[i], 0, 127, ENG_RGB[i], 0);
    }
    cv_blit(0, 36);

    /* bottom: the step strip, the keys, the legend */
    cv_begin(240, 76, C_BLACK);
    {
        uint32_t shown = PAT->steps > 16u ? 32u : 16u, cw = shown > 16u ? 7u : 14u;
        for (i = 0; i < shown; i++) {
            int in = i < PAT->steps, on = seq.playing && (int32_t)i == st, has = seq_lane_has(PAT, set.focus, i);
            cv_rect(5 + (int32_t)(i * cw), 2, (int32_t)cw - 2, 6, on ? C_WHITE : !in ? C_BLACK : has ? ENG_RGB[set.focus] : (i & 3u) ? C_LINE : C_DIM);
        }
    }
    for (i = 0; i < 16u; i++) {
        int held = ui.keys >> WK[i] & 1u;
        int pad = set.focus == E_CAT ? 1 : set.focus == E_HOP && hop_chop_mode() && i < (uint32_t)hop_chops();
        cv_rect(4 + (int32_t)i * 14, 26, 13, 30, held ? ENG_RGB[set.focus] : pad ? C_GRAY : C_DIM);
        if (pad) { int_text((int32_t)i + 1, t); cv_text(5 + (int32_t)i * 14 + (i < 9u ? 2 : 0), 44, &FONT_XS, t, held ? C_BLACK : C_WHITE); }
    }
    for (i = 0; i < 11u; i++)
        cv_rect(4 + (int32_t)AT[i] * 14 + 8, 12, 11, 18, ui.keys >> BK[i] & 1u ? C_WHITE : C_LINE);
    cv_text(4, 61, &FONT_XS, body == PG_SEQ ? "KEYS=STEPS  ALGO=PATTERN  SEL=LANE" : "ALGO=PRESET  EDIT=MIX  GLO=TAP", C_GRAY);
    int_text((int32_t)(plat_master() * 100u / 4096u), t);
    text_cat(t, "%");
    cv_text(236 - text_w(&FONT_XS, t), 61, &FONT_XS, t, C_HI);
    cv_blit(0, 164);
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_pct; static int last_pad = -2, last_slice = -2;
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u;
    input();
    speak();
    if (pct != last_pct || (ui.msg_until && now >= ui.msg_until)) {
        last_pct = pct;
        if (now >= ui.msg_until) ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (seq.step != ui.shown_step) { ui.shown_step = seq.step; ui.redraw = 1; }
    if (cat_last_pad() != last_pad) { last_pad = cat_last_pad(); ui.redraw = 1; }
    if (hop_last_slice() != last_slice) { last_slice = hop_last_slice(); ui.redraw = 1; }
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
    else if (!strcmp(what, "step")) *got = seq.step;
    else if (!strcmp(what, "bpm")) *got = PAT->bpm;
    else if (!strcmp(what, "pattern")) *got = set.pattern;
    else if (!strcmp(what, "fox_mode")) *got = fox_mode();
    else if (!strcmp(what, "hop_rec")) *got = hop_recording();
    else if (!strcmp(what, "hop_secs")) *got = (int)hop_bank_seconds_x10();
    else if (!strcmp(what, "hop_peak")) *got = (int)hop_peak(0, hop_bank_len());
    else if (!strcmp(what, "kit")) *got = cat_kit_index();
    else if (!strcmp(what, "mixpage")) *got = set.mixpage;
    else if (!strcmp(what, "cat_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += PAT->cat_mask[i] != 0; *got = (int)n; }
    else if (!strcmp(what, "otter_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += PAT->otter_note[i] != 0; *got = (int)n; }
    else if (!strncmp(what, "param", 5)) { const char *nm; *got = *knob_target((uint32_t)atoi(what + 5) % NPARAM, &nm); }
    else return 0;
    return 1;
}
#endif
