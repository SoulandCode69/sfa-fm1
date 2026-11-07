/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * SFA Suite: three engines rendered together, one in focus on the keys.
 *   FX = OTTER   SEL = CAT   ENV = FOX      (focus; the LED shows which)
 *   keys        play the focused engine (Cat: first 8 white keys = pads; Fox CHOP: white keys = slices)
 *   K1..K4      the focused engine's four parameters      OCT-/OCT+  octave (Otter, Fox chromatic)
 *   SAVE        keep settings       MIDI in: ch 1 Otter, ch 2 Cat (36..43 = pads), ch 3 Fox
 * Part of fm1-base's unity build (device/unity.c, host/plat_host.c), after the platform. */
#include "fastmath.h"
#include "engine/engine.h"

extern const engine_t otter, cat, fox;
int fox_chop_mode(void);
int fox_chops(void);
static const engine_t *const ENG[3] = {&otter, &cat, &fox};
enum { E_OTTER, E_CAT, E_FOX };
static const uint16_t ENG_RGB[3] = {RGB(90, 200, 255), RGB(255, 170, 40), RGB(255, 90, 60)};

#define SET_MAGIC 0x53464131u            /* "SFA1" */
typedef struct {
    uint32_t magic;
    int8_t octave;
    uint8_t focus;
    uint8_t rsv[2];
    uint8_t param[3][NPARAM];
} settings_t;
static settings_t set;
static uint8_t set_dirty;

/* ------------------------------------------------------------------ audio --- */
static float mix[APP_BLOCK];
static float gain_now;

static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xF0u, ch = (pkt >> 8) & 0x0Fu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        int on = st == 0x90u && d2;
        if (st != 0x90u && st != 0x80u)
            continue;
        if (ch == 0u)
            otter.note((int32_t)d1, d2, on);
        else if (ch == 1u)
            cat.note((int32_t)d1 - 36, d2, on);
        else if (ch == 2u)
            fox.note((int32_t)d1, d2, on);
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;
    midi_in();
    for (i = 0; i < n; i++) mix[i] = 0.0f;
    otter.render(mix, n);
    cat.render(mix, n);
    fox.render(mix, n);
    for (i = 0; i < n; i++) {
        float s;
        gain_now += (gain - gain_now) * 0.004f;
        s = fm_tanhf(mix[i] * 0.9f) * gain_now;     /* a soft ceiling: three engines never clip hard */
        lr[2 * i] = (int32_t)s;
        lr[2 * i + 1] = (int32_t)s;
    }
}

void app_panic(void) { otter.panic(); cat.panic(); fox.panic(); }

/* --------------------------------------------------------------- settings --- */
static void params_to_set(void)
{
    uint32_t e, p;
    for (e = 0; e < 3u; e++) for (p = 0; p < NPARAM; p++) set.param[e][p] = ENG[e]->param[p];
}
static void params_from_set(void)
{
    uint32_t e, p;
    for (e = 0; e < 3u; e++) for (p = 0; p < NPARAM; p++) ENG[e]->param[p] = set.param[e][p];
}

void app_init(void)
{
    otter.init(); cat.init(); fox.init();
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
        && set.octave >= -2 && set.octave <= 2 && set.focus < 3u) {
        params_from_set();
    } else {
        set.magic = SET_MAGIC;
        set.octave = 0;
        set.focus = E_OTTER;
        params_to_set();
    }
}

/* ----------------------------------------------------------------- backup --- */
int app_backup_playing(void) { return 0; }
void app_backup_flush(void) { params_to_set(); plat_store_save(OBJ_SETTINGS, &set, sizeof set); set_dirty = 0; }
void app_backup_hold(int on) { (void)on; }
void app_backup_restored(void) { app_init(); }

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    int32_t key_note[NKEYS];              /* what each held key started (so a release finds it after a focus change) */
    uint8_t key_eng[NKEYS];
    uint8_t redraw;
    int32_t last_note;
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
        text_cat(t, ENG[set.focus]->name);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* a key -> what the focused engine hears */
static int32_t key_to_note(uint32_t k, uint8_t e)
{
    static const uint8_t WK[16] = PLAT_WHITE_KEYS;
    uint32_t w;
    int32_t white = -1;
    for (w = 0; w < 16u; w++) if (WK[w] == k) white = (int32_t)w;
    if (e == E_CAT)
        return white;                                        /* pads 0..7 on white keys; black keys and w8+ ignored */
    if (e == E_FOX && fox_chop_mode())
        return white;                                        /* slices on the white keys */
    return 53 + (int32_t)k + 12 * set.octave;                /* key 0 is F3 (MIDI 53) */
}

static void focus(uint8_t e)
{
    if (e == set.focus) return;
    set.focus = e;
    set_dirty = 1;
    app_say(ENG[e]->name, "");
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k;
    int32_t e;
    const engine_t *cur = ENG[set.focus];
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
                cur->note(note, v, 1);
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
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0) continue;
        if (k >= EN_K1 && k < EN_K1 + NPARAM) {
            uint32_t p = k - EN_K1;
            char t[8];
            cur->param[p] = (uint8_t)clamp((int32_t)cur->param[p] + e * 3, 0, 127);
            set_dirty = 1;
            int_text(cur->param[p], t);
            app_say(cur->param_name[p], t);
        } else if (k == EN_SELECT) {
            focus((uint8_t)((set.focus + 3 + (e > 0 ? 1 : 2)) % 3u));
        }
    }
    if (keys != ui.keys || btn != ui.btn) ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    plat_leds((1u << (B_FX + set.focus)) | (set_dirty ? 1u << B_SAVE : 0u), keys);
}

void app_input_only(void) { input(); }

static void draw(void)
{
    char t[16];
    const engine_t *cur = ENG[set.focus];
    uint32_t i, now = plat_ms();
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
    uint16_t accent = ENG_RGB[set.focus];
    /* header: SFA + the three engines, the focused one lit; or a message */
    cv_begin(240, 36, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 8, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 8, &FONT_B, ui.msg[1], accent);
    } else {
        cv_text(10, 8, &FONT_B, APP_NAME, C_WHITE);
        for (i = 0; i < 3u; i++) {
            int32_t x = 84 + (int32_t)i * 52;
            int on = i == set.focus;
            cv_rect(x, 6, 48, 22, on ? ENG_RGB[i] : C_LINE);
            cv_text(x + (48 - text_w(&FONT_XS, ENG[i]->name)) / 2, 11, &FONT_XS, ENG[i]->name, on ? C_BLACK : C_GRAY);
        }
    }
    cv_blit(0, 0);
    /* the page: the engine's name big, four parameters as bars */
    cv_begin(240, 140, C_BLACK);
    cv_text(10, 4, &FONT_B, cur->name, accent);
    if (set.focus == E_OTTER || (set.focus == E_FOX && !fox_chop_mode())) {
        t[0] = set.octave < 0 ? '-' : '+'; t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave)); t[2] = 0;
        cv_text(190, 8, &FONT_XS, "OCT", C_GRAY);
        cv_text(214, 8, &FONT_XS, t, C_HI);
    } else if (set.focus == E_FOX) {
        cv_text(170, 8, &FONT_XS, "CHOP", C_HI);
    } else {
        cv_text(170, 8, &FONT_XS, "8 PADS", C_HI);
    }
    for (i = 0; i < NPARAM; i++) {
        int32_t x = 10 + (int32_t)i * 57, v = (int32_t)cur->param[i];
        int32_t h = (v * 70 + 63) / 127;
        cv_rect(x, 36, 50, 72, C_LINE);
        cv_rect(x + 1, 36 + 71 - h, 48, h, accent);
        cv_text(x + (50 - text_w(&FONT_XS, cur->param_name[i])) / 2, 112, &FONT_XS, cur->param_name[i], C_GRAY);
        int_text(set.focus == E_FOX && i == 2u ? fox_chops() : v, t);
        cv_text(x + (50 - text_w(&FONT_XS, t)) / 2, 124, &FONT_XS, t, C_HI);
    }
    cv_blit(0, 36);
    /* keys: held ones lit; Cat's pads numbered */
    cv_begin(240, 64, C_BLACK);
    for (i = 0; i < 16u; i++) {
        int held = ui.keys >> WK[i] & 1u;
        int pad = set.focus == E_CAT ? i < 8u : set.focus == E_FOX && fox_chop_mode() && i < (uint32_t)fox_chops();
        cv_rect(4 + (int32_t)i * 14, 16, 13, 30, held ? accent : pad ? C_GRAY : C_DIM);
        if (pad) { int_text((int32_t)i + 1, t); cv_text(7 + (int32_t)i * 14, 34, &FONT_XS, t, held ? C_BLACK : C_WHITE); }
    }
    for (i = 0; i < 11u; i++)
        cv_rect(4 + (int32_t)AT[i] * 14 + 8, 2, 11, 18, ui.keys >> BK[i] & 1u ? C_WHITE : C_LINE);
    cv_text(4, 50, &FONT_XS, "FX OTTER  SEL CAT  ENV FOX", C_GRAY);
    int_text((int32_t)(plat_master() * 100u / 4096u), t);
    text_cat(t, "%");
    cv_text(236 - text_w(&FONT_XS, t), 50, &FONT_XS, t, C_HI);
    cv_blit(0, 176);
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
    if (ui.redraw) { ui.redraw = 0; draw(); }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "focus")) *got = set.focus;
    else if (!strcmp(what, "octave")) *got = set.octave;
    else if (!strcmp(what, "dirty")) *got = set_dirty;
    else if (!strcmp(what, "note")) *got = ui.last_note;
    else if (!strncmp(what, "param", 5)) *got = ENG[set.focus]->param[atoi(what + 5) % NPARAM];
    else return 0;
    return 1;
}
#endif
