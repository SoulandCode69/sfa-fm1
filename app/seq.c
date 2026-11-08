/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * BEAVER: the sequencer. Four tracks (Otter, Cat, Fox, Hopper), patterns of up to 32 steps, one tempo, swing.
 * Every step: a note (Cat: a pad mask), VELOCITY, LENGTH (in quarter steps, 0 = the track's gate) and PROBABILITY.
 * Live record onto the armed track; step entry on the Beaver page; copy / paste / clear; a chain A>B>C>D.
 * Part of app.c (unity build). */
#define NSTEP 32
#define NLANE 4

typedef struct {
    uint8_t note[NSTEP];                  /* 0 = rest (Hopper: 255 = rest); Cat: unused */
    uint16_t mask[NSTEP];                 /* Cat: the pads; others unused */
    uint8_t vel[NSTEP], len[NSTEP], prob[NSTEP];
} lane_t;

typedef struct {
    uint8_t bpm, swing, steps, gate;      /* 40..240; 0..127; 1..32; 0..127 of a step */
    uint8_t mute;                         /* bit per lane */
    uint8_t rsv[3];
    lane_t lane[NLANE];
} pattern_t;

static struct {
    volatile uint8_t playing, rec, armed; /* armed: bit per lane */
    volatile int32_t step;
    uint32_t phase, step_len;
    int32_t held[NLANE];                  /* the note the sequencer holds on a melodic lane, -1 none */
    uint32_t off_at[NLANE], held_t[NLANE]; /* its length, and how long it has sounded (samples) */
    uint8_t solo;                         /* bit per lane, 0 = none */
    uint8_t stepin;                       /* step entry: keys write at the cursor and advance it */
    int32_t cursor;
    uint8_t chain, chain_pos;             /* chain mode: play the slots in a row */
    uint32_t rng;
} seq;

static int lane_rest(uint8_t e, uint8_t v) { return e == E_HOP ? v == 255u : v == 0u; }
static uint8_t lane_rest_value(uint8_t e) { return e == E_HOP ? 255u : 0u; }

static void pattern_defaults(pattern_t *p)
{
    uint32_t e, i;
    p->bpm = 120; p->swing = 20; p->steps = 16; p->gate = 60; p->mute = 0;
    for (e = 0; e < NLANE; e++) for (i = 0; i < NSTEP; i++) {
        p->lane[e].note[i] = lane_rest_value((uint8_t)e); p->lane[e].mask[i] = 0;
        p->lane[e].vel[i] = 100; p->lane[e].len[i] = 0; p->lane[e].prob[i] = 127;
    }
    for (i = 0; i < 16u; i += 4) p->lane[E_CAT].mask[i] |= 1u << 0;    /* a starter beat */
    for (i = 0; i < 16u; i += 2) p->lane[E_CAT].mask[i] |= 1u << 5;
    p->lane[E_CAT].mask[4] |= 1u << 2; p->lane[E_CAT].mask[12] |= 1u << 2;
}

static uint32_t seq_step_samples(const pattern_t *p)
{
    uint32_t bpm = p->bpm < 40u ? 40u : p->bpm;
    return (uint32_t)(44100.0f * 15.0f / (float)bpm);
}
static uint32_t seq_this_len(const pattern_t *p, int32_t st)
{
    uint32_t base = seq.step_len, sw = (uint32_t)p->swing * base / 127u / 3u;
    return (st & 1) ? base - sw : base + sw;
}
static int lane_audible(const pattern_t *p, uint8_t e) { return !(p->mute >> e & 1u) && (!seq.solo || (seq.solo >> e & 1u)); }

static void seq_release(uint8_t e)
{
    if (seq.held[e] >= 0) { ENG[e]->note(seq.held[e], 0, 0); seq.held[e] = -1; }
}

static void seq_fire(const pattern_t *p, int32_t st)
{
    uint32_t e, i;
    for (e = 0; e < NLANE; e++) {
        const lane_t *l = &p->lane[e];
        uint32_t len_q = l->len[st] ? l->len[st] : 0u;
        if (e != E_CAT && !lane_rest((uint8_t)e, l->note[st])) seq_release((uint8_t)e);
        if (!lane_audible(p, (uint8_t)e)) continue;
        if (l->prob[st] < 127u) {                                        /* probability */
            seq.rng = seq.rng * 1664525u + 1013904223u;
            if ((seq.rng >> 24) > (uint32_t)l->prob[st] * 2u) continue;
        }
        if (e == E_CAT) {
            if (l->mask[st]) for (i = 0; i < 16u; i++) if (l->mask[st] >> i & 1u) cat.note((int32_t)i, l->vel[st], 1);
        } else if (!lane_rest((uint8_t)e, l->note[st])) {
            ENG[e]->note(l->note[st], l->vel[st], 1);
            if (e == E_HOP) continue;                                      /* one-shots */
            seq.held[e] = l->note[st];
            seq.held_t[e] = 0;
            seq.off_at[e] = len_q ? seq.step_len * len_q / 4u : seq_this_len(p, st) * (uint32_t)p->gate / 127u;
            if (seq.off_at[e] < 300u) seq.off_at[e] = 300u;
        }
    }
}

static void chain_advance(void);

static void seq_tick(pattern_t *p, uint32_t n)
{
    uint32_t e;
    if (!seq.playing) return;
    if (seq.step < 0) { seq.step = 0; seq.phase = 0; seq.step_len = seq_step_samples(p); seq_fire(p, 0); }
    seq.phase += n;
    for (e = 0; e < NLANE; e++)
        if (seq.held[e] >= 0) { seq.held_t[e] += n; if (seq.held_t[e] >= seq.off_at[e]) seq_release((uint8_t)e); }
    if (seq.phase >= seq_this_len(p, seq.step)) {
        seq.phase -= seq_this_len(p, seq.step);
        seq.step_len = seq_step_samples(p);
        seq.step = seq.step + 1;
        if (seq.step >= (int32_t)(p->steps ? p->steps : 1)) { seq.step = 0; if (seq.chain) chain_advance(); }
        seq_fire(PAT, seq.step);
    }
}

static void seq_start(void) { uint32_t e; seq.step = -1; for (e = 0; e < NLANE; e++) seq.held[e] = -1; seq.playing = 1; }
static void seq_stop(void)
{
    uint32_t e;
    seq.playing = 0;
    for (e = 0; e < NLANE; e++) seq_release((uint8_t)e);
    seq.step = -1;
}

/* live record: a note on the armed track lands on the nearest step */
static void seq_record(pattern_t *p, uint8_t e, int32_t note, uint32_t vel)
{
    int32_t st;
    lane_t *l = &p->lane[e];
    if (!seq.rec || !seq.playing || seq.step < 0 || note < 0 || !(seq.armed >> e & 1u)) return;
    st = seq.step;
    if (seq.phase > seq_this_len(p, st) / 2u) st = (st + 1) % (p->steps ? p->steps : 1);
    if (e == E_CAT) { if (note < 16) { l->mask[st] |= (uint16_t)(1u << note); l->vel[st] = (uint8_t)vel; } }
    else { l->note[st] = (uint8_t)note; l->vel[st] = (uint8_t)vel; }
}

static void seq_clear_lane(pattern_t *p, uint8_t e)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) { p->lane[e].note[i] = lane_rest_value(e); p->lane[e].mask[i] = 0; p->lane[e].len[i] = 0; p->lane[e].prob[i] = 127; }
}

static int seq_lane_has(const pattern_t *p, uint8_t e, uint32_t st)
{
    return e == E_CAT ? p->lane[e].mask[st] != 0 : !lane_rest(e, p->lane[e].note[st]);
}

/* the Beaver's keys: toggle a step of lane e */
static void seq_toggle(pattern_t *p, uint8_t e, uint32_t st, int32_t last_note, int last_pad, int last_slice)
{
    lane_t *l = &p->lane[e];
    if (st >= NSTEP) return;
    if (e == E_CAT) l->mask[st] ^= (uint16_t)(1u << (last_pad >= 0 ? last_pad : 0));
    else if (!lane_rest(e, l->note[st])) l->note[st] = lane_rest_value(e);
    else l->note[st] = e == E_HOP ? (uint8_t)(last_slice >= 0 ? last_slice : 0) : (uint8_t)(last_note > 0 && last_note < 120 ? last_note : e == E_FOX ? 36 : 48);
}

/* a step's four values for the knobs: NOTE/PAD, VEL, LEN, PROB */
static uint8_t *seq_step_param(pattern_t *p, uint8_t e, uint32_t st, uint32_t k)
{
    lane_t *l = &p->lane[e];
    return k == 0 ? &l->note[st] : k == 1 ? &l->vel[st] : k == 2 ? &l->len[st] : &l->prob[st];
}

/* step entry: a key on the focused lane writes the cursor's step and moves on (OCT-/+ step the cursor, + = a rest) */
static void seq_step_in(pattern_t *p, uint8_t e, int32_t note, uint32_t vel)
{
    lane_t *l = &p->lane[e];
    uint32_t n = p->steps ? p->steps : 1u;
    if (!seq.stepin || note < 0) return;
    if (seq.cursor < 0 || seq.cursor >= (int32_t)n) seq.cursor = 0;
    if (e == E_CAT) { if (note < 16) l->mask[seq.cursor] |= (uint16_t)(1u << note); }
    else l->note[seq.cursor] = (uint8_t)note;
    l->vel[seq.cursor] = (uint8_t)vel;
    seq.cursor = (seq.cursor + 1) % (int32_t)n;
}

/* the groove generators: a fresh, musical starting point for one lane every press */
static uint32_t rnd(uint32_t n) { seq.rng = seq.rng * 1664525u + 1013904223u; return (seq.rng >> 16) % n; }
static uint8_t hvel(uint32_t base) { int32_t v = (int32_t)base + (int32_t)rnd(13) - 6; return (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v); }
static void seq_gen(pattern_t *p, uint8_t e, int32_t root, int32_t nslices)
{
    static const uint16_t KICK[7] = {0x1111, 0x0511, 0x4141, 0x0909, 0x0491, 0x5111, 0x2111};   /* bit i = step i */
    static const uint16_t SNARE[4] = {0x1010, 0x1010, 0x9010, 0x1090};
    static const uint16_t HAT[5] = {0x5555, 0xFFFF, 0x4444, 0x5755, 0xDDDD};
    static const uint16_t BASS[6] = {0x4D4D, 0x1111, 0x5B5B, 0x2525, 0x4F45, 0x9999};
    static const int8_t PENTA[6] = {0, 3, 5, 7, 10, 12};
    lane_t *l = &p->lane[e];
    uint32_t i, n = p->steps ? p->steps : 16u;
    seq_clear_lane(p, e);
    if (e == E_CAT) {
        uint16_t k = KICK[rnd(7)], sn = SNARE[rnd(4)], h = HAT[rnd(5)];
        int clap = rnd(3) == 0, ohat = rnd(2), ghost = rnd(2), perc = rnd(3) == 0;
        for (i = 0; i < n; i++) {
            uint32_t b = i & 15u;
            if (k >> b & 1u) l->mask[i] |= 1u << 0;
            if (sn >> b & 1u) l->mask[i] |= clap ? 1u << 4 : 1u << 2;
            if (h >> b & 1u) l->mask[i] |= (ohat && (b == 14u || b == 6u)) ? 1u << 6 : 1u << 5;
            if (ghost && (b == 7u || b == 15u) && rnd(2)) { l->mask[i] |= 1u << 3; l->prob[i] = 90; }
            if (perc && (b == 3u || b == 11u)) l->mask[i] |= 1u << (11u + rnd(3));
            l->vel[i] = hvel((b & 3u) == 0u ? 112u : (b & 1u) ? 72u : 92u);
        }
    } else if (e == E_FOX) {
        uint16_t b = BASS[rnd(6)];
        int oct = rnd(3) == 0, fifth = rnd(2);
        if (root < 24 || root > 60) root = 36;
        for (i = 0; i < n; i++)
            if (b >> (i & 15u) & 1u) {
                int32_t nt = root;
                if (fifth && (i & 15u) >= 8u && rnd(3) == 0) nt += (int32_t)rnd(2) ? 7 : 5;
                if (oct && (i & 3u) == 2u) nt += 12;
                l->note[i] = (uint8_t)nt; l->vel[i] = hvel((i & 3u) ? 88u : 110u); l->len[i] = (uint8_t)(1u + rnd(3));
            }
    } else if (e == E_OTTER) {
        int dir = rnd(2), dense = rnd(2), hi = rnd(2);
        uint32_t idx = 0;
        if (root < 36 || root > 84) root = 60;
        for (i = 0; i < n; i++) {
            if (!dense && (i & 1u)) continue;
            if (rnd(8) == 0) { l->prob[i] = 100; }
            idx = dir ? (idx + 1u + (uint32_t)rnd(2)) % 6u : (idx + 5u) % 6u;
            l->note[i] = (uint8_t)(root + PENTA[idx] + (hi && rnd(4) == 0 ? 12 : 0));
            l->vel[i] = hvel((i & 3u) ? 80u : 100u); l->len[i] = (uint8_t)(dense ? 1u : 2u);
        }
    } else {
        int order = rnd(2), every = 1 + (int)rnd(2);
        if (nslices < 1) nslices = 1;
        for (i = 0; i < n; i += (uint32_t)every) {
            l->note[i] = (uint8_t)(order ? (i / (uint32_t)every) % (uint32_t)nslices : rnd((uint32_t)nslices));
            l->vel[i] = hvel(100u);
            if (rnd(6) == 0) l->prob[i] = 80;
        }
    }
}

/* copy / paste */
static pattern_t clip; static uint8_t clip_full;
static void pattern_copy(const pattern_t *p) { clip = *p; clip_full = 1; }
static int pattern_paste(pattern_t *p) { if (!clip_full) return 0; *p = clip; return 1; }
