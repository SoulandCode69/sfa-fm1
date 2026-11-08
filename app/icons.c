/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * Five little animals drawn as smooth cartoon vector art: ellipses and triangles in a 64-unit box, a bold dark
 * outline, a shaded body, a cream muzzle, shiny eyes and pink cheeks. Rendered with 2x2 supersampling at any
 * pixel size (anti-aliased against the given background), cached so a page redraw costs a blit. Part of app.c. */
typedef struct { uint8_t id; } icon_t;
static const icon_t ICON_OTTER = {0}, ICON_CAT = {1}, ICON_FOX = {2}, ICON_HOPPER = {3}, ICON_BEAVER = {4};
static const icon_t *const ICONS[5] = {&ICON_OTTER, &ICON_CAT, &ICON_FOX, &ICON_HOPPER, &ICON_BEAVER};

/* a shape: ELL cx cy rx ry  |  TRI x0 y0 x1 y1 x2 y2 ; paint index; outlined or not */
enum { ELL, TRI };
enum { P_BODY, P_DARK, P_LITE, P_CREAM, P_WHITE, P_BLACK, P_PINK, P_NOSE, P_IRIS, P_OUT };
typedef struct { uint8_t kind, paint, outline; int8_t a, b, c, d, e, f; } shape_t;

/* every creature, back to front. Eyes: a black pupil, a white glint. */
static const shape_t SH_OTTER[] = {
    {ELL, P_DARK, 1, 14, 21, 7, 7, 0, 0}, {ELL, P_DARK, 1, 50, 21, 7, 7, 0, 0},       /* ears */
    {ELL, P_PINK, 0, 14, 21, 3, 3, 0, 0}, {ELL, P_PINK, 0, 50, 21, 3, 3, 0, 0},
    {ELL, P_DARK, 1, 32, 37, 23, 21, 0, 0},                                          /* head: shade, then light */
    {ELL, P_BODY, 0, 31, 35, 22, 20, 0, 0},
    {ELL, P_LITE, 0, 26, 27, 10, 6, 0, 0},                                           /* a highlight */
    {ELL, P_CREAM, 0, 32, 45, 15, 11, 0, 0},                                         /* muzzle */
    {ELL, P_PINK, 0, 15, 43, 5, 3, 0, 0}, {ELL, P_PINK, 0, 49, 43, 5, 3, 0, 0},       /* cheeks */
    {ELL, P_NOSE, 0, 32, 39, 6, 4, 0, 0},
    {ELL, P_NOSE, 0, 32, 46, 1, 4, 0, 0},
    {ELL, P_BLACK, 0, 23, 31, 4, 4, 0, 0}, {ELL, P_BLACK, 0, 41, 31, 4, 4, 0, 0},
    {ELL, P_WHITE, 0, 24, 29, 2, 2, 0, 0}, {ELL, P_WHITE, 0, 42, 29, 2, 2, 0, 0},
};
static const shape_t SH_CAT[] = {
    {TRI, P_BODY, 1, 10, 32, 15, 6, 29, 24},                                         /* ears */
    {TRI, P_BODY, 1, 54, 32, 49, 6, 35, 24},
    {TRI, P_PINK, 0, 14, 29, 16, 12, 26, 25}, {TRI, P_PINK, 0, 50, 29, 48, 12, 38, 25},
    {ELL, P_DARK, 1, 32, 40, 24, 20, 0, 0},
    {ELL, P_BODY, 0, 31, 38, 23, 19, 0, 0},
    {ELL, P_LITE, 0, 24, 29, 10, 5, 0, 0},
    {ELL, P_CREAM, 0, 24, 48, 10, 7, 0, 0}, {ELL, P_CREAM, 0, 40, 48, 10, 7, 0, 0},   /* cheeks */
    {ELL, P_PINK, 0, 13, 44, 4, 3, 0, 0}, {ELL, P_PINK, 0, 51, 44, 4, 3, 0, 0},
    {ELL, P_IRIS, 0, 22, 37, 5, 6, 0, 0}, {ELL, P_IRIS, 0, 42, 37, 5, 6, 0, 0},       /* green eyes, slit pupils */
    {ELL, P_BLACK, 0, 22, 37, 2, 5, 0, 0}, {ELL, P_BLACK, 0, 42, 37, 2, 5, 0, 0},
    {ELL, P_WHITE, 0, 23, 34, 2, 2, 0, 0}, {ELL, P_WHITE, 0, 43, 34, 2, 2, 0, 0},
    {TRI, P_PINK, 0, 28, 44, 36, 44, 32, 48},                                        /* nose */
};
static const shape_t SH_FOX[] = {
    {TRI, P_BODY, 1, 6, 36, 11, 4, 28, 26}, {TRI, P_BODY, 1, 58, 36, 53, 4, 36, 26},  /* big ears */
    {TRI, P_DARK, 0, 11, 32, 13, 12, 25, 27}, {TRI, P_DARK, 0, 53, 32, 51, 12, 39, 27},
    {ELL, P_DARK, 1, 32, 40, 25, 19, 0, 0},
    {ELL, P_BODY, 0, 31, 38, 24, 18, 0, 0},
    {ELL, P_LITE, 0, 24, 30, 10, 5, 0, 0},
    {ELL, P_CREAM, 0, 20, 48, 11, 9, 0, 0}, {ELL, P_CREAM, 0, 44, 48, 11, 9, 0, 0},   /* white cheeks */
    {ELL, P_CREAM, 0, 32, 50, 9, 8, 0, 0},
    {ELL, P_BLACK, 0, 22, 36, 4, 4, 0, 0}, {ELL, P_BLACK, 0, 42, 36, 4, 4, 0, 0},
    {ELL, P_WHITE, 0, 23, 34, 2, 2, 0, 0}, {ELL, P_WHITE, 0, 43, 34, 2, 2, 0, 0},
    {ELL, P_NOSE, 0, 32, 46, 5, 4, 0, 0},
};
static const shape_t SH_HOPPER[] = {
    {ELL, P_DARK, 1, 18, 24, 10, 10, 0, 0}, {ELL, P_DARK, 1, 46, 24, 10, 10, 0, 0},   /* eye bumps */
    {ELL, P_BODY, 0, 17, 23, 9, 9, 0, 0}, {ELL, P_BODY, 0, 45, 23, 9, 9, 0, 0},
    {ELL, P_DARK, 1, 32, 42, 25, 17, 0, 0},
    {ELL, P_BODY, 0, 31, 40, 24, 16, 0, 0},
    {ELL, P_LITE, 0, 26, 33, 10, 4, 0, 0},
    {ELL, P_WHITE, 0, 18, 24, 7, 7, 0, 0}, {ELL, P_WHITE, 0, 46, 24, 7, 7, 0, 0},
    {ELL, P_BLACK, 0, 19, 25, 4, 4, 0, 0}, {ELL, P_BLACK, 0, 47, 25, 4, 4, 0, 0},
    {ELL, P_WHITE, 0, 20, 23, 2, 2, 0, 0}, {ELL, P_WHITE, 0, 48, 23, 2, 2, 0, 0},
    {ELL, P_PINK, 0, 14, 46, 5, 3, 0, 0}, {ELL, P_PINK, 0, 50, 46, 5, 3, 0, 0},
    {ELL, P_NOSE, 0, 32, 46, 15, 6, 0, 0},                                           /* the smile: a crescent */
    {ELL, P_BODY, 0, 32, 43, 15, 5, 0, 0},
    {ELL, P_NOSE, 0, 28, 38, 1, 1, 0, 0}, {ELL, P_NOSE, 0, 36, 38, 1, 1, 0, 0},
};
static const shape_t SH_BEAVER[] = {
    {ELL, P_DARK, 1, 12, 23, 7, 7, 0, 0}, {ELL, P_DARK, 1, 52, 23, 7, 7, 0, 0},
    {ELL, P_BODY, 0, 12, 23, 5, 5, 0, 0}, {ELL, P_BODY, 0, 52, 23, 5, 5, 0, 0},
    {ELL, P_DARK, 1, 32, 37, 23, 21, 0, 0},
    {ELL, P_BODY, 0, 31, 35, 22, 20, 0, 0},
    {ELL, P_LITE, 0, 25, 27, 10, 5, 0, 0},
    {ELL, P_CREAM, 0, 32, 46, 14, 11, 0, 0},
    {ELL, P_PINK, 0, 14, 42, 5, 3, 0, 0}, {ELL, P_PINK, 0, 50, 42, 5, 3, 0, 0},
    {ELL, P_NOSE, 0, 32, 39, 7, 5, 0, 0},
    {ELL, P_WHITE, 0, 29, 50, 3, 5, 0, 0}, {ELL, P_WHITE, 0, 35, 50, 3, 5, 0, 0},     /* the teeth */
    {ELL, P_NOSE, 0, 32, 50, 1, 5, 0, 0},
    {ELL, P_BLACK, 0, 23, 31, 4, 4, 0, 0}, {ELL, P_BLACK, 0, 41, 31, 4, 4, 0, 0},
    {ELL, P_WHITE, 0, 24, 29, 2, 2, 0, 0}, {ELL, P_WHITE, 0, 42, 29, 2, 2, 0, 0},
};
static const shape_t *const SH[5] = {SH_OTTER, SH_CAT, SH_FOX, SH_HOPPER, SH_BEAVER};
static const uint8_t SHN[5] = {sizeof SH_OTTER / sizeof *SH_OTTER, sizeof SH_CAT / sizeof *SH_CAT, sizeof SH_FOX / sizeof *SH_FOX,
                               sizeof SH_HOPPER / sizeof *SH_HOPPER, sizeof SH_BEAVER / sizeof *SH_BEAVER};

#define OUTLINE_W 2.2f
static int in_ell(float x, float y, float cx, float cy, float rx, float ry)
{
    float dx = (x - cx) / rx, dy = (y - cy) / ry;
    return dx * dx + dy * dy <= 1.0f;
}
static int in_tri(float x, float y, float x0, float y0, float x1, float y1, float x2, float y2)
{
    float d0 = (x - x1) * (y0 - y1) - (x0 - x1) * (y - y1), d1 = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2),
          d2 = (x - x0) * (y2 - y0) - (x2 - x0) * (y - y0);
    return !((d0 < 0 || d1 < 0 || d2 < 0) && (d0 > 0 || d1 > 0 || d2 > 0));
}
static int hit(const shape_t *s, float x, float y, float grow)
{
    if (s->kind == ELL) return in_ell(x, y, s->a, s->b, s->c + grow, s->d + grow);
    else {
        float cx = (s->a + s->c + s->e) / 3.0f, cy = (s->b + s->d + s->f) / 3.0f, k = 1.0f + grow / 9.0f;
        return in_tri(x, y, cx + (s->a - cx) * k, cy + (s->b - cy) * k, cx + (s->c - cx) * k, cy + (s->d - cy) * k, cx + (s->e - cx) * k, cy + (s->f - cy) * k);
    }
}

/* colour helpers on RGB565 */
static void rgb_of(uint16_t c, int32_t *r, int32_t *g, int32_t *b) { *r = (c >> 11) * 255 / 31; *g = ((c >> 5) & 63) * 255 / 63; *b = (c & 31) * 255 / 31; }
static uint16_t rgb_mix(uint16_t c, int32_t r2, int32_t g2, int32_t b2, int32_t amt)   /* amt/256 towards (r2,g2,b2) */
{
    int32_t r, g, b; rgb_of(c, &r, &g, &b);
    r += (r2 - r) * amt / 256; g += (g2 - g) * amt / 256; b += (b2 - b) * amt / 256;
    return RGB(r, g, b);
}

/* paint a sample point: the top-most shape that covers it, or -1 */
static int sample(const shape_t *sh, uint32_t n, float x, float y, int pass_outline)
{
    int32_t i;
    for (i = (int32_t)n - 1; i >= 0; i--)
        if (hit(&sh[i], x, y, pass_outline ? OUTLINE_W : 0.0f) && (!pass_outline || sh[i].outline)) return pass_outline ? P_OUT : sh[i].paint;
    return -1;
}

/* the cache: a few rendered icons (id, size, body colour, background) */
#define ICACHE 10
#define ICON_MAX 40
typedef struct { uint8_t id, size; uint16_t col, bg; uint32_t age; } ikey_t;
static ikey_t icache_key[ICACHE];
PLAT_POOL static uint16_t icache_px[ICACHE][ICON_MAX * ICON_MAX];
static uint32_t icache_clock;

static void icon_render(uint16_t *px, uint8_t id, uint32_t size, uint16_t col, uint16_t bg)
{
    uint16_t pal[10];
    const shape_t *sh = SH[id]; uint32_t n = SHN[id], x, y;
    float unit = 64.0f / (float)size;
    pal[P_BODY] = col;
    pal[P_DARK] = rgb_mix(col, 0, 0, 0, 90);
    pal[P_LITE] = rgb_mix(col, 255, 255, 255, 70);
    pal[P_CREAM] = RGB(255, 243, 226);
    pal[P_WHITE] = RGB(255, 255, 255);
    pal[P_BLACK] = RGB(24, 18, 28);
    pal[P_PINK] = RGB(255, 150, 170);
    pal[P_NOSE] = RGB(48, 30, 44);
    pal[P_IRIS] = RGB(150, 230, 110);
    pal[P_OUT] = RGB(22, 16, 26);
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++) {
            int32_t r = 0, g = 0, b = 0, cr, cg, cb; uint32_t k;
            for (k = 0; k < 4u; k++) {
                float sx = ((float)x + 0.25f + 0.5f * (float)(k & 1u)) * unit, sy = ((float)y + 0.25f + 0.5f * (float)(k >> 1)) * unit;
                int p = sample(sh, n, sx, sy, 0);
                if (p < 0) p = sample(sh, n, sx, sy, 1);
                rgb_of(p < 0 ? bg : pal[p], &cr, &cg, &cb);
                r += cr; g += cg; b += cb;
            }
            px[y * size + x] = RGB(r / 4, g / 4, b / 4);
        }
}

static const uint16_t *icon_get(uint8_t id, uint32_t size, uint16_t col, uint16_t bg)
{
    uint32_t i, old = 0;
    for (i = 0; i < ICACHE; i++)
        if (icache_key[i].age && icache_key[i].id == id && icache_key[i].size == size && icache_key[i].col == col && icache_key[i].bg == bg) {
            icache_key[i].age = ++icache_clock; return icache_px[i];
        }
    for (i = 1; i < ICACHE; i++) if (icache_key[i].age < icache_key[old].age) old = i;
    icache_key[old].id = id; icache_key[old].size = (uint8_t)size; icache_key[old].col = col; icache_key[old].bg = bg; icache_key[old].age = ++icache_clock;
    icon_render(icache_px[old], id, size, col, bg);
    return icache_px[old];
}

/* draw a creature with its box size px (16 .. ICON_MAX) at x, y over the background bg */
static void icon_px(int32_t x, int32_t y, const icon_t *ic, uint32_t size, uint16_t col, uint16_t bg)
{
    const uint16_t *px; uint32_t r, c;
    if (size > ICON_MAX) size = ICON_MAX;
    if (size < 8u) size = 8u;
    px = icon_get(ic->id, size, col, bg);
    for (r = 0; r < size; r++) for (c = 0; c < size; c++) cv_pset(x + (int32_t)c, y + (int32_t)r, px[r * size + c]);
}
/* the old entry points: scale 1 = 16 px, 2 = 32 px; outline colour doubles as the background */
static void icon_o(int32_t x, int32_t y, const icon_t *ic, uint16_t col, int32_t scale, uint16_t outline, int do_outline)
{
    (void)do_outline;
    icon_px(x, y, ic, (uint32_t)scale * 16u, col, outline);
}
static void icon(int32_t x, int32_t y, const icon_t *ic, uint16_t col, int32_t scale) { icon_o(x, y, ic, col, scale, C_BG, 1); }
