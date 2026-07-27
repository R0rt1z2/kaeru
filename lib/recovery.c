//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/string.h>
#include <lib/recovery.h>

#ifdef CONFIG_RECOVERY_CMDLINE_PATCH
#include <lib/bootargs.h>
#include <lib/spoof.h>
#endif

bootmode_t misc_command_to_bootmode(const char* c) {
    if (!c || !c[0])
        return BOOTMODE_NORMAL;

    if (!strncmp(c, "boot-recovery",       13)) return BOOTMODE_RECOVERY;
    if (!strncmp(c, "boot-fastboot",       13)) return BOOTMODE_RECOVERY;
    if (!strncmp(c, "boot-bootloader",     15)) return BOOTMODE_FASTBOOT;
    if (!strncmp(c, "bootonce-bootloader", 19)) return BOOTMODE_FASTBOOT;
    return BOOTMODE_NORMAL;
}

#ifdef CONFIG_RECOVERY_CMDLINE_PATCH
// When booting into recovery, we need to ensure verifiedbootstate is set
// to "orange" so adbd and fastbootd detect the device as unlocked. With
// the spoofed locked state they both refuse to do anything, no adb access
// and no flashing.
//
// Don't touch vbmeta.device_state here, rewriting it to "unlocked" hangs
// recovery on some devices.
void handle_recovery_boot(void) {
    if (get_bootmode() != BOOTMODE_RECOVERY || !is_spoofing_enabled())
        return;

    printf("Recovery boot detected, modifying cmdline for unlocked state.\n");

    static const uint32_t cmdline_addrs[] = {
        CONFIG_RECOVERY_CMDLINE1_ADDRESS,
        CONFIG_RECOVERY_CMDLINE2_ADDRESS,
    };

    for (int i = 0; i < ARRAY_SIZE(cmdline_addrs); i++) {
        if (!cmdline_addrs[i])
            continue;

        printf("Patching cmdline at 0x%08X\n", cmdline_addrs[i]);
        cmdline_replace((char*)cmdline_addrs[i],
                        "androidboot.verifiedbootstate=", "green", "orange");
    }
}
#endif
