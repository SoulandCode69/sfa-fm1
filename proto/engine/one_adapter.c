/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * SFA ONE on the FM-1: the engine contract around core/sfa_one.c (symlinked beside this file), the platform's fast
 * sin / exp2 handed to it, and the slicer API answered as "nothing here" (the app's Hopper hooks), as the Greys did.
 * The firmware's four knobs are the current page's four parameters; ALGO browses the presets. */
#include "fastmath.h"
#include "engine.h"
#include "sfa_one.h"

static one_t one;
static uint8_t preset_i;
static float h_sin(float x) { return fm_sinf(x); }
static float h_exp2(float x) { return fm_exp2f(x); }

static const char *const TITLES[5] = {"MACROS", "EXCITER", "RESONATOR", "FILTER", "MOTION"};
/* the engine contract carries five pages; SFA ONE has seven. The firmware shows MACROS, EXCITER, RESONATOR, FILTER and
 * MOTION; ENVELOPE and VOICE ride along in the presets (the plugin shows all seven). */
static const uint8_t PAGE_MAP[5] = {0, 1, 2, 3, 5};

static void a_init(void) { one_host_t h = {h_sin, h_exp2, SR}; one_init(&one, &h); one_preset_load(&one, preset_i); }
static void a_note(int32_t note, uint32_t vel, int on) { one_note(&one, note, vel, on); }
static void a_render(float *out, uint32_t n) { one_render(&one, out, n); }
static void a_panic(void) { one_panic(&one); }
static const char *a_text(uint8_t page, uint32_t p, char *buf) { return one_param_text(&one, PAGE_MAP[page < 5u ? page : 0], p, buf); }

const engine_t hopper = {"SFA ONE", a_init, a_note, a_render, a_panic, 5,
                         {ONE_NAMES[0], ONE_NAMES[1], ONE_NAMES[2], ONE_NAMES[3], ONE_NAMES[5]},
                         {one.p[0], one.p[1], one.p[2], one.p[3], one.p[5]}, a_text, TITLES};

/* ---- the slicer's API, answered: presets instead of sources */
const char *hop_source_name(char *buf) { (void)buf; return ONE_PRESETS[preset_i].name; }
int hop_source(int32_t step) { preset_i = (uint8_t)(((int32_t)preset_i + step + (int32_t)ONE_NPRESETS) % (int32_t)ONE_NPRESETS); one_preset_load(&one, preset_i); return 1; }
int hop_source_index(void) { return preset_i; }
void hop_source_set(int s) { preset_i = (uint8_t)((uint32_t)s % ONE_NPRESETS); one_preset_load(&one, preset_i); }
const char *greys_preset_name_at(int32_t off) { return ONE_PRESETS[((int32_t)preset_i + off + (int32_t)ONE_NPRESETS * 4) % (int32_t)ONE_NPRESETS].name; }
int hop_chop_mode(void) { return 0; }
int hop_chops(void) { return 8; }
int hop_chopmode(void) { return 0; }
void hop_chopmode_set(int m) { (void)m; }
void hop_chop_on(int chops) { (void)chops; }
int hop_recording(void) { return 0; }
void hop_record_start(void) {}
void hop_record_stop(void) {}
void hop_record_feed(const float *mix, uint32_t n) { (void)mix; (void)n; }
int hop_save_needed(void) { return 0; }
int hop_save(void) { return 0; }
uint32_t hop_bank_seconds_x10(void) { return 0; }
uint32_t hop_bank_len(void) { return 0; }
int32_t hop_peak(uint32_t from, uint32_t to) { (void)from; (void)to; return 0; }
uint32_t hop_window(uint32_t *a, uint32_t *b) { *a = *b = 0; return 0; }
float hop_playhead(void) { return -1.0f; }
uint32_t hop_slice_points(const uint32_t **pts) { *pts = 0; return 0; }
uint8_t *hop_slice_blob(uint32_t *size) { static uint8_t z[64]; *size = sizeof z; return z; }
void hop_slices_default(void) {}
int hop_last_slice(void) { return 0; }
void hop_sync(void) {}
void hop_rechop(void) {}
void hop_slice_nudge(uint32_t idx, int32_t delta) { (void)idx; (void)delta; }
int hop_slice_active(uint32_t idx) { (void)idx; return 0; }
