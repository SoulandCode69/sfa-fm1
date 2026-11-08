/* SPDX-License-Identifier: GPL-3.0-only
 * SFA ONE on the host: renders every preset (a note, then a chord) to WAV files, and the per-preset peak / RMS.
 *   cc -O2 -o one_render one_render.c sfa_one.c -lm && ./one_render out_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sfa_one.h"
static float h_sin(float x) { return sinf(x); }
static float h_exp2(float x) { return exp2f(x); }
static void wav(const char *path, const float *x, uint32_t n, uint32_t sr)
{
    FILE *f = fopen(path, "wb"); uint32_t i, data = n * 2u, riff = 36u + data; uint16_t one = 1, ch = 1, bits = 16, ba = 2; uint32_t br = sr * 2u;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); { uint32_t sz = 16; fwrite(&sz, 4, 1, f); }
    fwrite(&one, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (i = 0; i < n; i++) { float v = x[i]; int16_t s = (int16_t)((v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v) * 32767.0f); fwrite(&s, 2, 1, f); }
    fclose(f);
}
int main(int argc, char **argv)
{
    static one_t o; one_host_t h = {h_sin, h_exp2, 44100.0f};
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t sr = 44100, secs = 4, n = sr * secs, p, i;
    float *buf = calloc(n, sizeof *buf);
    printf("%-14s %6s %6s %6s\n", "preset", "peak", "rms", "crest");
    for (p = 0; p < ONE_NPRESETS; p++) {
        char path[256]; float pk = 0, e = 0;
        one_init(&o, &h); one_preset_load(&o, p);
        memset(buf, 0, n * sizeof *buf);
        one_note(&o, 48, 110, 1);
        for (i = 0; i < n; i += 256) {
            uint32_t s = i / 256;
            if (s == 1 * sr / 256) one_note(&o, 48, 110, 0);
            if (s == (uint32_t)(1.6f * sr / 256)) { one_note(&o, 60, 100, 1); one_note(&o, 64, 90, 1); one_note(&o, 67, 90, 1); }
            if (s == (uint32_t)(2.8f * sr / 256)) { one_note(&o, 60, 0, 0); one_note(&o, 64, 0, 0); one_note(&o, 67, 0, 0); }
            one_render(&o, buf + i, n - i < 256 ? n - i : 256);
        }
        for (i = 0; i < n; i++) { float a = fabsf(buf[i]); if (a > pk) pk = a; e += buf[i] * buf[i]; }
        e = sqrtf(e / n);
        snprintf(path, sizeof path, "%s/one_%02u_%s.wav", dir, p, ONE_PRESETS[p].name);
        for (i = 0; path[i]; i++) if (path[i] == ' ') path[i] = '_';
        wav(path, buf, n, sr);
        printf("%-14s %6.1f %6.1f %6.1f\n", ONE_PRESETS[p].name, 20 * log10f(pk + 1e-9f), 20 * log10f(e + 1e-9f), 20 * log10f(pk + 1e-9f) - 20 * log10f(e + 1e-9f));
    }
    free(buf);
    return 0;
}
