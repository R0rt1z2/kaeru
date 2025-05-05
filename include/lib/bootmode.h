//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

typedef enum {
    BOOTMODE_NORMAL = 0,
    BOOTMODE_META = 1,
    BOOTMODE_RECOVERY = 2,
    BOOTMODE_FACTORY = 4,
    BOOTMODE_ADVMETA = 5,
    BOOTMODE_ATEFACT = 6,
    BOOTMODE_ALARM = 7,
    BOOTMODE_POWEROFF_CHARGING = 9,
    BOOTMODE_FASTBOOT = 99,
    BOOTMODE_ERECOVERY = 101
} bootmode_t;

extern const char* bootmode2str(bootmode_t mode);
extern void show_bootmode(bootmode_t mode);
extern bootmode_t get_bootmode(void);
extern void set_bootmode(bootmode_t mode);