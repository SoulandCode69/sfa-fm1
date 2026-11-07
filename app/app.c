/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * SFA Suite: a groovebox of three engines rendered together, one in focus on the keys, one sequencer, one delay.
 *   FX = OTTER   SEL = CAT   ENV = FOX   focus (again: page 2)     SEQ = the sequencer page (TEMPO SWING STEPS GATE)
 *   LFO = the mix page (OTTER CAT FOX DELAY)                       PRESET knob: page     SELECT knob: focus
 *   keys        play the focused engine (Cat: 16 white keys = pads; Fox CHOP: white keys = slices)
 *   PLAY        run / stop    REC  record what you play onto the steps (hold 1 s: clear the focused lane)
 *   OCT-/OCT+   octave        SAVE keep everything (settings + pattern)    HOME  stop, back to the engine
 *   MIDI in: ch 1 Otter, ch 2 Cat (36..51 = pads), ch 3 Fox; recorded too while REC is lit
 * Part of fm1-base's unity build (device/unity.c, host/plat_host.c), after the platform. */
#include "fastmath.h"
#include "engine/engine.h"

extern const engine_t otter, cat, fox;
int fox_chop_mode(void);
int fox_chops(void);
static const engine_t *const ENG[3] = {&otter, &cat, &fox};
enum { E_OTTER, E_CAT, E_FOX };
enum { PG_ENGINE, PG_SEQ, PG_MIX };
static const uint16_t ENG_RGB[3] = {RGB(90, 200, 255), RGB(255, 170, 40), RGB(255, 90, 60)};
#define C_SFA RGB(255, 64, 160)

#include "icons.c"

#define SET_MAGIC 0x53464133u            /* "SFA3" */
typedef struct {
    uint32_t magic;
    int8_t octave;
    uint8_t focus;
    uint8_t page[3];
    uint8_t level[3];
    uint8_t delay_mix, delay_time, delay_fb, rsv;
    uint8_t param[3][2][NPARAM];
    uint8_t pat[256];                     /* pattern_t */
} settings_t;
static settings_t set;
static uint8_t set_dirty;
static uint8_t body;

#include "seq.c"
#define PAT ((pattern_t *)set.pat)

/* ------------------------------------------------------------------ audio --- */
#define DLY_LEN 11025u                    /* 0.5 s at half rate (22.05 kHz), 22 KB in the pool */
PLAT_POOL static int16_t dly[DLY_LEN];
static uint32_t dly_w;
static float dly_lp, dly_prev, dc_x, dc_y;
static uint8_t dly_tick;
static float mix[3][APP_BLOCK];
static float gain_now;

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
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, e;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;
    float lvl[3], dmix = p01(set.delay_mix) * 0.8f, dfb = p01(set.delay_fb) * 0.8f;
    uint32_t dtime = 1100u + (uint32_t)(p01(set.delay_time) * (float)(DLY_LEN - 1200u));
    midi_in();
    seq_tick(PAT, n);
    for (e = 0; e < 3u; e++) {
        lvl[e] = p01(set.level[e]) * 1.27f;
        for (i = 0; i < n; i++) mix[e][i] = 0.0f;
        ENG[e]->render(mix[e], n);
    }
    for (i = 0; i < n; i++) {
        float dry = mix[0][i] * lvl[0] + mix[1][i] * lvl[1] + mix[2][i] * lvl[2], s, y;
        if (dly_tick ^= 1u) {                            /* the delay line at half rate */
            uint32_t r = (dly_w + DLY_LEN - dtime) % DLY_LEN;
            float d = (float)dly[r] * (1.0f / 32768.0f), w;
            dly_lp += (d - dly_lp) * 0.5f;               /* darker each repeat */
            w = (dly_prev + dry) * 0.5f + dly_lp * dfb;
            if (w > 1.0f) w = 1.0f; else if (w < -1.0f) w = -1.0f;
            dly[dly_w] = (int16_t)(w * 32767.0f);
            dly_w = (dly_w + 1u) % DLY_LEN;
        }
        dly_prev = dry;
        s = dry + dly_lp * dmix;
        y = s - dc_x + 0.995f * dc_y;                    /* DC block */
        dc_x = s; dc_y = y;
        gain_now += (gain - gain_now) * 0.004f;
        s = fm_tanhf(y * 0.8f) * 1.1f * gain_now;        /* a soft ceiling */
        lr[2 * i] = (int32_t)s;
        lr[2 * i + 1] = (int32_t)s;
    }
}

void app_panic(void) { seq_stop(); otter.panic(); cat.panic(); fox.panic(); }

/* --------------------------------------------------------------- settings --- */
static uint8_t *page_param(uint8_t e, uint8_t pg) { return pg && ENG[e]->param2 ? ENG[e]->param2 : ENG[e]->param; }
static const char *const *page_names(uint8_t e, uint8_t pg) { return pg && ENG[e]->param2 ? ENG[e]->param_name2 : ENG[e]->param_name; }
static void params_to_set(void)
{
    uint32_t e, g, p;
    for (e = 0; e < 3u; e++) for (g = 0; g < 2u; g++) for (p = 0; p < NPARAM; p++) set.param[e][g][p] = page_param((uint8_t)e, (uint8_t)g)[p];
}
static void params_from_set(void)
{
    uint32_t e, g, p;
    for (e = 0; e < 3u; e++) for (g = 0; g < 2u; g++) for (p = 0; p < NPARAM; p++) page_param((uint8_t)e, (uint8_t)g)[p] = set.param[e][g][p];
}

void app_init(void)
{
    _Static_assert(sizeof(pattern_t) <= 256u, "the pattern fits");
    otter.init(); cat.init(); fox.init();
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
        && set.octave >= -2 && set.octave <= 2 && set.focus < 3u && set.page[0] < 2u && set.page[1] < 2u && set.page[2] < 2u
        && PAT->bpm >= 40u && PAT->steps >= 1u && PAT->steps <= NSTEP) {
        params_from_set();
    } else {
        set.magic = SET_MAGIC;
        set.octave = 0;
        set.focus = E_OTTER;
        set.page[0] = set.page[1] = set.page[2] = 0;
        set.level[0] = 100; set.level[1] = 110; set.level[2] = 100;
        set.delay_mix = 30; set.delay_time = 60; set.delay_fb = 50;
        pattern_defaults(PAT);
        params_to_set();
    }
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

static void speak(void)
{
    if (ann_on() && ann_where()) {
        char t[64] = APP_NAME ". ";
        text_cat(t, body == PG_SEQ ? "SEQUENCER" : body == PG_MIX ? "MIX" : ENG[set.focus]->name);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

static int32_t key_to_note(uint32_t k, uint8_t e)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS;
    uint32_t w;
    int32_t white = -1;
    for (w = 0; w < 16u; w++) if (WK[w] == k) white = (int32_t)w;
    if (e == E_CAT) return white;
    if (e == E_FOX && fox_chop_mode()) return white;
    return 53 + (int32_t)k + 12 * set.octave;
}

static void page_set(uint8_t pg)
{
    if (ENG[set.focus]->npages < 2u) pg = 0;
    set.page[set.focus] = pg;
    set_dirty = 1;
    ui.redraw = 1;
    app_say(ENG[set.focus]->name, pg ? "PAGE 2" : "PAGE 1");
}
static void focus(uint8_t e)
{
    if (body == PG_ENGINE && e == set.focus) { page_set((uint8_t)!set.page[e]); return; }
    body = PG_ENGINE;
    set.focus = e;
    set_dirty = 1;
    ui.redraw = 1;
    app_say(ENG[e]->name, "");
}

static uint8_t *knob_target(uint32_t p, const char **name)
{
    static const char *const SEQN[4] = {"TEMPO", "SWING", "STEPS", "GATE"};
    static const char *const MIXN[4] = {"OTTER", "CAT", "FOX", "DELAY"};
    if (body == PG_SEQ) {
        *name = SEQN[p];
        return p == 0 ? &PAT->bpm : p == 1 ? &PAT->swing : p == 2 ? &PAT->steps : &PAT->gate;
    }
    if (body == PG_MIX) {
        *name = MIXN[p];
        return p < 3u ? &set.level[p] : &set.delay_mix;
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
    set_dirty = 1;
    int_text(*v, t);
    app_say(name, t);
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
        else if (k == B_SEQ) { body = body == PG_SEQ ? PG_ENGINE : PG_SEQ; ui.redraw = 1; app_say(body == PG_SEQ ? "SEQUENCER" : ENG[set.focus]->name, ""); }
        else if (k == B_LFO) { body = body == PG_MIX ? PG_ENGINE : PG_MIX; ui.redraw = 1; app_say(body == PG_MIX ? "MIX" : ENG[set.focus]->name, ""); }
        else if (k == B_PLAY) { if (seq.playing) seq_stop(); else seq_start(); ui.redraw = 1; app_say(seq.playing ? "PLAY" : "STOP", ""); }
        else if (k == B_REC) ui.rec_t = plat_ms();
        else if (k == B_HOME) { seq_stop(); body = PG_ENGINE; ui.redraw = 1; app_say("HOME", ""); }
        else if (k == B_OCTDN || k == B_OCTUP) {
            int32_t o = clamp(set.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
            char t[4] = {o < 0 ? '-' : '+', (char)('0' + (o < 0 ? -o : o)), 0};
            if (o != set.octave) { otter.panic(); set.octave = (int8_t)o; set_dirty = 1; app_say("OCTAVE", t); }
        } else if (k == B_SAVE) {
            params_to_set();
            if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) { set_dirty = 0; app_say("SAVED", ""); }
            else app_say("SAVE", "FAILED");
        }
    }
    if (ui.rec_t) {
        if (btn >> B_REC & 1u) {
            if (plat_ms() - ui.rec_t >= 1000u) {
                ui.rec_t = 0;
                seq_clear_lane(PAT, set.focus);
                set_dirty = 1;
                app_say(ENG[set.focus]->name, "CLEARED");
            }
        } else {
            ui.rec_t = 0;
            seq.rec = (uint8_t)!seq.rec;
            ui.redraw = 1;
            app_say("REC", seq.rec ? "ON" : "OFF");
        }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0) continue;
        if (k >= EN_K1 && k < EN_K1 + NPARAM) knob(k - EN_K1, e);
        else if (k == EN_PRESET) { body = PG_ENGINE; page_set((uint8_t)!set.page[set.focus]); }
        else if (k == EN_SELECT) { body = PG_ENGINE; set.focus = (uint8_t)((set.focus + 3 + (e > 0 ? 1 : 2)) % 3u); set_dirty = 1; app_say(ENG[set.focus]->name, ""); }
    }
    if (keys != ui.keys || btn != ui.btn) ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    plat_leds((1u << (B_FX + set.focus)) | (set_dirty ? 1u << B_SAVE : 0u) | (seq.playing ? 1u << B_PLAY : 0u)
              | (seq.rec ? 1u << B_REC : 0u) | (body == PG_SEQ ? 1u << B_SEQ : 0u) | (body == PG_MIX ? 1u << B_LFO : 0u), keys);
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

static void draw(void)
{
    char t[16];
    const engine_t *cur = ENG[set.focus];
    uint32_t i, now = plat_ms();
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
    uint16_t accent = body == PG_SEQ ? C_SFA : body == PG_MIX ? C_WHITE : ENG_RGB[set.focus];
    int32_t st = seq.step;

    /* header: SFA, the three animals as tabs, tempo and transport */
    cv_begin(240, 36, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 9, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 9, &FONT_B, ui.msg[1], accent);
    } else {
        const char *tr = seq.rec ? "REC" : seq.playing ? "PLAY" : "STOP";
        cv_text(8, 9, &FONT_B, APP_NAME, C_SFA);
        for (i = 0; i < 3u; i++) {
            int32_t x = 44 + (int32_t)i * 52;
            int on = body == PG_ENGINE && i == set.focus;
            cv_rect(x, 4, 49, 28, on ? ENG_RGB[i] : C_LINE);
            icon(x + 3, 12, ICONS[i], on ? C_BLACK : ENG_RGB[i], 1);
            cv_text(x + 17, 11, &FONT_XS, ENG[i]->name, on ? C_BLACK : C_GRAY);
        }
        int_text(PAT->bpm, t);
        cv_text(234 - text_w(&FONT_XS, t), 3, &FONT_XS, t, seq.playing ? C_SFA : C_GRAY);
        cv_text(234 - text_w(&FONT_XS, tr), 18, &FONT_XS, tr, seq.rec ? RGB(255, 40, 40) : seq.playing ? C_SFA : C_DIM);
    }
    cv_blit(0, 0);

    /* body */
    cv_begin(240, 128, C_BLACK);
    if (body == PG_ENGINE) {
        icon(8, 2, ICONS[set.focus], accent, 2);
        cv_text(36, 8, &FONT_B, cur->name, accent);
        if (cur->npages > 1u) {
            cv_rect(36, 26, 6, 4, set.page[set.focus] == 0 ? accent : C_LINE);
            cv_rect(46, 26, 6, 4, set.page[set.focus] == 1 ? accent : C_LINE);
        }
        if (set.focus == E_OTTER || (set.focus == E_FOX && !fox_chop_mode())) {
            t[0] = set.octave < 0 ? '-' : '+'; t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave)); t[2] = 0;
            cv_text(190, 10, &FONT_XS, "OCT", C_GRAY);
            cv_text(214, 10, &FONT_XS, t, C_HI);
        } else if (set.focus == E_FOX) {
            cv_text(170, 10, &FONT_XS, "CHOP", C_HI);
        } else {
            cv_text(166, 10, &FONT_XS, "16 PADS", C_HI);
        }
        for (i = 0; i < NPARAM; i++) {
            const char *nm, *txt = 0;
            uint8_t *pp = knob_target(i, &nm);
            int sw = set.focus == E_FOX && set.page[E_FOX] == 1u && i < 2u;
            int32_t v = *pp;
            if (sw) { txt = v >= 64 ? "ON" : "OFF"; v = v >= 64 ? 127 : 0; }
            else if (set.focus == E_FOX && set.page[E_FOX] == 0u && i == 2u) { int_text(fox_chops(), t); txt = t; }
            else if (ENG[set.focus]->param_text) txt = ENG[set.focus]->param_text(set.page[set.focus], i, t);
            bar(10 + (int32_t)i * 57, 36, nm, v, 0, 127, accent, txt);
        }
    } else if (body == PG_SEQ) {
        uint32_t e, s;
        cv_text(8, 4, &FONT_B, "SEQUENCER", accent);
        cv_text(104, 8, &FONT_XS, "REC+KEYS WRITES. HOLD REC CLEARS", C_GRAY);
        for (e = 0; e < 3u; e++) {
            int32_t y = 24 + (int32_t)e * 10;
            icon(8, y - 2, ICONS[e], ENG_RGB[e], 1);
            for (s = 0; s < NSTEP; s++) {
                int has = seq_lane_has(PAT, (uint8_t)e, s), in = s < PAT->steps;
                cv_rect(26 + (int32_t)s * 13, y, 11, 8, !in ? C_BLACK : has ? ENG_RGB[e] : (s & 3u) ? C_LINE : C_DIM);
            }
        }
        bar_h(10, 58, 40, "TEMPO", PAT->bpm, 40, 240, accent, 0);
        bar_h(67, 58, 40, "SWING", PAT->swing, 0, 127, accent, 0);
        bar_h(124, 58, 40, "STEPS", PAT->steps, 0, 16, accent, 0);
        bar_h(181, 58, 40, "GATE", PAT->gate, 0, 127, accent, 0);
    } else {
        cv_text(8, 4, &FONT_B, "MIX", accent);
        for (i = 0; i < 3u; i++) bar(10 + (int32_t)i * 57, 36, ENG[i]->name, set.level[i], 0, 127, ENG_RGB[i], 0);
        bar(181, 36, "DELAY", set.delay_mix, 0, 127, C_WHITE, 0);
    }
    cv_blit(0, 36);

    /* bottom: the step strip, the keys, the legend */
    cv_begin(240, 76, C_BLACK);
    for (i = 0; i < NSTEP; i++) {
        int in = i < PAT->steps, on = seq.playing && (int32_t)i == st;
        int has = seq_lane_has(PAT, set.focus, i);
        cv_rect(5 + (int32_t)i * 14, 2, 12, 6, on ? C_WHITE : !in ? C_BLACK : has ? ENG_RGB[set.focus] : (i & 3u) ? C_LINE : C_DIM);
    }
    for (i = 0; i < 16u; i++) {
        int held = ui.keys >> WK[i] & 1u;
        int pad = set.focus == E_CAT ? 1 : set.focus == E_FOX && fox_chop_mode() && i < (uint32_t)fox_chops();
        int smp = set.focus == E_CAT && i >= 8u;
        cv_rect(4 + (int32_t)i * 14, 26, 13, 30, held ? ENG_RGB[set.focus] : smp ? ENG_RGB[E_FOX] : pad ? C_GRAY : C_DIM);
        if (pad) { int_text((int32_t)i + 1, t); cv_text(5 + (int32_t)i * 14 + (i < 9u ? 2 : 0), 44, &FONT_XS, t, held ? C_BLACK : C_WHITE); }
    }
    for (i = 0; i < 11u; i++)
        cv_rect(4 + (int32_t)AT[i] * 14 + 8, 12, 11, 18, ui.keys >> BK[i] & 1u ? C_WHITE : C_LINE);
    cv_text(4, 61, &FONT_XS, "SEQ  LFO=MIX  PLAY  REC  2x=PAGE", C_GRAY);
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
    static uint32_t last_pct;
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u;
    input();
    speak();
    if (pct != last_pct || (ui.msg_until && now >= ui.msg_until)) {
        last_pct = pct;
        if (now >= ui.msg_until) ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (seq.step != ui.shown_step) { ui.shown_step = seq.step; ui.redraw = 1; }
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
    else if (!strcmp(what, "cat_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += PAT->cat_mask[i] != 0; *got = (int)n; }
    else if (!strcmp(what, "otter_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += PAT->otter_note[i] != 0; *got = (int)n; }
    else if (!strncmp(what, "param", 5)) { const char *nm; *got = *knob_target((uint32_t)atoi(what + 5) % NPARAM, &nm); }
    else return 0;
    return 1;
}
#endif
