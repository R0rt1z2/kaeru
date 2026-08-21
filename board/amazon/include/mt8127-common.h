//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <board_ops.h>

#if defined(CONFIG_AMAZON_AUSTIN)
#include "mt8127-austin.h"
#elif defined(CONFIG_AMAZON_FORD)
#include "mt8127-ford.h"
#else
#error "Invalid device selection"
#endif

#define PL_BOOTARG_MAGIC        0x504C504C  // 'PLPL'
#define RTC_PDN1                0x802C
#define RTC_PDN1_FAST_BOOT      0x2000
#define RTC_PDN1_RECOVERY_MASK  0x0030

// LK functions
static inline void pwrap_read(uint32_t reg, uint32_t *val) {
    ((void (*)(uint32_t, uint32_t *))(PWRAP_READ_FUNC_ADDR|1))(reg, val);
}

static inline void pwrap_write(uint32_t reg, uint32_t val) {
    ((void (*)(uint32_t, uint32_t))(PWRAP_WRITE_FUNC_ADDR|1))(reg, val);
}

static inline void rtc_writeif_unlock(void) {
    ((void (*)(void))(RTC_WRITEIF_UNLOCK_FUNC_ADDR|1))();
}

static inline void rtc_write_trigger(void) {
    ((void (*)(void))(RTC_WRITE_TRIGGER_FUNC_ADDR|1))();
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

// Optional hooks that a device can implement if required.
#ifdef HAVE_EARLY_INIT
void device_early_init(void);
#endif

#ifdef HAVE_LATE_INIT
void device_late_init(void);
#endif
