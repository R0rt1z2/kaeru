//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/bootmode.h>
#include <lib/common.h>
#include <lib/debug.h>

const char* bootmode2str(bootmode_t mode) {
    switch (mode) {
        case BOOTMODE_NORMAL:
            return "NORMAL";
        case BOOTMODE_META:
            return "META";
        case BOOTMODE_RECOVERY:
            return "RECOVERY";
        case BOOTMODE_FACTORY:
            return "FACTORY";
        case BOOTMODE_ADVMETA:
            return "ADVMETA";
        case BOOTMODE_ATEFACT:
            return "ATEFACT";
        case BOOTMODE_ALARM:
            return "ALARM";
        case BOOTMODE_POWEROFF_CHARGING:
            return "POWEROFF CHARGING";
        case BOOTMODE_FASTBOOT:
            return "FASTBOOT";
        case BOOTMODE_ERECOVERY:
            return "ERECOVERY";
        default:
            return "UNKNOWN";
    }
}

bootmode_t get_bootmode(void) {
    return READ32(CONFIG_BOOTMODE_ADDRESS);
}

void set_bootmode(bootmode_t mode) {
    WRITE32(CONFIG_BOOTMODE_ADDRESS, mode);
}

void show_bootmode(bootmode_t mode) {
    printf(" => %s mode...\n", bootmode2str(mode));
    video_printf(" => %s mode%s\n", bootmode2str(mode),
                 (mode == BOOTMODE_FASTBOOT) ? " (kaeru)..." : "...");
}