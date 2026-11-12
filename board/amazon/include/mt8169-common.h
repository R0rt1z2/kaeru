//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <board_ops.h>

#if defined(CONFIG_AMAZON_RASPITE)
#include "mt8169-raspite.h"
#elif defined(CONFIG_AMAZON_TUNGSTEN)
#include "mt8169-tungsten.h"
#else
#error "Invalid device selection"
#endif

static inline void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(VIDEO_SET_CURSOR_FUNC_ADDR|1))(row, col);
}

static inline int video_get_rows(void) {
    return ((int (*)(void))(VIDEO_GET_ROWS_FUNC_ADDR|1))();
}

static inline void mdelay(unsigned ms) {
    ((void (*)(unsigned))(MDELAY_FUNC_ADDR|1))(ms);
}

static inline void mt_power_off(void) {
    ((void (*)(void))(MT_POWER_OFF_FUNC_ADDR|1))();
}

static inline void cmdline_append(const char *arg) {
    ((void (*)(const char *))(CMDLINE_APPEND_FUNC_ADDR|1))(arg);
}

static inline char *get_cmdline(void) {
    return ((char *(*)(void))(CMDLINE_GET_FUNC_ADDR|1))();
}

static inline void cmd_flash(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_FLASH_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_erase(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_ERASE_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_REBOOT_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_REBOOT_BOOTLOADER_FUNC_ADDR|1))(arg, data, sz);
}

#ifdef HAVE_EARLY_INIT
void device_early_init(void);
#endif

#ifdef HAVE_LATE_INIT
void device_late_init(void);
#endif

#ifdef HAVE_FASTBOOT_INIT
void device_fastboot_init(void);
#endif
