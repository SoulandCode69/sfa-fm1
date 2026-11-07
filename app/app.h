/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sin Fetish Audio
 * SFA Suite for the M-VAVE FM-1, on fm1-base (MPL-2.0, Charles Vestal) / felucca-bsp. */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "SFA"
#define APP_SPLASH_RGB RGB(255, 64, 160)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity from the keys (lib/touch.h) */

/* the store: settings, the panel calibration (the platform's) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

/* backup and restore over USB (device/backup.c): the settings */
#define APP_BACKUP_OBJS {OBJ_SETTINGS}
#define APP_BACKUP_MAGIC 0x31414653u     /* "SFA1" */
#define APP_BACKUP_VERSION 1
#define APP_BACKUP_WHAT "replaces the settings"
int app_backup_playing(void);
void app_backup_flush(void);
void app_backup_hold(int on);
void app_backup_restored(void);

/* the resource region: Fox's sample bank, in PLAT_XDATA1 (0xE5000..0xFC000: 23 sectors, 92 KiB) */
#define APP_RES_BASE PLAT_XDATA1_LO
#define APP_RES_SECTORS 23u

/* the contract: the platform calls these */
void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
