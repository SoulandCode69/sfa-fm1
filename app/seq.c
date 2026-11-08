/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * BEAVER: the sequencer. Patterns of up to 32 steps, four lanes (Otter, Cat, Fox, Hopper), one tempo, swing.
 * Live record (REC lit, PLAY, play: notes land on the nearest step) and step entry (the Beaver page: white keys toggle
 * the focused lane's steps). Per step: a note (Cat: a pad mask) and a velocity. Part of app.c (unity build). */
#define NSTEP 32
#define NLANE 4

typedef struct {
    uint8_t bpm;                          /* 40..240 */
    uint8_t swing;                        /* 0..127 */
    uint8_t steps;                        /* 1..32 */
    uint8_t gate;                         /* Otter / Fox note length, 0..127 of a step */
    uint8_t otter_note[NSTEP], otter_vel[NSTEP];        /* 0 = rest */
    uint16_t cat_mask[NSTEP]; uint8_t cat_vel[NSTEP];
    uint8_t fox_note[NSTEP], fox_vel[NSTEP];            /* 0 = rest */
    uint8_t hop_note[NSTEP], hop_vel[NSTEP];            /* 255 = rest (0 is slice 0) */
    uint8_t mute;                         /* bit per lane */
    uint8_t rsv[3];
} pattern_t;

static struct {
    volatile uint8_t playing, rec;
    volatile int32_t step;
    uint32_t phase, step_len;
    int32_t otter_held, fox_held;
    uint32_t off_at;
} seq;

static void pattern_defaults(pattern_t *p)
{
    uint32_t i;
    p->bpm = 120; p->swing = 20; p->steps = 16; p->gate = 60; p->mute = 0;
    for (i = 0; i < NSTEP; i++) {
        p->otter_note[i] = 0; p->otter_vel[i] = 100;
        p->cat_mask[i] = 0; p->cat_vel[i] = 100;
        p->fox_note[i] = 0; p->fox_vel[i] = 100;
        p->hop_note[i] = 255; p->hop_vel[i] = 100;
    }
    for (i = 0; i < 16u; i += 4) p->cat_mask[i] |= 1u << 0;          /* a starter beat: kick, hats, snare */
    for (i = 0; i < 16u; i += 2) p->cat_mask[i] |= 1u << 5;
    p->cat_mask[4] |= 1u << 2; p->cat_mask[12] |= 1u << 2;
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

static void seq_fire(const pattern_t *p, int32_t st)
{
    uint32_t i;
    if (seq.otter_held >= 0) { otter.note(seq.otter_held, 0, 0); seq.otter_held = -1; }
    if (seq.fox_held >= 0) { fox.note(seq.fox_held, 0, 0); seq.fox_held = -1; }
    if (!(p->mute & 1u) && p->otter_note[st]) { otter.note(p->otter_note[st], p->otter_vel[st], 1); seq.otter_held = p->otter_note[st]; }
    if (!(p->mute & 4u) && p->fox_note[st]) { fox.note(p->fox_note[st], p->fox_vel[st], 1); seq.fox_held = p->fox_note[st]; }
    seq.off_at = seq_this_len(p, st) * (uint32_t)p->gate / 127u;
    if (seq.off_at < 300u) seq.off_at = 300u;
    if (!(p->mute & 2u) && p->cat_mask[st])
        for (i = 0; i < 16u; i++) if (p->cat_mask[st] >> i & 1u) cat.note((int32_t)i, p->cat_vel[st], 1);
    if (!(p->mute & 8u) && p->hop_note[st] != 255u) hopper.note(p->hop_note[st], p->hop_vel[st], 1);
}

static void seq_tick(pattern_t *p, uint32_t n)
{
    if (!seq.playing) return;
    if (seq.step < 0) { seq.step = 0; seq.phase = 0; seq.step_len = seq_step_samples(p); seq_fire(p, 0); }
    seq.phase += n;
    if (seq.phase >= seq.off_at) {
        if (seq.otter_held >= 0) { otter.note(seq.otter_held, 0, 0); seq.otter_held = -1; }
        if (seq.fox_held >= 0) { fox.note(seq.fox_held, 0, 0); seq.fox_held = -1; }
    }
    if (seq.phase >= seq_this_len(p, seq.step)) {
        seq.phase -= seq_this_len(p, seq.step);
        seq.step_len = seq_step_samples(p);
        seq.step = (seq.step + 1) % (p->steps ? p->steps : 1);
        seq_fire(p, seq.step);
    }
}

static void seq_start(void) { seq.step = -1; seq.otter_held = -1; seq.fox_held = -1; seq.playing = 1; }
static void seq_stop(void)
{
    seq.playing = 0;
    if (seq.otter_held >= 0) { otter.note(seq.otter_held, 0, 0); seq.otter_held = -1; }
    if (seq.fox_held >= 0) { fox.note(seq.fox_held, 0, 0); seq.fox_held = -1; }
    seq.step = -1;
}

static void seq_record(pattern_t *p, uint8_t e, int32_t note, uint32_t vel)
{
    int32_t st;
    if (!seq.rec || !seq.playing || seq.step < 0 || note < 0) return;
    st = seq.step;
    if (seq.phase > seq_this_len(p, st) / 2u) st = (st + 1) % (p->steps ? p->steps : 1);
    if (e == E_OTTER) { p->otter_note[st] = (uint8_t)note; p->otter_vel[st] = (uint8_t)vel; }
    else if (e == E_CAT) { if (note < 16) { p->cat_mask[st] |= (uint16_t)(1u << note); p->cat_vel[st] = (uint8_t)vel; } }
    else if (e == E_FOX) { p->fox_note[st] = (uint8_t)note; p->fox_vel[st] = (uint8_t)vel; }
    else { p->hop_note[st] = (uint8_t)note; p->hop_vel[st] = (uint8_t)vel; }
}

static void seq_clear_lane(pattern_t *p, uint8_t e)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        if (e == E_OTTER) p->otter_note[i] = 0;
        else if (e == E_CAT) p->cat_mask[i] = 0;
        else if (e == E_FOX) p->fox_note[i] = 0;
        else p->hop_note[i] = 255;
    }
}

static int seq_lane_has(const pattern_t *p, uint8_t e, uint32_t st)
{
    return e == E_OTTER ? p->otter_note[st] != 0 : e == E_CAT ? p->cat_mask[st] != 0 : e == E_FOX ? p->fox_note[st] != 0 : p->hop_note[st] != 255u;
}

/* the Beaver's keys: toggle a step of lane e (Otter / Fox: the last note played; Cat: the last pad; Hopper: the last slice) */
static void seq_toggle(pattern_t *p, uint8_t e, uint32_t st, int32_t last_note, int last_pad, int last_slice)
{
    if (st >= NSTEP) return;
    if (e == E_OTTER) p->otter_note[st] = p->otter_note[st] ? 0 : (uint8_t)(last_note > 0 && last_note < 120 ? last_note : 48);
    else if (e == E_CAT) p->cat_mask[st] ^= (uint16_t)(1u << (last_pad >= 0 ? last_pad : 0));
    else if (e == E_FOX) p->fox_note[st] = p->fox_note[st] ? 0 : (uint8_t)(last_note > 0 && last_note < 120 ? last_note : 36);
    else p->hop_note[st] = p->hop_note[st] != 255u ? 255u : (uint8_t)(last_slice >= 0 ? last_slice : 0);
}
