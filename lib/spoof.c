
//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/spoof.h>
#include <lib/common.h>

int is_spoofing_enabled(void) {
    const char *val = get_env(KAERU_ENV_BLDR_SPOOF);
    return val && strcmp(val, "1") == 0;
}

int get_lock_state(uint32_t *lock_state) {
    int spoofing = is_spoofing_enabled();
    printf("Attempted to get lock state, spoofing is %s\n",
            spoofing ? "enabled" : "disabled");
    *lock_state = spoofing ? LKS_LOCK : LKS_UNLOCK;
    return 0;
}

void cmd_spoof_bootloader_lock(const char *arg, void *data, unsigned sz) {
    int status = is_spoofing_enabled();
    const char *option = arg + 1;
    int target = -1;

    if (!strcmp(option, "on"))       target = 1;
    else if (!strcmp(option, "off")) target = 0;

    if (target != -1) {
        if (status != target) {
            set_env(KAERU_ENV_BLDR_SPOOF, target ? "1" : "0");
            fastboot_info(target ?
                "Bootloader spoofing enabled." :
                "Bootloader spoofing disabled.");
            fastboot_info("A factory reset may be required.");
        } else {
            fastboot_info(target ?
                "Bootloader spoofing is already enabled." :
                "Bootloader spoofing is already disabled.");
        }
        fastboot_publish("is-spoofing", target ? "1" : "0");
        fastboot_okay("");
        return;
    }

    if (!strcmp(option, "status")) {
        fastboot_info(status ?
            "Bootloader spoofing is currently enabled." :
            "Bootloader spoofing is currently disabled.");
        fastboot_info(status ?
            "Device is currently spoofed as bootloader locked." :
            "Device is not being spoofed as bootloader locked.");
        fastboot_okay("");
        return;
    }

    fastboot_info("kaeru bootloader lock spoofing control");
    fastboot_info("");
    fastboot_info("When enabled, device reports as 'locked' to TEE");
    fastboot_info("while maintaining full fastboot and root capabilities.");
    fastboot_info("");
    fastboot_info("Commands:");
    fastboot_info("  on     - Enable spoofing (reboot required)");
    fastboot_info("  off    - Disable spoofing (reboot required)");
    fastboot_info("  status - Show current state");
    fastboot_fail("Usage: fastboot oem bldr_spoof <on|off|status>");
}