//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <board_ops.h>

#if defined(CONFIG_AMAZON_MAVERICK)
#include "mt8183-maverick.h"
#elif defined(CONFIG_AMAZON_ATHENA)
#include "mt8183-athena.h"
#elif defined(CONFIG_AMAZON_TRONA)
#include "mt8183-trona.h"
#else
#error "Invalid device selection"
#endif

#define UNLOCK_CHECK_PATTERN 0xB530, 0xB0C3, 0x4C12, 0xAD01, 0x2100, 0xF44F, 0x7280, 0x4628

static inline void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(VIDEO_SET_CURSOR_FUNC_ADDR|1))(row, col);
}

static inline int video_get_rows(void) {
    return ((int (*)(void))(VIDEO_GET_ROWS_FUNC_ADDR|1))();
}

#ifdef MT_GET_GPIO_IN_FUNC_ADDR
static inline int mt_get_gpio_in(uint32_t pin) {
    return ((int (*)(uint32_t))(MT_GET_GPIO_IN_FUNC_ADDR|1))(pin);
}
#endif

#ifdef KPOC_DETECT_FUNC_ADDR
static inline bool kernel_power_off_charging_detection(void) {
    return ((bool (*)(void))(KPOC_DETECT_FUNC_ADDR|1))();
}
#endif

static inline int bldr_load_dtb(const char *part) {
    return ((int (*)(const char *))(LOAD_DTB_FUNC_ADDR|1))(part);
}

static inline int recovery_dtbo_applied(void) {
    return ((int (*)(void))(RECOVERY_DTBO_APPLIED_FUNC_ADDR|1))();
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

#ifdef HAVE_FASTBOOT_CMD_REBOOT
void device_fastboot_cmd_reboot(const char *arg, void *data, unsigned sz);
#endif
