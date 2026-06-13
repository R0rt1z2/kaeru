
//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/string.h>
#include <lib/recovery.h>

bootmode_t misc_command_to_bootmode(const char* c) {
    if (!c || !c[0])
        return BOOTMODE_NORMAL;

    if (!strncmp(c, "boot-recovery",       13)) return BOOTMODE_RECOVERY;
    if (!strncmp(c, "boot-bootloader",     15)) return BOOTMODE_FASTBOOT;
    if (!strncmp(c, "bootonce-bootloader", 19)) return BOOTMODE_FASTBOOT;
    return BOOTMODE_NORMAL;
}