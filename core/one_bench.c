#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "sfa_one.h"
static float h_sin(float x) { return sinf(x); }
static float h_exp2(float x) { return exp2f(x); }
int main(void)
{
    static one_t o; one_host_t h = {h_sin, h_exp2, 44100.0f}; static float buf[256];
    uint32_t i, blocks = 44100 * 10 / 256;
    struct timespec a, b;
    one_init(&o, &h); one_preset_load(&o, 7);
    one_note(&o, 48, 100, 1); one_note(&o, 55, 100, 1); one_note(&o, 60, 100, 1); one_note(&o, 64, 100, 1);
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < blocks; i++) { memset(buf, 0, sizeof buf); one_render(&o, buf, 256); }
    clock_gettime(CLOCK_MONOTONIC, &b);
    { double t = (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9; printf("4 voices, 10 s of audio: %.3f s host CPU = %.2f%% of real time (%.1f us a block)\n", t, t / 10.0 * 100, t / blocks * 1e6); }
    return 0;
}
