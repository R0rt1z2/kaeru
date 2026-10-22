//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <board_ops.h>

#if defined(CONFIG_AMAZON_CHECKERS)
#include "mt8163-checkers.h"
#elif defined(CONFIG_AMAZON_CROWN)
#include "mt8163-crown.h"
#elif defined(CONFIG_AMAZON_CRONOS)
#include "mt8163-cronos.h"
#elif defined(CONFIG_AMAZON_BISCUIT)
#include "mt8163-biscuit.h"
#elif defined(CONFIG_AMAZON_RADAR)
#include "mt8163-radar.h"
#else
#error "Invalid device selection"
#endif

#define UNLOCK_CHECK_PATTERN 0xB510, 0xB0C0, 0x2100, 0xF44F, 0x7280, 0x4668

#ifndef KERNEL_CMDLINE_SIZE
#define KERNEL_CMDLINE_SIZE 0x400
#endif

#define LK_IDME_REGISTER_FUNC_ADDR 0x4BD008E0

#ifdef HAVE_DISPLAY
static inline void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(VIDEO_SET_CURSOR_FUNC_ADDR|1))(row, col);
}

static inline int video_get_rows(void) {
    return ((int (*)(void))(VIDEO_GET_ROWS_FUNC_ADDR|1))();
}
#endif

#ifdef MDELAY_FUNC_ADDR
static inline void mdelay(unsigned long msecs) {
    ((void (*)(unsigned long))(MDELAY_FUNC_ADDR|1))(msecs);
}
#endif

#ifdef THREAD_SLEEP_FUNC_ADDR
static inline void thread_sleep(unsigned long msecs) {
    ((void (*)(unsigned long))(THREAD_SLEEP_FUNC_ADDR|1))(msecs);
}
#endif

#ifdef CONFIG_AMAZON_MT8163_AB
static inline const char* get_boot_part(int slot) {
    return ((const char* (*)(int))(GET_BOOT_PART_FUNC_ADDR|1))(slot);
}
#endif

#ifdef HAVE_EARLY_INIT
void device_early_init(void);
#endif

#ifdef HAVE_LATE_INIT
void device_late_init(void);
#endif

#ifdef HAVE_FASTBOOT_INIT
void device_fastboot_init(void);
#endif

#ifdef HAVE_FASTBOOT_CMD_REBOOT
void device_fastboot_cmd_reboot(const char *arg, void *data, unsigned sz);
#endif
