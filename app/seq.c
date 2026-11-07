/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * The sequencer: 16 steps, one lane per engine, one tempo. Live record: REC lit, PLAY, play the keys; notes land on
 * the nearest step. Hold REC a second: the focused lane is cleared. Part of app.c (unity build).
 *   Otter lane: one note a step (and its velocity); the note holds for GATE of the step
 *   Cat lane:   a 16-bit pad mask a step, one velocity
 *   Fox lane:   one note or slice a step */
#define NSTEP 16

typedef struct {
    uint8_t bpm;                          /* 40..240 */
    uint8_t swing;                        /* 0..127: 0 straight, 127 heavy */
    uint8_t steps;                        /* 1..16 */
    uint8_t gate;                         /* Otter's note length, 0..127 of a step */
    uint8_t otter_note[NSTEP], otter_vel[NSTEP];        /* note 0 = rest */
    uint16_t cat_mask[NSTEP]; uint8_t cat_vel[NSTEP];
    uint8_t fox_note[NSTEP], fox_vel[NSTEP];            /* 255 = rest (0 is slice 0 in chop mode) */
} pattern_t;

static struct {
    volatile uint8_t playing, rec;
    volatile int32_t step;                /* the step playing, -1 stopped */
    uint32_t phase;                       /* samples into the step (audio irq) */
    uint32_t step_len;                    /* samples a step */
    int32_t otter_held;                   /* the note the sequencer holds on Otter, -1 none */
    uint32_t otter_off_at;                /* phase at which it is released */
    uint8_t late;                         /* the current step's events already fired */
} seq;

static void pattern_defaults(pattern_t *p)
{
    uint32_t i;
    p->bpm = 120; p->swing = 20; p->steps = 16; p->gate = 60;
    for (i = 0; i < NSTEP; i++) {
        p->otter_note[i] = 0; p->otter_vel[i] = 100;
        p->cat_mask[i] = 0; p->cat_vel[i] = 100;
        p->fox_note[i] = 255; p->fox_vel[i] = 100;
    }
    /* a starter beat, so PLAY makes noise: kick on 1, 5, 9, 13; closed hat on the 8ths; snare on 5, 13 */
    for (i = 0; i < NSTEP; i += 4) p->cat_mask[i] |= 1u << 0;
    for (i = 0; i < NSTEP; i += 2) p->cat_mask[i] |= 1u << 2;
    p->cat_mask[4] |= 1u << 1; p->cat_mask[12] |= 1u << 1;
}

static uint32_t seq_step_samples(const pattern_t *p)
{
    uint32_t bpm = p->bpm < 40u ? 40u : p->bpm;
    return (uint32_t)(44100.0f * 15.0f / (float)bpm);   /* a 16th */
}

/* the current step's length with swing: even steps longer, odd shorter */
static uint32_t seq_this_len(const pattern_t *p, int32_t st)
{
    uint32_t base = seq.step_len, sw = (uint32_t)p->swing * base / 127u / 3u;   /* up to a third of a step */
    return (st & 1) ? base - sw : base + sw;
}

/* audio irq: advance, fire the step's events through the engines */
static void seq_fire(const pattern_t *p, int32_t st)
{
    uint32_t i;
    if (p->otter_note[st]) {
        if (seq.otter_held >= 0) otter.note(seq.otter_held, 0, 0);
        otter.note(p->otter_note[st], p->otter_vel[st], 1);
        seq.otter_held = p->otter_note[st];
        seq.otter_off_at = seq_this_len(p, st) * (uint32_t)p->gate / 127u;
        if (seq.otter_off_at < 300u) seq.otter_off_at = 300u;
    }
    if (p->cat_mask[st])
        for (i = 0; i < 16u; i++)
            if (p->cat_mask[st] >> i & 1u) cat.note((int32_t)i, p->cat_vel[st], 1);
    if (p->fox_note[st] != 255u)
        fox.note(p->fox_note[st], p->fox_vel[st], 1);
}

static void seq_tick(pattern_t *p, uint32_t n)       /* n samples pass */
{
    if (!seq.playing) return;
    if (seq.step < 0) {                                  /* just started */
        seq.step = 0; seq.phase = 0; seq.step_len = seq_step_samples(p);
        seq_fire(p, 0);
    }
    seq.phase += n;
    if (seq.otter_held >= 0 && seq.phase >= seq.otter_off_at) {
        otter.note(seq.otter_held, 0, 0);
        seq.otter_held = -1;
    }
    if (seq.phase >= seq_this_len(p, seq.step)) {
        seq.phase -= seq_this_len(p, seq.step);
        seq.step_len = seq_step_samples(p);
        seq.step = (seq.step + 1) % (p->steps ? p->steps : 1);
        seq_fire(p, seq.step);
    }
}

static void seq_start(void) { seq.step = -1; seq.otter_held = -1; seq.playing = 1; }
static void seq_stop(void)
{
    seq.playing = 0;
    if (seq.otter_held >= 0) { otter.note(seq.otter_held, 0, 0); seq.otter_held = -1; }
    seq.step = -1;
}

/* main loop: a key or MIDI note on the focused engine while recording lands on the nearest step */
static void seq_record(pattern_t *p, uint8_t e, int32_t note, uint32_t vel)
{
    int32_t st;
    if (!seq.rec || !seq.playing || seq.step < 0 || note < 0) return;
    st = seq.step;
    if (seq.phase > seq_this_len(p, st) / 2u) st = (st + 1) % (p->steps ? p->steps : 1);   /* late in the step: the next */
    if (e == E_OTTER) { p->otter_note[st] = (uint8_t)note; p->otter_vel[st] = (uint8_t)vel; }
    else if (e == E_CAT) { if (note < 16) { p->cat_mask[st] |= (uint16_t)(1u << note); p->cat_vel[st] = (uint8_t)vel; } }
    else { p->fox_note[st] = (uint8_t)note; p->fox_vel[st] = (uint8_t)vel; }
}

static void seq_clear_lane(pattern_t *p, uint8_t e)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        if (e == E_OTTER) p->otter_note[i] = 0;
        else if (e == E_CAT) p->cat_mask[i] = 0;
        else p->fox_note[i] = 255;
    }
}

static int seq_lane_has(const pattern_t *p, uint8_t e, uint32_t st)
{
    return e == E_OTTER ? p->otter_note[st] != 0 : e == E_CAT ? p->cat_mask[st] != 0 : p->fox_note[st] != 255u;
}
