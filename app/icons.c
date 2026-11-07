/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * Three little animals, 12 x 12, one bit a pixel. Part of app.c. */
static const uint16_t ICON_OTTER[12] = {   /* round head, small ears, whiskers */
    0x0000, 0x0330, 0x07F8, 0x0FFC, 0x1FFE, 0x1B6E, 0x1FFE, 0x7CBE, 0x1FFE, 0x0FFC, 0x07F8, 0x0000};
static const uint16_t ICON_CAT[12] = {     /* pointed ears, wide face */
    0x0000, 0x0C0C, 0x0E1C, 0x0F3C, 0x0FFC, 0x1FFE, 0x1B6E, 0x1FFE, 0x1E7E, 0x0FFC, 0x03C0, 0x0000};
static const uint16_t ICON_FOX[12] = {     /* tall ears, narrow muzzle */
    0x0000, 0x180C, 0x1C1C, 0x1E3C, 0x1FFC, 0x1FFC, 0x1B6C, 0x0FF8, 0x07F0, 0x03E0, 0x01C0, 0x0000};
static const uint16_t *const ICONS[3] = {ICON_OTTER, ICON_CAT, ICON_FOX};

static void icon(int32_t x, int32_t y, const uint16_t *bits, uint16_t col, int32_t scale)
{
    int32_t r, c;
    for (r = 0; r < 12; r++)
        for (c = 0; c < 12; c++)
            if (bits[r] >> (12 - c) & 1u)
                cv_rect(x + c * scale, y + r * scale, scale, scale, col);
}
