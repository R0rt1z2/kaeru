//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <board_ops.h>

#if defined(CONFIG_AMAZON_CUPCAKE)
#include "mt8516-cupcake.h"
#elif defined(CONFIG_AMAZON_DONUT)
#include "mt8516-donut.h"
#else
#error "Invalid device selection"
#endif

// LK functions
static inline void cmd_flash(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_FLASH_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_erase(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_ERASE_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_REBOOT_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_oem_reboot_recovery(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_REBOOT_RECOVERY_FUNC_ADDR|1))(arg, data, sz);
}

static inline const char* get_boot_part(void) {
    return ((const char* (*)(void))(GET_BOOT_PART_FUNC_ADDR|1))();
}

static inline void cmdline_append(const char *arg) {
    ((void (*)(const char *))(CMDLINE_APPEND_FUNC_ADDR|1))(arg);
}

static inline void mdelay(unsigned long msecs) {
    ((void (*)(unsigned long))(MDELAY_FUNC_ADDR|1))(msecs);
}

static inline uint32_t pmic_read(uint32_t reg) {
    return ((uint32_t (*)(uint32_t))(PMIC_READ_FUNC_ADDR|1))(reg);
}

static inline void pmic_write(uint32_t reg, uint32_t val) {
    ((void (*)(uint32_t, uint32_t))(PMIC_WRITE_FUNC_ADDR|1))(reg, val);
}

static inline void rtc_writeif_unlock(void) {
    ((void (*)(void))(RTC_WRITEIF_UNLOCK_FUNC_ADDR|1))();
}

static inline void rtc_write_trigger(void) {
    ((void (*)(void))(RTC_WRITE_TRIGGER_FUNC_ADDR|1))();
}

static inline bool mtk_detect_pmic_just_rst(void) {
    return ((bool (*)(void))(MTK_DETECT_PMIC_JUST_RST_ADDR|1))();
}

static inline int idme_boot_mode(void) {
    return ((int (*)(void))(IDME_BOOT_MODE_FUNC_ADDR|1))();
}

// Optional hooks that a device can implement if required.
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

#ifdef HAVE_BOOT_KEYS
void device_boot_keys(bool *up, bool *down);
#endif
