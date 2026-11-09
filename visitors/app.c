/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * VISITORS: four tracks rendered together, one in focus on the keys, the Ship sequencing them.
 * (Nordics = the poly synth, Mantis = synthesized drums, Reptilians = bass and lead, Greys = the sampler.)
 *   FX OTTER   SEL CAT   ENV FOX   LFO HOPPER   press: that track; again: next page; HOLD: mute / unmute
 *   SEQ SHIP (the pattern)   EDIT MIXER   GLO tap tempo   HOME stop   SAVE keep everything
 *   K1..K4 always the four things on the screen   PRESET knob: page   ALGO knob: preset / kit / 808 / pattern
 *   SELECT knob: track (on every page)
 *   PLAY run   REC arm the focused track (recording writes the armed tracks; hold REC 1 s: clear its steps)
 *   ARP SAMPLE: Hopper records the output until ARP again
 *   Ship page: white keys toggle the focused track's steps; HOLD a step + K1..K4 = its NOTE VEL LENGTH PROB;
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
const char *hop_source_name(char *buf); int hop_source(int32_t step); int hop_source_index(void); void hop_source_set(int s);
void otter_arp_set(int on); const char *otter_preset_name_at(int32_t off); const char *fox_preset_name_at(int32_t off); const char *greys_preset_name_at(int32_t off);
int hop_chop_mode(void); int hop_chops(void); int hop_chopmode(void); void hop_chopmode_set(int m); void hop_chop_on(int chops); int hop_recording(void); void hop_record_start(void); void hop_record_stop(void);
void hop_record_feed(const float *mix, uint32_t n); int hop_save_needed(void); int hop_save(void); uint32_t hop_bank_seconds_x10(void);
uint32_t hop_bank_len(void); int32_t hop_peak(uint32_t from, uint32_t to); uint32_t hop_window(uint32_t *a, uint32_t *b); float hop_playhead(void);
uint32_t hop_slice_points(const uint32_t **pts); uint8_t *hop_slice_blob(uint32_t *size); void hop_slices_default(void); int hop_last_slice(void);
void hop_sync(void); void hop_rechop(void); void hop_slice_nudge(uint32_t idx, int32_t delta); int hop_slice_active(uint32_t idx);

#define NENG 4
static const engine_t *const ENG[NENG] = {&otter, &cat, &fox, &hopper};
enum { E_OTTER, E_CAT, E_FOX, E_HOP };
enum { PG_ENGINE, PG_SEQ, PG_MIX, PG_HOME };

/* the palette: a dark neutral ground, light text, one bright colour per creature, red for recording */
#define C_BG      RGB(8, 8, 22)
#define C_PANEL   RGB(28, 24, 58)
#define C_RULE    RGB(62, 54, 110)
#define C_MUTED   RGB(110, 102, 150)
#define C_SUB     RGB(190, 186, 228)
#define C_TXT     RGB(246, 244, 255)
#define C_REC     RGB(255, 56, 110)
#define C_BRAND   RGB(255, 62, 240)
static const uint16_t ENG_RGB[5] = {RGB(255, 216, 106), RGB(56, 224, 192), RGB(88, 255, 122), RGB(200, 208, 232), RGB(192, 92, 255)};   /* NORDICS gold, MANTIS teal, REPTILIANS acid, GREYS silver, SHIP violet */
static const uint16_t ENG_DARK[5] = {RGB(110, 90, 30), RGB(20, 96, 84), RGB(30, 108, 48), RGB(84, 88, 108), RGB(84, 36, 120)};
#define C_SHIP ENG_RGB[4]

#include "icons.c"

#define SET_MAGIC 0x414C4933u            /* "ALI3" */
#define SONG_PAT(b) ((b) & 7u)
#define SONG_REP(b) (((b) >> 3 & 7u) + 1u)       /* a slot: bits 0-2 the pattern, 3-5 the repeats - 1 */
#define USR_MAGIC 0x414C4955u            /* "ALIU" */
#define NPAT 8
#define PATSZ 652
#define NSCENE 8
#define NSONG 16
typedef struct {
    uint32_t magic;
    int8_t octave;
    uint8_t focus;
    uint8_t page[NENG];
    uint8_t level[NENG], pan[NENG];
    uint8_t delay_mix, delay_time, verb, verb_size, drive, mixpage;
    uint8_t otter_preset, fox_preset, cat_kit, pattern;
    uint8_t chain, glue, lowend, air;     /* the master page */
    uint8_t song[NSONG], song_len, hop_source, rsv2[2];
    uint8_t send[NENG];                   /* each being's FX send (reverb + delay) */
    uint8_t song_scene[NSONG];            /* a scene to recall on entering the slot (0 = none, 1-8) */
    int8_t transpose[NENG];               /* live transpose per being, semitones (the Mantis ignores it) */   /* the song chain: pattern slots in order (CHAIN + key appends) */
    uint8_t param[NENG][NPAGES][NPARAM];
    uint8_t slices[16 * 4];
    struct { uint8_t used, pattern, otter, fox, kit, mute, level[4], pan[4], rsv[2]; } scene[NSCENE];   /* 16 B each */
    uint8_t pat[4][PATSZ];                /* patterns A-D (E-H: the user object) */
} settings_t;
typedef struct {
    uint32_t magic;
    uint8_t pads[8 * 16 * 9];             /* the kits' pads: 8 kits x 16 pads x 9 params (Mantis: + LAYER) */
    uint8_t pat[4][PATSZ];                /* patterns E-H */
} user_t;
static user_t usr;
static settings_t set;
static uint8_t set_dirty;
static uint8_t body;

#define PATN(n) ((pattern_t *)((n) < 4u ? set.pat[(n)] : usr.pat[(n) - 4u]))
#define PAT PATN(set.pattern)
static int32_t lane_transpose(uint8_t e) { return e == E_CAT ? 0 : set.transpose[e]; }
#include "seq.c"
static void scene_recall(uint32_t n);
static uint8_t chain_rep;                                            /* bars played of the current song slot */
static void chain_advance(void)
{
    if (set.song_len) {
        if (++chain_rep < SONG_REP(set.song[seq.chain_pos])) return;   /* the slot repeats */
        chain_rep = 0;
        seq.chain_pos = (uint8_t)((seq.chain_pos + 1u) % set.song_len);
        set.pattern = SONG_PAT(set.song[seq.chain_pos]);
        if (set.song_scene[seq.chain_pos]) scene_recall(set.song_scene[seq.chain_pos] - 1u);
    } else set.pattern = (uint8_t)((set.pattern + 1u) % NPAT);
}

/* ------------------------------------------------------------------ audio --- */
#define DLY_LEN 11025u
PLAT_POOL static int16_t dly[DLY_LEN];
/* the reverb bus: four combs and two allpasses at half rate (Schroeder), int16 in the pool, stereo from the comb signs */
static const uint32_t CL[4] = {779u, 907u, 1021u, 1117u}, AL[2] = {113u, 331u};
PLAT_POOL static int16_t comb[4][1117], apf[2][331];
static uint32_t comb_w[4], apf_w[2];
static float comb_lp[4], verb_l, verb_r;
static uint32_t dly_w;
static float dly_lp, dly_prev, dc_x[2], dc_y[2], lim_env = 1.0f;
static float low_lp[2], air_lp[2], glue_env, glue_g = 1.0f;
static float cgl[NENG], cgr[NENG], cgs[NENG];                      /* the gains as they are now (ramped) */
static uint8_t dly_tick;
static float mix[NENG][APP_BLOCK];
static float mixout[APP_BLOCK];
static int cpu2_rc = -9;
static float gain_now;
static uint32_t arp_phase;
static volatile float meter[NENG];       /* for the mixer's meters */

static void note_in(uint8_t e, int32_t note, uint32_t vel, int on)
{
    ENG[e]->note(note, vel, on);
    if (on) { seq_record(PAT, e, note, vel); if (seq.stepin && e == set.focus && !seq.playing) { seq_step_in(PAT, e, note, vel); set_dirty = 1; } }
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

static void render_one(uint8_t e, uint32_t n) { uint32_t i; for (i = 0; i < n; i++) mix[e][i] = 0.0f; ENG[e]->render(mix[e], n); }
static void render_synths(uint32_t n) { render_one(E_OTTER, n); render_one(E_FOX, n); }
static void render_samplers(uint32_t n) { render_one(E_CAT, n); render_one(E_HOP, n); }

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, e;
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;
    float gl[NENG], gr[NENG], gs[NENG], dgl[NENG], dgr[NENG], dgs[NENG], dmix = p01(set.delay_mix) * 0.8f, dfb = 0.45f;
    float vmix = p01(set.verb) * 0.7f, vfb = 0.72f + p01(set.verb_size) * 0.21f, vdamp = 0.25f + (1.0f - p01(set.verb_size)) * 0.35f;
    float mdrive = 1.0f + p01(set.drive) * p01(set.drive) * 6.0f, dnorm = 1.0f / fm_soft(mdrive * 0.7f);
    uint32_t dtime = 1100u + (uint32_t)(p01(set.delay_time) * (float)(DLY_LEN - 1200u));
    float low_c = 1.0f - fm_exp2f(-6.2831853f * 110.0f / SR), low_amt = p01(set.lowend) * 1.0f;      /* a low shelf at 110 Hz, up to +6 dB */
    float air_c = 1.0f - fm_exp2f(-6.2831853f * 5000.0f / SR), air_amt = p01(set.air) * 0.8f;       /* a high shelf from 5 kHz */
    float gthr = 1.0f - p01(set.glue) * 0.5f, gslope = p01(set.glue) * 0.45f, gmake = 1.0f + p01(set.glue) * 0.25f;
    midi_in();
    seq_tick(PAT, n);
    if (otter_arp_on()) {
        static const uint8_t NUM[4] = {2, 1, 4, 1}, DEN[4] = {1, 1, 3, 2};
        uint32_t r = (uint32_t)otter_arp_rate(), len = seq_step_samples(PAT) * NUM[r] / DEN[r];
        arp_phase += n;
        if (arp_phase >= len) { arp_phase -= len; otter_arp_tick(); }
    } else arp_phase = 0;
    /* the synths (Otter, Fox) on the second core when it runs, the samplers (Cat, Hopper) here */
    {
        int handed = plat_cpu2_run(render_synths, n);
        render_samplers(n);
        if (!handed || plat_cpu2_wait() != 0) render_synths(n);
        plat_cpu2_sync();
    }
    for (e = 0; e < NENG; e++) {
        float lvl = lane_audible(PAT, (uint8_t)e) ? p01(set.level[e]) : 0.0f, pan = p01(set.pan[e]), pk = 0.0f;
        gl[e] = lvl * fm_sinf((1.0f - pan) * 1.5707963f); gr[e] = lvl * fm_sinf(pan * 1.5707963f); gs[e] = lvl * p01(set.send[e]) * 0.8f;
        dgl[e] = (gl[e] - cgl[e]) / (float)n; dgr[e] = (gr[e] - cgr[e]) / (float)n; dgs[e] = (gs[e] - cgs[e]) / (float)n;   /* ramp over the block: no zipper on levels, pans, mutes */
        for (i = 0; i < n; i += 4) { float a = fm_fabsf(mix[e][i]) * lvl; if (a > pk) pk = a; }
        meter[e] = pk > meter[e] ? pk : meter[e] * 0.85f;
    }
    for (i = 0; i < n; i++) {
        float l = 0.0f, r = 0.0f, mono, d, y[2], a, pk;
        mono = 0.0f;
        for (e = 0; e < NENG; e++) { cgl[e] += dgl[e]; cgr[e] += dgr[e]; cgs[e] += dgs[e]; l += mix[e][i] * cgl[e]; r += mix[e][i] * cgr[e]; mono += mix[e][i] * cgs[e]; }
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
        if (set.verb && dly_tick) {                                      /* the reverb, every other sample */
            float in = mono * 0.5f, sum_l = 0.0f, sum_r = 0.0f, ap;
            uint32_t c;
            for (c = 0; c < 4u; c++) {
                float yv = (float)comb[c][comb_w[c]] * (1.0f / 32768.0f);
                comb_lp[c] += (yv - comb_lp[c]) * vdamp;
                { float wv = in + comb_lp[c] * vfb; if (wv > 1.0f) wv = 1.0f; else if (wv < -1.0f) wv = -1.0f; comb[c][comb_w[c]] = (int16_t)(wv * 32767.0f); }
                comb_w[c] = (comb_w[c] + 1u) % CL[c];
                if (c & 1u) sum_r += yv; else sum_l += yv;
            }
            ap = sum_l + sum_r;
            for (c = 0; c < 2u; c++) {                                   /* two allpasses in series on the sum */
                float bv = (float)apf[c][apf_w[c]] * (1.0f / 32768.0f), wv = ap + bv * 0.5f;
                if (wv > 1.0f) wv = 1.0f; else if (wv < -1.0f) wv = -1.0f;
                apf[c][apf_w[c]] = (int16_t)(wv * 32767.0f);
                apf_w[c] = (apf_w[c] + 1u) % AL[c];
                ap = bv - wv * 0.5f;
            }
            verb_l = ap * 0.6f + sum_l * 0.5f; verb_r = ap * 0.6f + sum_r * 0.5f;
        }
        if (set.verb) { l += verb_l * vmix; r += verb_r * vmix; }
        if (set.lowend) { low_lp[0] += (l - low_lp[0]) * low_c; low_lp[1] += (r - low_lp[1]) * low_c; l += low_lp[0] * low_amt; r += low_lp[1] * low_amt; }
        if (set.air) { air_lp[0] += (l - air_lp[0]) * air_c; air_lp[1] += (r - air_lp[1]) * air_c; l += (l - air_lp[0]) * air_amt; r += (r - air_lp[1]) * air_amt; }
        if (set.glue) {                                                  /* GLUE: a master compressor, 2 ms attack, 150 ms release */
            float a = fm_fabsf(l), b = fm_fabsf(r); if (b > a) a = b;
            glue_env += (a > glue_env ? 0.01f : 0.00008f) * (a - glue_env);
            if ((i & 7u) == 0u) glue_g = glue_env > gthr ? fm_powf(glue_env / gthr, -gslope) : 1.0f;
            l *= glue_g * gmake; r *= glue_g * gmake;
        }
        if (set.drive) { l = fm_soft(l * 0.7f * mdrive) * dnorm; r = fm_soft(r * 0.7f * mdrive) * dnorm; }
        y[0] = l - dc_x[0] + 0.995f * dc_y[0]; dc_x[0] = l; dc_y[0] = y[0];
        y[1] = r - dc_x[1] + 0.995f * dc_y[1]; dc_x[1] = r; dc_y[1] = y[1];
        a = fm_fabsf(y[0]); pk = fm_fabsf(y[1]); if (pk > a) a = pk;
        if (a > lim_env) lim_env += (a - lim_env) * 0.5f; else lim_env += (a - lim_env) * 0.00012f;   /* ~190 ms release: no pumping */
        if (lim_env > 0.95f) { float g = 0.95f / lim_env; y[0] *= g; y[1] *= g; }
        if (!(y[0] == y[0]) || !(y[1] == y[1])) {
            for (e = 0; e < NENG; e++) ENG[e]->init();
            dc_x[0] = dc_x[1] = dc_y[0] = dc_y[1] = dly_lp = dly_prev = 0.0f; lim_env = 1.0f; y[0] = y[1] = 0.0f;
        }
        mixout[i] = (y[0] + y[1]) * 0.5f;
        gain_now += (gain - gain_now) * 0.004f;
        lr[2 * i] = (int32_t)(fm_clip(y[0]) * 0.85f * gain_now);            /* the output: -1.4 dB of headroom under the clip */
        lr[2 * i + 1] = (int32_t)(fm_clip(y[1]) * 0.85f * gain_now);
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
    b = hop_slice_blob(&sz); for (p = 0; p < sz && p < sizeof set.slices; p++) set.slices[p] = b[p];
}
static void params_from_set(void)
{
    uint32_t e, g, p, sz;
    uint8_t *b;
    for (e = 0; e < NENG; e++) for (g = 0; g < ENG[e]->npages; g++) for (p = 0; p < NPARAM; p++) ENG[e]->params[g][p] = set.param[e][g][p];
    b = hop_slice_blob(&sz); for (p = 0; p < sz && p < sizeof set.slices; p++) b[p] = set.slices[p];
    seq.chain = set.chain;
}

void app_init(void)
{
    uint32_t e;
    _Static_assert(sizeof(pattern_t) <= PATSZ, "the pattern fits");
    _Static_assert(sizeof(user_t) <= 3840u, "the user object fits a store object");
    _Static_assert(sizeof(settings_t) <= 3900u, "the settings fit a store object");
    for (e = 0; e < NENG; e++) ENG[e]->init();
    cpu2_rc = plat_cpu2_set(1);                                        /* the second core renders the synths */
    plat_usb_audio_full(1);                                            /* USB audio at full level whatever MASTER does: sampling into a computer or an MPC */
    seq.rng = 0x1234567u;
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
        && set.octave >= -2 && set.octave <= 2 && set.focus < NENG && set.pattern < NPAT
        && PAT->bpm >= 40u && PAT->steps >= 1u && PAT->steps <= NSTEP) {
        for (e = 0; e < NENG; e++) if (set.page[e] >= ENG[e]->npages) set.page[e] = 0;
        otter_preset_set(set.otter_preset); fox_preset_set(set.fox_preset); cat_kit_set(set.cat_kit);
        hop_source_set(set.hop_source);
        params_from_set();
        fox_mode_set_params();
    } else {
        uint32_t k;
        set.magic = SET_MAGIC; set.octave = 0; set.focus = E_CAT;
        for (e = 0; e < NENG; e++) { set.page[e] = 0; set.level[e] = 92; set.pan[e] = 64; }
        set.level[E_CAT] = 100;
        set.delay_mix = 0; set.delay_time = 60; set.verb = 40; set.verb_size = 60; set.drive = 0; set.mixpage = 0;
        set.send[E_OTTER] = 70; set.send[E_CAT] = 10; set.send[E_FOX] = 0; set.send[E_HOP] = 80;
        set.glue = 22; set.lowend = 16; set.air = 12;
        set.otter_preset = 0; set.fox_preset = 0; set.cat_kit = 0; set.pattern = 0; set.chain = 0;
        cat_pads_default(); hop_slices_default();
        for (k = 0; k < 4u; k++) pattern_defaults((pattern_t *)set.pat[k]);
        params_to_set();
    }
    if (!(plat_store_load(OBJ_USER, &usr, sizeof usr) == (int)sizeof usr && usr.magic == USR_MAGIC)) {
        uint32_t k;
        uint32_t sz, p; const uint8_t *b;
        usr.magic = USR_MAGIC;
        cat_pads_default();
        b = cat_pad_blob(&sz); for (p = 0; p < sz && p < sizeof usr.pads; p++) usr.pads[p] = b[p];
        for (k = 0; k < 4u; k++) pattern_defaults((pattern_t *)usr.pat[k]);
    }
    { uint32_t sz, p; uint8_t *b = cat_pad_blob(&sz); for (p = 0; p < sz && p < sizeof usr.pads; p++) b[p] = usr.pads[p]; }
    cat.init(); hopper.init();
    seq_stop();
    body = PG_HOME;
}

/* ----------------------------------------------------------------- backup --- */
int app_backup_playing(void) { return seq.playing; }
static void user_to_usr(void) { uint32_t sz, p; const uint8_t *b = cat_pad_blob(&sz); for (p = 0; p < sz && p < sizeof usr.pads; p++) usr.pads[p] = b[p]; }
void app_backup_flush(void) { params_to_set(); user_to_usr(); plat_store_save(OBJ_SETTINGS, &set, sizeof set); plat_store_save(OBJ_USER, &usr, sizeof usr); set_dirty = 0; }
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
    int32_t held_step;                    /* the Ship: the step key held (-1 none) */
    uint8_t step_edited;                  /* a knob moved while it was held: no toggle on release */
    int32_t held_slice;                   /* the Hopper: the slice key held (-1 none) */
    uint8_t chain_held, chain_used;       /* the Ship's CHAIN key down; something was done while it was */
    uint8_t lock_arm, shippage;           /* a held step + the being's button: K1-K4 write parameter locks; the Ship's page */
    uint8_t steppage;                     /* the Ship's keys: steps 1-16 (0) or 17-32 (1), OCT- / OCT+ */
    uint8_t song_sel;                     /* the song page: the slot being edited */
    uint32_t fill_t; uint8_t fill_on;     /* SIGNAL held: after 350 ms the pattern becomes a fill until release */
    pattern_t fill_keep;
    uint8_t gen_held;                     /* the GEN key down: K1-K3 shape a Euclidean pattern live */
    uint32_t key_t[NKEYS];                /* when each key went down (the mixer's scene keys: hold = store) */
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

static const char *body_name(void) { return body == PG_HOME ? "HOME" : body == PG_SEQ ? "SHIP" : body == PG_MIX ? (set.mixpage == 0 ? "MIXER" : set.mixpage == 1 ? "PANS" : set.mixpage == 2 ? "FX" : set.mixpage == 3 ? "MASTER" : "SENDS") : ENG[set.focus]->name; }

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

static uint8_t scratch;
static int32_t held_engine(void)                                      /* which creature button is down (-1 none) */
{
    static const uint8_t B[4] = {B_FX, B_SEL, B_ENV, B_LFO};
    uint32_t i;
    for (i = 0; i < 4u; i++) if (ui.btn >> B[i] & 1u) return (int32_t)i;
    return -1;
}
static uint8_t *knob_target(uint32_t p, const char **name)
{
    static const char *const SEQN[4] = {"TEMPO", "SWING", "STEPS", "GATE"};
    static const char *const STEPN[4] = {"NOTE", "VEL", "LENGTH", "PROB"};
    static const char *const PADN[4] = {"PAD", "VEL", "LENGTH", "PROB"};
    static const char *const MIXN[4] = {"NORDIC", "MANTIS", "REPTIL", "GREYS"};
    static const char *const MIX2[4] = {"VERB", "SIZE", "DELAY", "DTIME"};
    static const char *const MIX3[4] = {"GLUE", "LOWEND", "AIR", "DRIVE"};
    static const char *const MIX4[4] = {"SEND NOR", "SEND MAN", "SEND REP", "SEND GRY"};
    static const char *const PANN[4] = {"PAN NOR", "PAN MAN", "PAN REP", "PAN GRY"};
    {                                                                   /* a creature's button held: its LEVEL / PAN, from any page */
        int32_t h = held_engine();
        if (h >= 0) { *name = p == 0 ? "LEVEL" : p == 1 ? "PAN" : "-"; return p == 0 ? &set.level[h] : p == 1 ? &set.pan[h] : &scratch; }
    }
    if (body == PG_SEQ) {
        static const char *const LOCKN[4] = {"LOCK K1", "LOCK K2", "LOCK K3", "LOCK K4"};
        static const char *const LENN[4] = {"LEN NOR", "LEN MAN", "LEN REP", "LEN GRY"};
        if (ui.held_step >= 0 && ui.lock_arm) { *name = LOCKN[p]; return &scratch; }
        if (ui.held_step >= 0) { *name = (set.focus == E_CAT ? PADN : STEPN)[p]; return seq_step_param(PAT, set.focus, (uint32_t)ui.held_step, p); }
        if (ui.shippage == 3u) { static const char *const SGN[4] = {"SLOT", "PATTERN", "REPEAT", "SCENE"}; *name = SGN[p]; return &scratch; }
        if (ui.shippage == 2u) { static const char *const TRN[4] = {"TR NOR", "TR MAN", "TR REP", "TR GRY"}; *name = TRN[p]; return (uint8_t *)&set.transpose[p]; }
        if (ui.shippage) { *name = LENN[p]; return &PAT->llen[p]; }
        *name = SEQN[p]; return p == 0 ? &PAT->bpm : p == 1 ? &PAT->swing : p == 2 ? &PAT->steps : &PAT->gate;
    }
    if (body == PG_HOME) { *name = MIXN[p]; return &set.level[p]; }
    if (body == PG_MIX) {
        if (set.mixpage == 1u) { *name = PANN[p]; return &set.pan[p]; }
        if (set.mixpage == 2u) { *name = MIX2[p]; return p == 0 ? &set.verb : p == 1 ? &set.verb_size : p == 2 ? &set.delay_mix : &set.delay_time; }
        if (set.mixpage == 3u) { *name = MIX3[p]; return p == 0 ? &set.glue : p == 1 ? &set.lowend : p == 2 ? &set.air : &set.drive; }
        if (set.mixpage == 4u) { *name = MIX4[p]; return &set.send[p]; }
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
    {
        int32_t h = held_engine();
        if (h >= 0) {                                                       /* hold a creature + K1/K2: its level / pan (no mute then) */
            static const uint8_t B[4] = {B_FX, B_SEL, B_ENV, B_LFO};
            ui.btn_long[B[h]] = 2;
            if (p > 1) return;
            *v = (uint8_t)clamp((int32_t)*v + e * (p == 0 ? 3 : 2), 0, 127);
            if (p == 1) { if (*v == 64u) { t[0] = 'C'; t[1] = 0; } else { t[0] = *v < 64u ? 'L' : 'R'; int_text(*v < 64u ? 64 - (int32_t)*v : (int32_t)*v - 64, t + 1); } }
            else int_text(*v, t);
            app_say(ENG[h]->name, t); set_dirty = 1; ui.redraw = 1; return;
        }
    }
    if (body == PG_ENGINE && set.focus == E_HOP && ui.held_slice > 0 && p == 0) {   /* MANUAL chop: move the held slice's start */
        hop_slice_nudge((uint32_t)ui.held_slice, e * 440);
        app_say("SLICE START", e > 0 ? ">" : "<");
        set_dirty = 1; ui.redraw = 1; return;
    }
    if (body == PG_SEQ && ui.gen_held && p < 3u) {                      /* GEN held + K1 pulses, K2 steps, K3 rotation: Euclid, live */
        static uint8_t eu_pulses = 4, eu_steps = 16, eu_rot = 0;
        if (p == 0) eu_pulses = (uint8_t)clamp((int32_t)eu_pulses + e, 1, 32);
        else if (p == 1) eu_steps = (uint8_t)clamp((int32_t)eu_steps + e, 2, 32);
        else eu_rot = (uint8_t)(((int32_t)eu_rot + e + 32) % 32);
        if (eu_pulses > eu_steps) eu_pulses = eu_steps;
        seq_euclid(PAT, set.focus, eu_pulses, eu_steps, eu_rot, ui.last_note, cat_last_pad());
        ui.step_edited = 1; set_dirty = 1; ui.redraw = 1;
        { char et[12]; int_text(eu_pulses, et); text_cat(et, "/"); int_text(eu_steps, et + (eu_pulses > 9 ? 3 : 2)); app_say("EUCLID", et); }
        return;
    }
    if (body == PG_SEQ && ui.held_step >= 0 && ui.lock_arm && set.focus != E_CAT) {   /* a parameter lock on this step */
        lane_t *l = &PAT->lane[set.focus];
        uint8_t lk = LLOCK(l, ui.held_step), cur = (lk & 0x80u) && (lk >> 5 & 3u) == p ? (uint8_t)((lk & 31u) * 4u + 2u) : ENG[set.focus]->params[0][p];
        int32_t nv = clamp((int32_t)cur + e * 4, 0, 127);
        LLOCK(l, ui.held_step) = (uint8_t)(0x80u | p << 5 | (uint32_t)nv >> 2);
        ui.step_edited = 1; set_dirty = 1;
        int_text(nv, t); app_say(page_names(set.focus, 0)[p], t);
        return;
    }
    if (body == PG_SEQ && ui.held_step >= 0) {
        ui.step_edited = 1;
        if (p == 0) {                                                       /* NOTE / PAD: one per detent */
            if (set.focus == E_CAT) { uint16_t *m = &LMASK(&PAT->lane[E_CAT], ui.held_step); int32_t pd = cat_last_pad(); pd = clamp(pd + e, 0, 15); *m = (uint16_t)(1u << pd); int_text(pd + 1, t); }
            else { int32_t nv = lane_rest(set.focus, *v) ? 48 : (int32_t)*v; *v = (uint8_t)clamp(nv + e, 1, 119); int_text(*v, t); }
        } else if (p == 2) { uint8_t r = (uint8_t)(*v & 0xE0u); *v = (uint8_t)(clamp((int32_t)STEP_LEN(*v) + e, 0, 16) | r); if (STEP_LEN(*v)) { int_text((int32_t)STEP_LEN(*v), t); text_cat(t, "/4"); } else { t[0] = 0; text_cat(t, "GATE"); } }
        else { *v = (uint8_t)clamp((int32_t)*v + e * 4, 0, 127); if (p == 3 && *v >= 127u) { t[0] = 0; text_cat(t, "ALWAYS"); } else int_text(*v, t); }
        set_dirty = 1; app_say(name, t); return;
    }
    if (body == PG_SEQ && ui.shippage == 3u) {                            /* the song builder: slot, its pattern, repeats, scene */
        uint32_t sl = ui.song_sel; uint8_t *b = &set.song[sl];
        if (p == 0) { ui.song_sel = (uint8_t)clamp((int32_t)sl + e, 0, NSONG - 1); int_text((int32_t)ui.song_sel + 1, t); app_say("SLOT", t); ui.redraw = 1; return; }
        if (sl >= set.song_len) {
            uint32_t k; for (k = set.song_len; k <= sl; k++) { set.song[k] = (uint8_t)(k ? SONG_PAT(set.song[k - 1]) : 0); set.song_scene[k] = 0; }
            set.song_len = (uint8_t)(sl + 1u);
        }
        if (p == 1) { *b = (uint8_t)((*b & ~7u) | (uint32_t)clamp((int32_t)SONG_PAT(*b) + e, 0, NPAT - 1)); t[0] = (char)('A' + SONG_PAT(*b)); t[1] = 0; app_say("PATTERN", t); }
        else if (p == 2) {
            int32_t rep = (int32_t)SONG_REP(*b) + e;
            if (rep < 1) { set.song_len = (uint8_t)sl; app_say("SONG END", "HERE"); if (!set.song_len) seq.chain_pos = 0; }
            else { if (rep > 8) rep = 8; *b = (uint8_t)(SONG_PAT(*b) | (uint32_t)(rep - 1) << 3); int_text(rep, t); text_cat(t, "X"); app_say("REPEAT", t); }
        }
        else { set.song_scene[sl] = (uint8_t)clamp((int32_t)set.song_scene[sl] + e, 0, NSCENE); if (set.song_scene[sl]) { int_text(set.song_scene[sl], t); app_say("SCENE", t); } else app_say("SCENE", "NONE"); }
        set_dirty = 1; ui.redraw = 1; return;
    }
    if (body == PG_SEQ && ui.shippage == 2u) {                            /* transpose: -24..+24 semitones, live */
        int8_t *tr = (int8_t *)v; if (p == E_CAT) { app_say(name, "-"); return; }
        *tr = (int8_t)clamp((int32_t)*tr + e, -24, 24); set_dirty = 1;
        t[0] = *tr < 0 ? '-' : '+'; int_text(*tr < 0 ? -*tr : *tr, t + 1); app_say(name, t); return;
    }
    if (body == PG_SEQ && ui.shippage) { *v = (uint8_t)clamp((int32_t)*v + e, 0, NSTEP); if (!*v) { set_dirty = 1; app_say(name, "PATTERN"); return; } }
    else if (body == PG_SEQ && p == 0) *v = (uint8_t)clamp((int32_t)*v + e, 40, 240);
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

static void gen_all(void)
{
    uint32_t e;
    for (e = 0; e < NENG; e++) seq_gen(PAT, (uint8_t)e, e == E_FOX ? 36 : 60, hop_chops());
    PAT->mute = 0; set_dirty = 1;
    if (!seq.playing) seq_start();
    app_say("NEW SIGNAL", "A-H: PATTERNS");
}

static void song_say(void)
{
    char t[18]; uint32_t i;
    for (i = 0; i < set.song_len && i < 8u; i++) { t[2 * i] = (char)('A' + (set.song[i] & 7u)); t[2 * i + 1] = ' '; }
    if (set.song_len > 8u) { t[16] = '.'; t[17] = 0; } else
    t[2 * i] = 0;
    app_say("SONG", set.song_len ? t : "A>B>C>D");
}
/* scenes: a snapshot of the sound and the mix (pattern, presets, kit, levels, pans, mutes) on the mixer's keys */
static int scene_key(int32_t w) { return w >= 4 && w < 8 ? w - 4 : w >= 12 && w < 16 ? w - 8 : -1; }
static void scene_store(uint32_t n)
{
    uint32_t e;
    set.scene[n].used = 1; set.scene[n].pattern = set.pattern;
    set.scene[n].otter = (uint8_t)otter_preset_index(); set.scene[n].fox = (uint8_t)fox_preset_index(); set.scene[n].kit = (uint8_t)cat_kit_index();
    set.scene[n].mute = PAT->mute;
    for (e = 0; e < NENG; e++) { set.scene[n].level[e] = set.level[e]; set.scene[n].pan[e] = set.pan[e]; }
    set_dirty = 1; ui.redraw = 1;
    { char t[3] = {(char)('1' + n), 0, 0}; app_say("SCENE STORED", t); }
}
static void scene_recall(uint32_t n)
{
    uint32_t e; char t[3] = {(char)('1' + n), 0, 0};
    if (!set.scene[n].used) { app_say("SCENE EMPTY", "HOLD TO STORE"); return; }
    set.pattern = set.scene[n].pattern & 7u;
    otter_preset((int32_t)set.scene[n].otter - otter_preset_index());
    fox_preset((int32_t)set.scene[n].fox - fox_preset_index());
    cat_kit_set(set.scene[n].kit);
    PAT->mute = set.scene[n].mute;
    for (e = 0; e < NENG; e++) { set.level[e] = set.scene[n].level[e]; set.pan[e] = set.scene[n].pan[e]; }
    set_dirty = 1; ui.redraw = 1;
    app_say("SCENE", t);
}

static void beaver_black(int32_t b)
{
    char t[4] = {'A', 0};
    switch (b) {
    case 0: pattern_copy(PAT); app_say("COPIED", ""); break;
    case 1: if (pattern_paste(PAT)) { set_dirty = 1; app_say("PASTED", ""); } else app_say("NOTHING", "COPIED"); break;
    case 2: seq_clear_lane(PAT, set.focus); set_dirty = 1; app_say(ENG[set.focus]->name, "CLEARED"); break;
    case 3: seq.chain = (uint8_t)!seq.chain; seq.chain_pos = 0; chain_rep = 0; set_dirty = 1; if (seq.chain) { if (set.song_len) { set.pattern = SONG_PAT(set.song[0]); if (set.song_scene[0]) scene_recall(set.song_scene[0] - 1u); } song_say(); } else app_say("CHAIN", "OFF"); break;
    case 4: break;                                              /* SIGNAL: handled on the key's release */
    case 5: case 6: case 7: case 8: mute_toggle((uint8_t)(b - 5)); break;
    case 9: seq_gen(PAT, set.focus, ui.last_note, hop_chops()); set_dirty = 1; app_say(ENG[set.focus]->name, "NEW GROOVE"); break;
    case 10: solo_toggle(set.focus); break;
    default: t[0] = (char)('A' + set.pattern); app_say("PATTERN", t); break;
    }
    ui.redraw = 1;
}

static void engine_button(uint32_t k, uint8_t e, uint32_t btn, uint32_t now)
{
    uint32_t bit = 1u << k;
    if ((btn & bit) && !(ui.btn & bit)) { ui.btn_t[k] = now; ui.btn_long[k] = 0; }
    else if ((btn & bit) && ui.btn_t[k] && !ui.btn_long[k] && now - ui.btn_t[k] >= 600u) { ui.btn_long[k] = 1; }   /* a long hold: no page flip on release (hold + K1/K2 = level/pan) */
    else if (!(btn & bit) && (ui.btn & bit)) { if (!ui.btn_long[k] && !(body == PG_SEQ && ui.held_step >= 0 && e == set.focus)) focus(e); ui.btn_t[k] = 0; }
    if ((btn & bit) && !(ui.btn & bit) && body == PG_SEQ && ui.held_step >= 0 && e == set.focus && e != E_CAT) { ui.lock_arm = 1; ui.step_edited = 1; app_say("LOCK", "TURN K1-K4"); }
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
            if (body == PG_SEQ) {                                      /* the Ship's keys */
                if (ui.held_step >= 0 && b == 9) {                     /* a held step + GEN: its ratchet x1 x2 x3 x4 */
                    uint8_t *lv = seq_step_param(PAT, set.focus, (uint32_t)ui.held_step, 2);
                    uint32_t r = (STEP_RAT(*lv) + 1u) & 3u;
                    char rt[3] = {'x', (char)('1' + r), 0};
                    *lv = (uint8_t)(STEP_LEN(*lv) | r << 5); ui.step_edited = 1; set_dirty = 1;
                    app_say("RATCHET", rt);
                } else if (ui.chain_held && w >= 0 && w < NPAT) {      /* CHAIN held + key 1-8: append to the song */
                    if (set.song_len < NSONG) { set.song[set.song_len++] = (uint8_t)w; set_dirty = 1; }
                    ui.chain_used = 1; song_say();
                } else if (ui.chain_held && b == 2) {                  /* CHAIN held + CLR: an empty song */
                    set.song_len = 0; seq.chain_pos = 0; ui.chain_used = 1; set_dirty = 1; app_say("SONG", "CLEARED");
                } else if (b == 3) { ui.chain_held = 1; ui.chain_used = 0; }
                else if (b == 9) { ui.gen_held = 1; ui.step_edited = 0; }  /* GEN down: tap = a groove, GEN + K1-K3 = Euclid */
                else if (b == 4) { ui.fill_t = now ? now : 1u; }           /* SIGNAL down: see the release */
                else if (w >= 0 && ui.shippage == 3u) { ui.song_sel = (uint8_t)w; ui.redraw = 1; }
                else if (w >= 0) { ui.held_step = w + (int32_t)ui.steppage * 16; ui.step_edited = 0; }
                else if (b >= 0) beaver_black(b);
                ui.redraw = 1;
                continue;
            }
            if (body == PG_HOME) {                                     /* home: keys 1-5 jump to a creature */
                if (w >= 0 && w < 4) focus((uint8_t)w);
                else if (w == 4) { body = PG_SEQ; ui.redraw = 1; app_say("SHIP", ""); }
                else if (w == 5) { body = PG_MIX; ui.redraw = 1; app_say("MIXER", ""); }
                else if (w == 6) { gen_all(); ui.redraw = 1; }
                continue;
            }
            if (body == PG_MIX) {                                      /* the mixer's keys: mute 1-4, solo 9-12, scenes 5-8 / 13-16 */
                if (w >= 0 && w < 4) mute_toggle((uint8_t)w);
                else if (w >= 8 && w < 12) solo_toggle((uint8_t)(w - 8));
                else if (scene_key(w) >= 0) ui.key_t[k] = now ? now : 1u;
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
            int32_t note = ui.key_note[k], w = white_index(k), b = black_index(k);
            if (body == PG_SEQ && b == 3 && ui.chain_held) { ui.chain_held = 0; if (!ui.chain_used) beaver_black(3); ui.redraw = 1; }
            if (body == PG_MIX && scene_key(w) >= 0 && ui.key_t[k]) {  /* a scene key up: a hold stored it, a tap recalls it */
                if (now - ui.key_t[k] >= 700u) scene_store((uint32_t)scene_key(w)); else scene_recall((uint32_t)scene_key(w));
                ui.key_t[k] = 0;
            }
            if (body == PG_SEQ && b == 4 && ui.fill_t) {                /* SIGNAL up: a tap = a new signal; a hold was a fill, now undone */
                if (ui.fill_on) { pattern_t *pp = PAT; *pp = ui.fill_keep; ui.fill_on = 0; app_say("FILL", "BACK"); }
                else if (now - ui.fill_t < 350u) gen_all();
                ui.fill_t = 0; ui.redraw = 1;
            }
            if (body == PG_SEQ && b == 9 && ui.gen_held) {             /* GEN up: a tap makes a groove, a hold with knobs already did */
                ui.gen_held = 0;
                if (!ui.step_edited) beaver_black(9);
                ui.redraw = 1;
            }
            if (body == PG_SEQ && w >= 0 && w + (int32_t)ui.steppage * 16 == ui.held_step) ui.lock_arm = 0;
            if (body == PG_SEQ && w >= 0 && w + (int32_t)ui.steppage * 16 == ui.held_step) {
                if (!ui.step_edited) { seq_toggle(PAT, set.focus, (uint32_t)ui.held_step, ui.last_note, cat_last_pad(), hop_last_slice()); set_dirty = 1; }
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
        else if (k == B_PLAY) { if (seq.playing) seq_stop(); else { seq.stepin = 0; seq_start(); } ui.redraw = 1; app_say(seq.playing ? "PLAY" : "STOP", ""); }
        else if (k == B_REC) ui.btn_t[B_REC] = now;
        else if (k == B_ARP) {
            if (1) { int on = !otter_arp_on(); otter_arp_set(on); app_say("NORDICS ARP", on ? "ON" : "OFF"); ui.redraw = 1; }
            else if (hop_recording()) {
                char ct[8];
                hop_record_stop();
                hop_chopmode_set(1); if (!hop_chop_mode()) hop_chop_on(8); hop_rechop();
                set.page[E_HOP] = 3; set.focus = E_HOP; body = PG_ENGINE; ui.redraw = 1;
                int_text(hop_chops(), ct); app_say("SAMPLED", ct); set_dirty = 1;
            }
            else { hop_record_start(); body = PG_ENGINE; set.focus = E_HOP; app_say("SAMPLING", "ARP STOPS"); }
            ui.redraw = 1;
        }
        else if (k == B_HOME) { body = body == PG_HOME ? PG_ENGINE : PG_HOME; ui.held_step = -1; ui.redraw = 1; app_say(body_name(), ""); }
        else if ((k == B_OCTDN || k == B_OCTUP) && body == PG_SEQ && !(seq.stepin && !seq.playing)) {
            ui.steppage = (uint8_t)(k == B_OCTUP); ui.redraw = 1; app_say("STEPS", ui.steppage ? "17-32" : "1-16");
            if (ui.steppage && PAT->steps <= 16u) { PAT->steps = 32; set_dirty = 1; }
        }
        else if ((k == B_OCTDN || k == B_OCTUP) && seq.stepin && !seq.playing) {
            int32_t n = PAT->steps ? PAT->steps : 1;
            seq.cursor = (seq.cursor + (k == B_OCTUP ? 1 : n - 1)) % n;
            char ct[8]; int_text(seq.cursor + 1, ct); app_say(k == B_OCTUP ? "REST" : "BACK", ct); ui.redraw = 1;
        }
        else if (k == B_OCTDN || k == B_OCTUP) {
            int32_t o = clamp(set.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
            char t[4] = {o < 0 ? '-' : '+', (char)('0' + (o < 0 ? -o : o)), 0};
            if (o != set.octave) { otter.panic(); fox.panic(); set.octave = (int8_t)o; set_dirty = 1; app_say("OCTAVE", t); }
        } else if (k == B_SAVE) {
            int rc;
            params_to_set();
            rc = plat_store_save(OBJ_SETTINGS, &set, sizeof set);
            if (rc == 0) { user_to_usr(); rc = plat_store_save(OBJ_USER, &usr, sizeof usr); }
            if (rc == 0 && hop_save_needed()) { app_panic(); rc = hop_save(); }
            if (rc == 0) { set_dirty = 0; app_say("SAVED", ""); } else app_say("SAVE", "FAILED");
        }
    }
    if (ui.fill_t && !ui.fill_on && now - ui.fill_t >= 350u) {           /* a held SIGNAL: a fill on every being, kept until release */
        uint32_t e; ui.fill_keep = *PAT; ui.fill_on = 1;
        for (e = 0; e < NENG; e++) seq_fill(PAT, (uint8_t)e);
        app_say("FILL", "HOLD"); ui.redraw = 1;
    }
    if (ui.btn_t[B_REC]) {                                             /* REC: a press arms the focused track; a hold clears it */
        if (btn >> B_REC & 1u) {
            if (now - ui.btn_t[B_REC] >= 1000u) { ui.btn_t[B_REC] = 0; seq_clear_lane(PAT, set.focus); set_dirty = 1; app_say(ENG[set.focus]->name, "CLEARED"); }
        } else {
            ui.btn_t[B_REC] = 0;
            if (!seq.playing) {                                        /* stopped: REC = step entry on the focused track */
                seq.stepin = (uint8_t)!seq.stepin;
                seq.cursor = 0;
                if (seq.stepin) seq.armed |= (uint8_t)(1u << set.focus);
                seq.rec = seq.armed != 0u;
                app_say(ENG[set.focus]->name, seq.stepin ? "STEP ENTRY" : "STEP OFF");
            } else {
                seq.armed ^= (uint8_t)(1u << set.focus);
                seq.rec = seq.armed != 0u;
                app_say(ENG[set.focus]->name, seq.armed >> set.focus & 1u ? "ARMED" : "DISARMED");
            }
            ui.redraw = 1;
        }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0) continue;
        if (k >= EN_K1 && k < EN_K1 + NPARAM) knob(k - EN_K1, e);
        else if (k == EN_PRESET) {
            if (body == PG_MIX) { set.mixpage = (uint8_t)((set.mixpage + 5u + (e > 0 ? 1u : 4u)) % 5u); ui.redraw = 1; app_say(body_name(), ""); }
            else if (body == PG_ENGINE) page_set((uint8_t)(set.page[set.focus] + (e > 0 ? 1u : ENG[set.focus]->npages - 1u)));
            else if (body == PG_SEQ) { ui.shippage = (uint8_t)((ui.shippage + (e > 0 ? 1u : 3u)) % 4u); ui.redraw = 1; app_say("SHIP", ui.shippage == 1u ? "LANE LENGTHS" : ui.shippage == 2u ? "TRANSPOSE" : ui.shippage == 3u ? "SONG" : "PATTERN"); }
        } else if (k == EN_ALGO) {
            if (body == PG_SEQ) {
                char t[2] = {'A', 0};
                set.pattern = (uint8_t)(((int32_t)set.pattern + e % NPAT + NPAT) % NPAT);
                t[0] = (char)('A' + set.pattern);
                if (seq.playing) { seq_stop(); seq_start(); }
                app_say("PATTERN", t);
            } else {
                body = PG_ENGINE;
                if (set.focus == E_OTTER) { otter.panic(); otter_preset(e); app_say("PRESET", otter_preset_name()); }
                else if (set.focus == E_CAT) { cat_kit(e); app_say("KIT", cat_kit_name()); }
                else if (set.focus == E_FOX) { fox.panic(); fox_preset(e); app_say(fox_mode() ? "SYNTH" : "REPTILE", fox_preset_name()); }
                else {                                                 /* Hopper: the source; a sample lands chopped by transients */
                    char sb[14]; hopper.panic(); hop_source(e); set.hop_source = (uint8_t)hop_source_index();
                    app_say("GREYS", hop_source_name(sb));
                }
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
/* a panel with depth: a rule, a vertical gradient (lighter up top), a one-pixel highlight, rounded corners */
static uint16_t shade(uint16_t c, int32_t amt) { return rgb_mix(c, amt > 0 ? 255 : 0, amt > 0 ? 255 : 0, amt > 0 ? 255 : 0, amt > 0 ? amt : -amt); }
static void ui_panel(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t r;
    cv_rect(x + 1, y, w - 2, h, C_RULE); cv_rect(x, y + 1, w, h - 2, C_RULE);
    for (r = 1; r < h - 1; r++) cv_rect(x + 1, y + r, w - 2, 1, rgb_mix(C_PANEL, 0, 0, 0, r * 110 / h));
    cv_rect(x + 2, y + 1, w - 4, 1, shade(C_PANEL, 40));
}
/* a value fill with a gradient and a bright cap */
static void ui_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t col)
{
    int32_t r;
    if (h <= 0) return;
    for (r = 0; r < h; r++) cv_rect(x, y + r, w, 1, rgb_mix(col, 0, 0, 0, r * 120 / (h + 8)));
    cv_rect(x, y, w, 1, shade(col, 110));
}

static void bar_h(int32_t x, int32_t y, int32_t bh, const char *name, int32_t v, int32_t lo, int32_t hi, uint16_t col, const char *txt)
{
    int32_t h = (v - lo) * (bh - 4) / (hi - lo);
    char t[8];
    ui_panel(x, y, 52, bh);
    { int32_t k; for (k = 1; k < 4; k++) cv_rect(x + 2, y + 2 + (bh - 4) * k / 4, 48, 1, shade(C_PANEL, 18)); }   /* quarter ticks */
    {                                                                  /* a stack of 10 segments, the top one bright */
        int32_t seg = (bh - 4) / 10, k, lit = h ? (h + seg - 1) / seg : 0;
        if (seg < 3) { ui_fill(x + 2, y + bh - 2 - h, 48, h, col); seg = 0; }      /* a short bar: solid */
        for (k = 0; seg && k < 10; k++) {
            int32_t sy = y + bh - 2 - (k + 1) * seg;
            if (k < lit) ui_fill(x + 3, sy + 1, 46, seg - 1, k == lit - 1 ? shade(col, 60) : rgb_mix(col, 0, 0, 0, 40 - k * 4));
        }
    }
    cv_text(x + (52 - text_w(&FONT_XS, name)) / 2, y + bh + 3, &FONT_XS, name, C_SUB);
    if (!txt) { int_text(v, t); txt = t; }
    cv_text(x + (52 - text_w(&FONT_XS, txt)) / 2, y + bh + 15, &FONT_XS, txt, C_TXT);
}
#define BX(i) (8 + (int32_t)(i) * 57)
/* Alien Stack's parameter rows: the name, a horizontal stack of 12 segments, the value; four rows fill the page */
#define ROW_Y(i) (30 + (int32_t)(i) * 18)
/* a string cut to fit a width (the tiles' preset names) */
static const char *fit_text(const char *s, const felucca_font_t *f, int32_t w, char *buf)
{
    uint32_t n = 0;
    while (s[n] && n < 13u) { buf[n] = s[n]; buf[n + 1] = 0; if (text_w(f, buf) > w) { buf[n] = 0; break; } n++; }
    return buf;
}
static void rowbar(int32_t i, const char *name, int32_t v, int32_t lo, int32_t hi, uint16_t col, const char *txt, int32_t meter_px)
{
    int32_t y = ROW_Y(i), k, lit = (v - lo) * 12 / (hi - lo), x0 = 74, segw = 9;
    char t[8];
    if (lit > 12) lit = 12;
    if (lit < 0) lit = 0;
    cv_rect(0, y, 240, 18, i & 1 ? rgb_mix(C_BG, 255, 255, 255, 6) : C_BG);
    cv_text(8, y + 2, &FONT_XS, name, C_SUB);
    for (k = 0; k < 12; k++) {
        int32_t sx = x0 + k * segw;
        if (k < lit) ui_fill(sx, y + 4, segw - 2, 9, k == lit - 1 ? shade(col, 60) : rgb_mix(col, 0, 0, 0, 36 - k * 3));
        else cv_rect(sx, y + 4, segw - 2, 9, C_PANEL);
    }
    if (meter_px >= 0) cv_rect(x0, y + 14, meter_px > 106 ? 106 : meter_px, 2, C_TXT);      /* a meter under the stack */
    if (!txt) { int_text(v, t); txt = t; }
    cv_text(232 - text_w(&FONT_XS, txt), y + 2, &FONT_XS, txt, C_TXT);
}
static void rowpan(int32_t i, const char *name, int32_t v, uint16_t col)
{
    int32_t y = ROW_Y(i), c = 74 + 53, p = (v - 64) * 52 / 64;
    char t[8];
    cv_rect(0, y, 240, 18, i & 1 ? rgb_mix(C_BG, 255, 255, 255, 6) : C_BG);
    cv_text(8, y + 2, &FONT_XS, name, C_SUB);
    cv_rect(74, y + 4, 106, 9, C_PANEL);
    cv_rect(c, y + 4, 1, 9, C_MUTED);
    if (p >= 0) ui_fill(c, y + 4, p + 1, 9, col); else ui_fill(c + p, y + 4, -p + 1, 9, col);
    if (v == 64) { t[0] = 'C'; t[1] = 0; } else { t[0] = v < 64 ? 'L' : 'R'; int_text(v < 64 ? 64 - v : v - 64, t + 1); }
    cv_text(232 - text_w(&FONT_XS, t), y + 2, &FONT_XS, t, C_TXT);
}
#define bar(x, y, name, v, lo, hi, col, txt) rowbar(((x) - 8) / 57, name, v, lo, hi, col, txt, -1)

static void pan_bar(int32_t x, int32_t y, const char *name, int32_t v, uint16_t col)
{
    char t[8];
    int32_t c = x + 26, p = (v - 64) * 23 / 64;
    ui_panel(x, y, 52, 56);
    cv_rect(c, y + 4, 1, 48, C_MUTED);
    if (p >= 0) ui_fill(c, y + 20, p + 1, 16, col); else ui_fill(c + p, y + 20, -p + 1, 16, col);
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
    uint16_t accent = body == PG_SEQ ? C_SHIP : body == PG_MIX ? C_TXT : ENG_RGB[set.focus];
    int32_t st = seq.step;

    /* header: ECB, the five creatures as tabs with their track state, pattern, tempo, transport */
    cv_begin(240, 36, C_BG);
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 9, &FONT_B, ui.msg[0], C_TXT);
        cv_text(x + 8, 9, &FONT_B, ui.msg[1], accent);
    } else {
        const char *tr = seq.stepin && !seq.playing ? "STEP" : seq.rec ? "REC" : seq.playing ? "PLAY" : "STOP";
        cv_text(4, 3, &FONT_T, "ALIEN", C_BRAND); cv_text(4, 14, &FONT_T, "STACK", C_SUB);
        for (i = 0; i < 5u; i++) {
            int32_t x = 38 + (int32_t)i * 27;
            int on = i < NENG ? body == PG_ENGINE && i == set.focus : body == PG_SEQ;
            int muted = i < NENG && !lane_audible(PAT, (uint8_t)i), armed = i < NENG && (seq.armed >> i & 1u);
            cv_rect(x, 3, 25, 30, on ? ENG_DARK[i] : C_PANEL);
            if (on) cv_rect(x, 3, 25, 1, ENG_RGB[i]);
            icon_px(x + 2, 4, ICONS[i], 21, muted ? C_MUTED : ENG_RGB[i], on ? ENG_DARK[i] : C_PANEL);
            if (i < NENG) cv_rect(x + 4, 28, 17, 3, armed ? C_REC : muted ? C_MUTED : on ? ENG_RGB[i] : ENG_DARK[i]);   /* the track's state line */
        }
        int_text(PAT->bpm, t);
        cv_text(235 - text_w(&FONT_XS, t), 3, &FONT_XS, t, seq.playing ? C_TXT : C_SUB);
        cv_text(235 - text_w(&FONT_XS, tr), 18, &FONT_XS, tr, seq.rec ? C_REC : seq.playing ? ENG_RGB[3] : C_MUTED);
        for (i = 0; i < NPAT; i++) {                               /* the eight pattern slots, the live one lit */
            int32_t x = 166 + (int32_t)i * 6;
            cv_rect(x, 4, 5, 5, i == set.pattern ? C_SHIP : seq.chain ? ENG_DARK[4] : C_RULE);
        }
        if (seq.chain) cv_text(176, 12, &FONT_T, "CHAIN", C_SHIP);
    }
    cv_rect(0, 35, 240, 1, accent);
    cv_blit(0, 0);

    /* body */
    cv_begin(240, 128, C_BG);
    if (body == PG_ENGINE) {
        int hop_wave = 0;
        icon_px(4, 0, ICONS[set.focus], 34, accent, C_BG);
        cv_text(44, 2, &FONT_B, cur->name, accent);
        if (cur->titles) cv_text(44, 18, &FONT_XS, cur->titles[set.page[set.focus]], C_SUB);
        {                                                              /* the track's state, always in view */
            int armed = seq.armed >> set.focus & 1u, muted = !lane_audible(PAT, (uint8_t)set.focus);
            const char *bd = seq.stepin && !seq.playing ? "STEP" : armed && seq.playing && seq.rec ? "REC" : armed ? "ARMED" : muted ? "MUTED" : 0;
            if (bd) { int32_t bx = 44 + text_w(&FONT_B, cur->name) + 8, bw = text_w(&FONT_XS, bd) + 8; cv_rect(bx, 3, bw, 12, muted && !armed ? C_MUTED : C_REC); cv_text(bx + 4, 4, &FONT_XS, bd, C_TXT); }
        }
        for (i = 0; i < cur->npages; i++) cv_rect(44 + text_w(&FONT_XS, cur->titles ? cur->titles[set.page[set.focus]] : "") + 8 + (int32_t)i * 8, 23, 5, 3, set.page[set.focus] == i ? accent : C_RULE);
        {
            const char *a = "", *b = "";
            if (set.focus == E_OTTER) { a = otter_preset_name(); t[0] = set.octave < 0 ? '-' : '+'; t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave)); t[2] = 0; b = t; }
            else if (set.focus == E_CAT) { a = cat_kit_name(); b = cat_pad_name(cat_last_pad()); }
            else if (set.focus == E_FOX) { a = fox_mode() ? "SYNTH" : "REPTILE"; b = fox_preset_name(); }
            else { static char sb[14]; a = "PRESET"; b = hop_source_name(sb); }
            cv_text(232 - text_w(&FONT_XS, b), 4, &FONT_XS, b, set.focus == E_HOP && hop_recording() ? C_REC : C_TXT);
            cv_text(232 - text_w(&FONT_XS, a), 17, &FONT_XS, a, C_SUB);
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
            rowbar((int32_t)i, nm, v, 0, 127, accent, txt, -1);
        }
        if (set.focus == E_CAT) {                                      /* Mantis: the sixteen voices, the one you hit lit */
            for (i = 0; i < 16u; i++) {
                int32_t x = 4 + (int32_t)(i & 7u) * 29, y = 107 + (int32_t)(i >> 3) * 11;
                int on = (int32_t)i == cat_last_pad();
                if (on) cv_rect(x - 1, y - 1, 28, 10, accent);
                cv_text(x, y, &FONT_T, cat_pad_name((int)i), on ? C_BG : C_MUTED);
            }
        } else {                                                       /* a synth: where ALGO sits among the presets */
            const char *pv = set.focus == E_OTTER ? otter_preset_name_at(-1) : set.focus == E_FOX ? fox_preset_name_at(-1) : greys_preset_name_at(-1);
            const char *nx = set.focus == E_OTTER ? otter_preset_name_at(1) : set.focus == E_FOX ? fox_preset_name_at(1) : greys_preset_name_at(1);
            const char *cu = set.focus == E_OTTER ? otter_preset_name() : set.focus == E_FOX ? fox_preset_name() : greys_preset_name_at(0);
            cv_rect(0, 104, 240, 1, C_RULE);
            cv_text(6, 112, &FONT_T, "<", C_MUTED); cv_text(14, 112, &FONT_T, pv, C_MUTED);
            cv_text(120 - text_w(&FONT_XS, cu) / 2, 109, &FONT_XS, cu, accent);
            cv_text(226 - text_w(&FONT_T, nx), 112, &FONT_T, nx, C_MUTED); cv_text(229, 112, &FONT_T, ">", C_MUTED);
        }
    } else if (body == PG_HOME) {
        {                                                              /* the mothership: a starfield behind the title */
            uint32_t sr = 0x9E3779B9u, k;
            for (k = 0; k < 70u; k++) { sr = sr * 1664525u + 1013904223u; cv_rect((int32_t)((sr >> 8) % 240u), (int32_t)((sr >> 20) % 128u), 1, 1, (sr >> 30) ? C_MUTED : C_SUB); }
        }
        cv_text(8, 4, &FONT_B, "ALIEN STACK", C_BRAND);
        cv_text(8 + text_w(&FONT_B, "ALIEN STACK") + 8, 8, &FONT_XS, "WE COME IN BEATS", C_SUB);
        for (i = 0; i < 5u; i++) {                                     /* five tiles */
            int32_t x = 8 + (int32_t)i * 46, y = 24;
            int is_eng = i < NENG, muted = is_eng && !lane_audible(PAT, (uint8_t)i), armed = is_eng && (seq.armed >> i & 1u);
            char sb0[14];
            const char *sub = i == E_OTTER ? otter_preset_name() : i == E_CAT ? cat_kit_name() : i == E_FOX ? fox_preset_name() : i == E_HOP ? hop_source_name(sb0) : (seq.playing ? "PLAY" : "STOP");
            char lv[4];
            ui_panel(x, y, 42, 100);
            icon_px(x + 3, y + 3, ICONS[i], 36, muted ? C_MUTED : ENG_RGB[i], C_PANEL);
            cv_text(x + (42 - text_w(&FONT_T, i < NENG ? ENG[i]->name : "SHIP")) / 2, y + 40, &FONT_T, i < NENG ? ENG[i]->name : "SHIP", muted ? C_MUTED : ENG_RGB[i]);
            { char ft[14]; cv_text(x + 3, y + 52, &FONT_T, fit_text(sub, &FONT_T, 38, ft), C_SUB); }
            if (is_eng) {
                int32_t m = (int32_t)(meter[i] * 30.0f); if (m > 30) m = 30;
                cv_rect(x + 3, y + 64, 36, 4, C_RULE); cv_rect(x + 3, y + 64, set.level[i] * 36 / 127, 4, muted ? C_MUTED : ENG_RGB[i]);
                cv_rect(x + 3, y + 70, m * 36 / 30, 2, C_TXT);
                int_text(set.level[i], lv); cv_text(x + 3, y + 76, &FONT_T, lv, C_TXT);
                if (armed) cv_text(x + 39 - text_w(&FONT_T, "REC"), y + 76, &FONT_T, "REC", C_REC);
                else if (muted) cv_text(x + 39 - text_w(&FONT_T, "MUTE"), y + 76, &FONT_T, "MUTE", C_MUTED);
            } else {
                t[0] = (char)('A' + set.pattern); t[1] = 0;
                int_text(PAT->bpm, lv);
                cv_text(x + 3, y + 64, &FONT_T, "PAT", C_SUB); cv_text(x + 3 + text_w(&FONT_T, "PAT "), y + 64, &FONT_T, t, C_SHIP);
                cv_text(x + 3, y + 76, &FONT_T, lv, C_TXT); cv_text(x + 3 + text_w(&FONT_T, lv) + 3, y + 76, &FONT_T, "BPM", C_SUB);
            }
            cv_text(x + 3, y + 88, &FONT_T, i == 0 ? "KEY 1" : i == 1 ? "KEY 2" : i == 2 ? "KEY 3" : i == 3 ? "KEY 4" : "KEY 5", C_MUTED);
        }
    } else if (body == PG_SEQ) {
        uint32_t e, s, shown = PAT->steps > 16u ? 32u : 16u, cw = shown > 16u ? 6u : 13u;
        icon_px(4, 0, ICONS[4], 34, accent, C_BG);
        cv_text(44, 6, &FONT_B, "SHIP", accent);
        if (seq.chain && set.song_len) {                             /* the song, the live slot lit */
            int32_t sw = set.song_len > 8u ? 8 : 13;
            for (i = 0; i < set.song_len; i++) {
                char l[2] = {(char)('A' + (set.song[i] & 7u)), 0};
                cv_rect(104 + (int32_t)i * sw, 5, sw - 2, 16, i == seq.chain_pos ? accent : C_PANEL);
                cv_text(104 + (int32_t)i * sw + (sw > 8 ? 3 : 1), 7, sw > 8 ? &FONT_XS : &FONT_T, l, i == seq.chain_pos ? C_BG : C_SUB);
            }
        } else for (i = 0; i < NPAT; i++) {
            char l[2] = {(char)('A' + i), 0};
            cv_rect(104 + (int32_t)i * 13, 5, 11, 16, i == set.pattern ? accent : C_PANEL);
            cv_text(104 + (int32_t)i * 13 + 3, 7, &FONT_XS, l, i == set.pattern ? C_BG : C_SUB);
        }
        cv_text(232 - text_w(&FONT_T, ENG[set.focus]->name), 22, &FONT_T, ENG[set.focus]->name, ENG_RGB[set.focus]);
        if (ui.shippage == 3u) {                                       /* the song builder: 16 slots, two rows */
            for (i = 0; i < NSONG; i++) {
                int32_t x = 6 + (int32_t)(i & 7u) * 29, y = 30 + (int32_t)(i >> 3) * 24;
                int in = i < set.song_len, sel = i == ui.song_sel, live = seq.chain && in && i == seq.chain_pos;
                char l[4];
                cv_rect(x, y, 27, 21, live ? accent : sel ? C_TXT : in ? C_PANEL : C_BG);
                if (!in) { cv_rect(x, y, 27, 21, C_BG); cv_rect(x, y, 27, 1, C_RULE); cv_rect(x, y + 20, 27, 1, C_RULE); }
                if (in) {
                    l[0] = (char)('A' + SONG_PAT(set.song[i])); l[1] = 0;
                    cv_text(x + 3, y + 2, &FONT_XS, l, live || sel ? C_BG : C_TXT);
                    int_text((int32_t)SONG_REP(set.song[i]), l); text_cat(l, "x");
                    cv_text(x + 25 - text_w(&FONT_T, l), y + 3, &FONT_T, l, live || sel ? C_BG : C_SUB);
                    if (set.song_scene[i]) { int_text(set.song_scene[i], l); cv_text(x + 3, y + 12, &FONT_T, "S", live || sel ? C_BG : C_BRAND); cv_text(x + 9, y + 12, &FONT_T, l, live || sel ? C_BG : C_BRAND); }
                } else if (sel) cv_text(x + 4, y + 5, &FONT_T, "ADD", C_SUB);
            }
        } else for (e = 0; e < NENG; e++) {
            int32_t y = 30 + (int32_t)e * 11;
            int muted = !lane_audible(PAT, (uint8_t)e), armed = seq.armed >> e & 1u;
            cv_rect(8, y + 1, 12, 8, e == set.focus ? ENG_RGB[e] : muted ? C_MUTED : ENG_DARK[e]);
            if (armed) cv_rect(8, y + 1, 3, 8, C_REC);
            uint32_t ll = PAT->llen[e] ? PAT->llen[e] : PAT->steps;
            int32_t lst = seq.playing ? (PAT->llen[e] ? seq.lstep[e] : st) : -1;
            for (s = 0; s < shown; s++) {
                int has = seq_lane_has(PAT, (uint8_t)e, s), in = s < ll, on = (int32_t)s == lst;
                uint16_t c = on && has ? C_TXT : !in ? C_BG : has ? (muted ? C_MUTED : ENG_RGB[e]) : (s & 3u) ? C_PANEL : C_RULE;
                if ((int32_t)s == ui.held_step && e == set.focus) c = C_TXT;
                if (shown > 16u && e == set.focus && (s >> 4) != ui.steppage) c = rgb_mix(c, 0, 0, 0, 90);   /* the half the keys do not address: dimmer */
                if (seq.stepin && !seq.playing && (int32_t)s == seq.cursor && e == set.focus && !has) c = C_REC;
                cv_rect(26 + (int32_t)(s * cw), y + 1, (int32_t)cw - 2, 8, c);
                if (e != E_CAT && (LLOCK(&PAT->lane[e], s) & 0x80u)) cv_rect(26 + (int32_t)(s * cw), y + 1, (int32_t)cw - 2, 2, C_BRAND);   /* a lock */
                if (has && STEP_RAT(PAT->lane[e].len[s])) {               /* ratchet: the cell split into its hits */
                    uint32_t r = STEP_RAT(PAT->lane[e].len[s]) + 1u, q;
                    for (q = 1; q < r; q++) cv_rect(26 + (int32_t)(s * cw) + (int32_t)((cw - 2u) * q / r), y + 1, 1, 8, C_BG);
                }
            }
        }
        for (i = 0; i < NPARAM; i++) {
            const char *nm, *txt = 0; uint8_t *pp = knob_target(i, &nm); int32_t v = *pp, lo = 0, hi = 127;
            if (ui.held_step >= 0 && ui.lock_arm) {
                uint8_t lk = LLOCK(&PAT->lane[set.focus], ui.held_step);
                nm = page_names(set.focus, 0)[i];
                if ((lk & 0x80u) && (lk >> 5 & 3u) == i) v = (int32_t)(lk & 31u) * 4 + 2; else { v = ENG[set.focus]->params[0][i]; txt = "-"; }
            } else if (ui.shippage == 2u) { int8_t tr = (int8_t)v; v = tr + 24; hi = 48; if (i == E_CAT) txt = "-"; else { t[0] = tr < 0 ? '-' : '+'; int_text(tr < 0 ? -tr : tr, t + 1); txt = t; } }
            else if (ui.shippage == 3u) {
                uint32_t sl = ui.song_sel; int in = sl < set.song_len;
                if (i == 0) { v = (int32_t)sl; hi = NSONG - 1; int_text((int32_t)sl + 1, t); txt = t; }
                else if (i == 1) { v = in ? (int32_t)SONG_PAT(set.song[sl]) : 0; hi = NPAT - 1; if (in) { t[0] = (char)('A' + v); t[1] = 0; txt = t; } else txt = "-"; }
                else if (i == 2) { v = in ? (int32_t)SONG_REP(set.song[sl]) : 0; hi = 8; if (in) { int_text(v, t); text_cat(t, "X"); txt = t; } else txt = "-"; }
                else { v = in ? set.song_scene[sl] : 0; hi = NSCENE; if (!in) txt = "-"; else if (!v) txt = "NONE"; }
            }
            else if (ui.shippage) { hi = NSTEP; if (!v) txt = "PAT"; }
            else if (ui.held_step >= 0) {
                if (i == 0) { if (set.focus == E_CAT) { int_text(cat_last_pad() + 1, t); txt = t; v = cat_last_pad() * 8; } else if (lane_rest(set.focus, (uint8_t)v)) { txt = "REST"; v = 0; } }
                else if (i == 2) { uint32_t r = STEP_RAT((uint8_t)v); v = (int32_t)STEP_LEN((uint8_t)v); if (r) { t[0] = 'x'; t[1] = (char)('1' + r); t[2] = 0; txt = t; } else if (!v) txt = "GATE"; hi = 16; }
                else if (i == 3 && v >= 127) txt = "ALWAYS";
            } else if (i == 0) { lo = 40; hi = 240; } else if (i == 2) { hi = NSTEP; }
            bar_h(BX(i), 78, 20, nm, v, lo, hi, ui.held_step >= 0 && ui.lock_arm ? C_BRAND : ui.held_step >= 0 ? ENG_RGB[set.focus] : ui.shippage ? ENG_RGB[i] : accent, txt);
        }
    } else {
        cv_text(8, 4, &FONT_B, body_name(), accent);
        cv_text(232 - text_w(&FONT_XS, "MUTE 1-4 SOLO 9-12 SCENES 5-8"), 8, &FONT_XS, "MUTE 1-4 SOLO 9-12 SCENES 5-8", C_SUB);
        if (set.mixpage == 1u) { for (i = 0; i < NENG; i++) rowpan((int32_t)i, ENG[i]->name, set.pan[i], ENG_RGB[i]); }
        else if (set.mixpage == 4u) { for (i = 0; i < NENG; i++) rowbar((int32_t)i, ENG[i]->name, set.send[i], 0, 127, ENG_RGB[i], set.send[i] ? 0 : "DRY", -1); }
        else if (set.mixpage == 3u) {
            bar(BX(0), 34, "GLUE", set.glue, 0, 127, C_BRAND, set.glue ? 0 : "OFF");
            bar(BX(1), 34, "LOWEND", set.lowend, 0, 127, C_BRAND, set.lowend ? 0 : "FLAT");
            bar(BX(2), 34, "AIR", set.air, 0, 127, C_BRAND, set.air ? 0 : "FLAT");
            bar(BX(3), 34, "DRIVE", set.drive, 0, 127, C_BRAND, set.drive ? 0 : "CLEAN");
        }
        else if (set.mixpage == 2u) {
            bar(BX(0), 34, "VERB", set.verb, 0, 127, C_BRAND, set.verb ? 0 : "DRY");
            bar(BX(1), 34, "SIZE", set.verb_size, 0, 127, C_BRAND, 0);
            bar(BX(2), 34, "DELAY", set.delay_mix, 0, 127, C_TXT, set.delay_mix ? 0 : "DRY");
            bar(BX(3), 34, "DTIME", set.delay_time, 0, 127, C_TXT, 0);
        } else for (i = 0; i < NENG; i++) {
            int muted = !lane_audible(PAT, (uint8_t)i);
            int32_t m = (int32_t)(meter[i] * 106.0f);
            rowbar((int32_t)i, ENG[i]->name, set.level[i], 0, 127, muted ? C_MUTED : ENG_RGB[i], muted ? (seq.solo && !(seq.solo >> i & 1u) ? "-" : "MUTE") : seq.solo >> i & 1u ? "SOLO" : 0, m);
        }
    }
    cv_blit(0, 36);

    /* bottom: the step strip, the keys, the legend */
    cv_begin(240, 76, C_BG);
    {
        uint32_t shown = PAT->steps > 16u ? 32u : 16u, cw = shown > 16u ? 7u : 14u;
        for (i = 0; i < shown; i++) {
            int in = i < PAT->steps, on = (seq.playing && (int32_t)i == st) || (seq.stepin && !seq.playing && (int32_t)i == seq.cursor), has = seq_lane_has(PAT, set.focus, i);
            cv_rect(5 + (int32_t)(i * cw), 2, (int32_t)cw - 2, 5, on ? (seq.playing ? C_TXT : C_REC) : !in ? C_BG : has ? ENG_RGB[set.focus] : (i & 3u) ? C_PANEL : C_RULE);
        }
    }
    for (i = 0; i < 16u; i++) {
        int held = ui.keys >> WK[i] & 1u;
        int pad = body == PG_ENGINE && (set.focus == E_CAT || (set.focus == E_HOP && hop_chop_mode() && i < (uint32_t)hop_chops()));
        int mixkey = body == PG_MIX && (i < 4u || (i >= 8u && i < 12u));
        int scenekey = body == PG_MIX && scene_key((int32_t)i) >= 0;
        int homekey = body == PG_HOME && i < 7u;
        uint16_t c = held ? ENG_RGB[set.focus] : pad ? C_SUB : mixkey ? ENG_DARK[i & 3u] : scenekey ? (set.scene[scene_key((int32_t)i)].used ? C_BRAND : C_RULE) : homekey ? (i < 5u ? ENG_DARK[i] : i == 6u ? C_BRAND : C_RULE) : C_MUTED;
        if (body == PG_SEQ) c = held ? C_TXT : i < PAT->steps ? (seq_lane_has(PAT, set.focus, i) ? ENG_RGB[set.focus] : C_MUTED) : C_PANEL;
        cv_rect(4 + (int32_t)i * 14, 24, 13, 30, c);
        if (pad || mixkey || homekey) { int_text((int32_t)(mixkey ? (i & 3u) : i) + 1, t); cv_text(5 + (int32_t)i * 14 + (i < 9u || mixkey ? 2 : 0), 42, &FONT_XS, t, held ? C_BG : C_TXT); }
    }
    for (i = 0; i < 11u; i++) {
        uint16_t c = ui.keys >> BK[i] & 1u ? C_TXT : C_RULE;
        if (body == PG_SEQ) { if (i <= 3u) c = C_SHIP; else if (i >= 5u && i <= 8u) c = (PAT->mute >> (i - 5u) & 1u) ? C_REC : ENG_DARK[i - 5u]; else if (i == 10u) c = seq.solo ? ENG_RGB[3] : C_RULE; else if (i == 4u || i == 9u) c = C_BRAND; if (ui.keys >> BK[i] & 1u) c = C_TXT; }
        cv_rect(4 + (int32_t)AT[i] * 14 + 8, 10, 11, 18, c);
    }
    cv_text(4, 61, &FONT_XS, body == PG_HOME ? "KEYS 1-6 JUMP  7 = NEW SIGNAL" : body == PG_SEQ ? "PRESET: PATTERN LENGTH TRANSP SONG" : body == PG_MIX ? "SCENES: TAP / HOLD" : seq.stepin && !seq.playing ? "STEP ENTRY: KEYS WRITE  OCT+ REST" : "REC=STEP  ARP=NORDIC ARP  HOLD+K1", C_SUB);
    int_text((int32_t)(plat_master() * 100u / 4096u), t); text_cat(t, "%");
    cv_text(236 - text_w(&FONT_XS, t), 61, &FONT_XS, t, C_TXT);
    if (body == PG_MIX) {                                              /* the mixer shows the audio load and any xruns */
        char c[12]; uint32_t pct = plat_cpu_pct(), xr = plat_xruns();
        int_text((int32_t)pct, c); text_cat(c, xr ? "% XRUN" : "% CPU");
        cv_text(236 - text_w(&FONT_XS, t) - 10 - text_w(&FONT_XS, c), 61, &FONT_XS, c, xr ? C_REC : pct > 80u ? C_REC : C_SUB);
    }
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
    if (((body == PG_MIX && set.mixpage == 0u) || body == PG_HOME) && now - last_meter > 80u) { last_meter = now; ui.redraw = 1; }
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
    else if (!strcmp(what, "stepin")) *got = seq.stepin;
    else if (!strcmp(what, "song_len")) *got = set.song_len;
    else if (!strcmp(what, "song_rep0")) *got = (int)SONG_REP(set.song[0]);
    else if (!strcmp(what, "song_scene1")) *got = set.song_scene[1];
    else if (!strcmp(what, "scene0")) *got = set.scene[0].used;
    else if (!strcmp(what, "hop_source")) *got = hop_source_index();
    else if (!strcmp(what, "lock0")) *got = LLOCK(&PAT->lane[set.focus], 0);
    else if (!strcmp(what, "cat_hits")) { uint32_t i, c = 0; for (i = 0; i < NSTEP; i++) c += LMASK(&PAT->lane[E_CAT], i) != 0; *got = (int)c; }
    else if (!strcmp(what, "step_16on")) *got = seq_lane_has(PAT, set.focus, 16);
    else if (!strcmp(what, "steps")) *got = PAT->steps;
    else if (!strcmp(what, "tr_rep")) *got = set.transpose[E_FOX];
    else if (!strcmp(what, "fill")) *got = ui.fill_on;
    else if (!strcmp(what, "llen2")) *got = PAT->llen[2];
    else if (!strcmp(what, "fox_p0")) *got = ENG[E_FOX]->params[0][0];
    else if (!strcmp(what, "cursor")) *got = seq.cursor;
    else if (!strcmp(what, "fox_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += LNOTE(&PAT->lane[E_FOX], i) != 0; *got = (int)n; }
    else if (!strcmp(what, "step")) *got = seq.step;
    else if (!strcmp(what, "bpm")) *got = PAT->bpm;
    else if (!strcmp(what, "pattern")) *got = set.pattern;
    else if (!strcmp(what, "fox_mode")) *got = fox_mode();
    else if (!strcmp(what, "hop_rec")) *got = hop_recording();
    else if (!strcmp(what, "hop_secs")) *got = (int)hop_bank_seconds_x10();
    else if (!strcmp(what, "hop_peak")) *got = (int)hop_peak(0, hop_bank_len());
    else if (!strcmp(what, "kit")) *got = cat_kit_index();
    else if (!strcmp(what, "mixpage")) *got = set.mixpage;
    else if (!strcmp(what, "cat_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += LMASK(&PAT->lane[E_CAT], i) != 0; *got = (int)n; }
    else if (!strcmp(what, "otter_steps")) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += LNOTE(&PAT->lane[E_OTTER], i) != 0; *got = (int)n; }
    else if (!strncmp(what, "step_", 5)) { const char *w = what + 5; uint32_t st = (uint32_t)atoi(w); const char *c = w; while (*c >= '0' && *c <= '9') c++; *got = *seq_step_param(PAT, set.focus, st, !strcmp(c, "vel") ? 1 : !strcmp(c, "len") ? 2 : !strcmp(c, "prob") ? 3 : 0); }
    else if (!strncmp(what, "param", 5)) { const char *nm; *got = *knob_target((uint32_t)atoi(what + 5) % NPARAM, &nm); }
    else return 0;
    return 1;
}
#endif
