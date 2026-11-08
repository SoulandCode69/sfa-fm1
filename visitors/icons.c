/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * Five visitors drawn as smooth cartoon vector art: ellipses and triangles in a 64-unit box, a bold dark
 * outline, a shaded body, a cream muzzle, shiny eyes and pink cheeks. Rendered with 2x2 supersampling at any
 * pixel size (anti-aliased against the given background), cached so a page redraw costs a blit. Part of app.c. */
typedef struct { uint8_t id; } icon_t;
static const icon_t ICON_OTTER = {0}, ICON_CAT = {1}, ICON_FOX = {2}, ICON_HOPPER = {3}, ICON_BEAVER = {4};
static const icon_t *const ICONS[5] = {&ICON_OTTER, &ICON_CAT, &ICON_FOX, &ICON_HOPPER, &ICON_BEAVER};

/* a shape: ELL cx cy rx ry  |  TRI x0 y0 x1 y1 x2 y2 ; paint index; outlined or not */
enum { ELL, TRI };
enum { P_BODY, P_DARK, P_LITE, P_CREAM, P_WHITE, P_BLACK, P_PINK, P_NOSE, P_IRIS, P_GOLD, P_BLUE, P_SILVER, P_OUT, NPAINT };
typedef struct { uint8_t kind, paint, outline; int8_t a, b, c, d, e, f; } shape_t;

/* every visitor, back to front. Eyes: a black pupil, a white glint. */
static const shape_t SH_OTTER[] = {                                                  /* the NORDIC: golden hair, a calm pale face, blue eyes */
    {ELL, P_DARK, 1, 32, 30, 23, 21, 0, 0},                                          /* hair, shaded */
    {ELL, P_BODY, 0, 31, 28, 22, 20, 0, 0},
    {ELL, P_LITE, 0, 24, 18, 9, 5, 0, 0},
    {ELL, P_DARK, 1, 32, 42, 16, 20, 0, 0},                                          /* the face's outline */
    {ELL, P_CREAM, 0, 32, 41, 15, 19, 0, 0},
    {ELL, P_BODY, 0, 14, 40, 5, 13, 0, 0}, {ELL, P_BODY, 0, 50, 40, 5, 13, 0, 0},     /* hair down the sides */
    {ELL, P_PINK, 0, 22, 48, 4, 2, 0, 0}, {ELL, P_PINK, 0, 42, 48, 4, 2, 0, 0},
    {ELL, P_BLUE, 0, 25, 39, 4, 5, 0, 0}, {ELL, P_BLUE, 0, 39, 39, 4, 5, 0, 0},
    {ELL, P_BLACK, 0, 25, 39, 2, 3, 0, 0}, {ELL, P_BLACK, 0, 39, 39, 2, 3, 0, 0},
    {ELL, P_WHITE, 0, 26, 37, 1, 1, 0, 0}, {ELL, P_WHITE, 0, 40, 37, 1, 1, 0, 0},
    {ELL, P_NOSE, 0, 32, 52, 4, 1, 0, 0},
};
static const shape_t SH_CAT[] = {                                                    /* the MANTIS: a triangular head, compound eyes, antennae */
    {TRI, P_DARK, 1, 12, 8, 20, 22, 10, 20}, {TRI, P_DARK, 1, 52, 8, 44, 22, 54, 20},   /* antennae */
    {TRI, P_DARK, 1, 6, 22, 58, 22, 32, 56},                                         /* the head, shaded */
    {TRI, P_BODY, 0, 8, 22, 56, 22, 32, 54},
    {TRI, P_LITE, 0, 20, 24, 44, 24, 32, 34},
    {ELL, P_BLACK, 1, 15, 27, 8, 9, 0, 0}, {ELL, P_BLACK, 1, 49, 27, 8, 9, 0, 0},     /* compound eyes */
    {ELL, P_IRIS, 0, 14, 26, 5, 6, 0, 0}, {ELL, P_IRIS, 0, 48, 26, 5, 6, 0, 0},
    {ELL, P_WHITE, 0, 13, 24, 2, 2, 0, 0}, {ELL, P_WHITE, 0, 47, 24, 2, 2, 0, 0},
    {TRI, P_NOSE, 0, 27, 44, 32, 50, 26, 50}, {TRI, P_NOSE, 0, 37, 44, 32, 50, 38, 50},   /* mandibles */
    {ELL, P_PINK, 0, 22, 38, 3, 2, 0, 0}, {ELL, P_PINK, 0, 42, 38, 3, 2, 0, 0},
};
static const shape_t SH_FOX[] = {                                                    /* the REPTILIAN: a crest, a snout, yellow slit eyes */
    {TRI, P_DARK, 1, 22, 20, 28, 4, 32, 20}, {TRI, P_DARK, 1, 30, 18, 36, 2, 40, 18}, {TRI, P_DARK, 1, 38, 20, 46, 6, 48, 22},   /* the crest */
    {TRI, P_BODY, 0, 23, 20, 28, 7, 31, 20}, {TRI, P_BODY, 0, 31, 18, 36, 5, 39, 18}, {TRI, P_BODY, 0, 39, 20, 46, 9, 47, 22},
    {ELL, P_DARK, 1, 32, 36, 23, 18, 0, 0},
    {ELL, P_BODY, 0, 31, 34, 22, 17, 0, 0},
    {ELL, P_LITE, 0, 24, 27, 9, 4, 0, 0},
    {ELL, P_LITE, 0, 32, 47, 15, 9, 0, 0},                                           /* the snout */
    {ELL, P_NOSE, 0, 25, 45, 1, 1, 0, 0}, {ELL, P_NOSE, 0, 39, 45, 1, 1, 0, 0},
    {ELL, P_GOLD, 0, 22, 34, 6, 5, 0, 0}, {ELL, P_GOLD, 0, 42, 34, 6, 5, 0, 0},
    {ELL, P_BLACK, 0, 22, 34, 1, 4, 0, 0}, {ELL, P_BLACK, 0, 42, 34, 1, 4, 0, 0},    /* slit pupils */
    {ELL, P_WHITE, 0, 24, 32, 1, 1, 0, 0}, {ELL, P_WHITE, 0, 44, 32, 1, 1, 0, 0},
    {ELL, P_NOSE, 0, 32, 52, 6, 1, 0, 0},
};
static const shape_t SH_HOPPER[] = {                                                 /* the GREY: a big head, huge black eyes, the tiny mouth */
    {ELL, P_DARK, 1, 32, 30, 23, 24, 0, 0},
    {ELL, P_BODY, 0, 31, 28, 22, 23, 0, 0},
    {ELL, P_LITE, 0, 24, 17, 9, 5, 0, 0},
    {ELL, P_DARK, 0, 32, 50, 12, 8, 0, 0},                                           /* the chin, narrow */
    {ELL, P_BODY, 0, 32, 48, 11, 8, 0, 0},
    {ELL, P_BLACK, 1, 21, 35, 9, 6, 0, 0}, {ELL, P_BLACK, 1, 43, 35, 9, 6, 0, 0},     /* the eyes */
    {ELL, P_BLACK, 0, 17, 32, 5, 7, 0, 0}, {ELL, P_BLACK, 0, 47, 32, 5, 7, 0, 0},
    {ELL, P_WHITE, 0, 18, 32, 2, 2, 0, 0}, {ELL, P_WHITE, 0, 44, 32, 2, 2, 0, 0},
    {ELL, P_NOSE, 0, 30, 46, 1, 1, 0, 0}, {ELL, P_NOSE, 0, 34, 46, 1, 1, 0, 0},
    {ELL, P_NOSE, 0, 32, 52, 3, 1, 0, 0},
};
static const shape_t SH_BEAVER[] = {                                                 /* the SHIP: a saucer, a dome, running lights, a beam */
    {TRI, P_LITE, 0, 22, 40, 42, 40, 32, 62},                                        /* the beam */
    {ELL, P_DARK, 1, 32, 36, 28, 9, 0, 0},                                           /* the hull */
    {ELL, P_BODY, 0, 32, 35, 27, 8, 0, 0},
    {ELL, P_DARK, 0, 32, 38, 20, 3, 0, 0},
    {ELL, P_DARK, 1, 32, 28, 13, 11, 0, 0},                                          /* the dome */
    {ELL, P_SILVER, 0, 32, 27, 12, 10, 0, 0},
    {ELL, P_WHITE, 0, 27, 22, 4, 2, 0, 0},
    {ELL, P_GOLD, 0, 12, 36, 2, 2, 0, 0}, {ELL, P_PINK, 0, 22, 39, 2, 2, 0, 0}, {ELL, P_GOLD, 0, 32, 40, 2, 2, 0, 0}, {ELL, P_PINK, 0, 42, 39, 2, 2, 0, 0}, {ELL, P_GOLD, 0, 52, 36, 2, 2, 0, 0},
    {ELL, P_BLACK, 0, 29, 27, 2, 3, 0, 0}, {ELL, P_BLACK, 0, 35, 27, 2, 3, 0, 0},    /* the pilot's eyes in the dome */
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
    uint16_t pal[NPAINT];
    const shape_t *sh = SH[id]; uint32_t n = SHN[id], x, y;
    float unit = 64.0f / (float)size;
    pal[P_BODY] = col;
    pal[P_DARK] = rgb_mix(col, 0, 0, 0, 90);
    pal[P_LITE] = rgb_mix(col, 255, 255, 255, 70);
    pal[P_CREAM] = RGB(245, 232, 226);
    pal[P_WHITE] = RGB(255, 255, 255);
    pal[P_BLACK] = RGB(24, 18, 28);
    pal[P_PINK] = RGB(255, 150, 170);
    pal[P_NOSE] = RGB(48, 30, 44);
    pal[P_IRIS] = RGB(150, 230, 110);
    pal[P_GOLD] = RGB(255, 214, 90);
    pal[P_BLUE] = RGB(90, 160, 255);
    pal[P_SILVER] = RGB(200, 208, 232);
    pal[P_OUT] = RGB(12, 8, 22);
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
