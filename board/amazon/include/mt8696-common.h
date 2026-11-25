//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <board_ops.h>

#if defined(CONFIG_AMAZON_KARAT)
#include "mt8696-karat.h"
#elif defined(CONFIG_AMAZON_KARA)
#include "mt8696-kara.h"
#else
#error "Invalid device selection"
#endif

#define BOOT_REASON_POWER_KEY 0

#define ENV_DISABLE_BOOTMENU "disable_bootmenu"

static inline void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(VIDEO_SET_CURSOR_FUNC_ADDR|1))(row, col);
}

static inline int video_get_rows(void) {
    return ((int (*)(void))(VIDEO_GET_ROWS_FUNC_ADDR|1))();
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

#ifdef BOOTIMG_SET_BASE_FUNC_ADDR
static inline void bootimg_set_base(uint32_t base) {
    ((void (*)(uint32_t))(BOOTIMG_SET_BASE_FUNC_ADDR|1))(base);
}
#endif

#ifdef BOOTIMG_PARSE_BOOTOPT_FUNC_ADDR
static inline void bootimg_parse_bootopt(const char *cmdline) {
    ((void (*)(const char *))(BOOTIMG_PARSE_BOOTOPT_FUNC_ADDR|1))(cmdline);
}
#endif

#ifdef BOOT_LINUX_MACH_TYPE
static inline void boot_linux(uint32_t kernel, uint32_t tags, const char *cmdline,
                              uint32_t ramdisk, uint32_t ramdisk_sz) {
    ((void (*)(uint32_t, uint32_t, const char *, uint32_t, uint32_t, uint32_t))
            (BOOT_LINUX_FUNC_ADDR|1))(kernel, tags, cmdline, BOOT_LINUX_MACH_TYPE,
                                      ramdisk, ramdisk_sz);
}
#else
static inline void boot_linux(void) {
    ((void (*)(void))(BOOT_LINUX_FUNC_ADDR|1))();
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
