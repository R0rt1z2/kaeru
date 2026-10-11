//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

static void cmd_device_mode(const char* arg, void* data, unsigned sz) {
    (void)data;
    (void)sz;

    if (!arg || !*arg) {
        char* mode = get_env("kaeru.device_mode");
        char buf[64];
        npf_snprintf(buf, sizeof(buf), "device_mode=%s", mode ? mode : "unlocked");
        fastboot_info(buf);
        fastboot_info("");
        fastboot_info("Usage: fastboot oem device-mode <engineering|unlocked>");
        fastboot_okay("");
        return;
    }

    if (strcmp(arg, "engineering") == 0 || strcmp(arg, "unlocked") == 0) {
        set_env("kaeru.device_mode", (char*)arg);
        fastboot_info("Device mode updated.");
        fastboot_info("Reboot for changes to take effect.");
        fastboot_okay("");
        return;
    }

    fastboot_fail("Unknown mode. Valid options: engineering, unlocked");
}

FASTBOOT_CMD(device_mode, "oem device-mode", cmd_device_mode, 1);

void sp_init_hook(void) {
    char* mode = get_env("kaeru.device_mode");

    if (mode && strcmp(mode, "engineering") == 0) {
        *(volatile uint32_t*)0x481E1E84 = 0x7070;
    } else {
        *(volatile uint32_t*)0x481E1E84 = 0x77EE;
        *(volatile uint32_t*)0x481E1E64 = 0x66CC;
        FORCE_RETURN(0x4807FD14, 1); // gvb_unlocked check
    }
}

void board_early_init(void) {
    printf("Entering early init for Motorola Moto G Pure 2021\n");

    // Motorola sets the device lock state from the SP partition during
    // boot. Hook after environment initialization so we can read the
    // persisted device mode and override the SP-derived globals.
    PATCH_CALL(0x48002DB0, (void*)sp_init_hook, TARGET_THUMB);

    // Prevent the phone from refusing to boot if the SP data has been
    // tampered with.
    FORCE_RETURN(0x4807ED40, 1);

    // Disable image verification so we can boot or flash any partition,
    // not just boot and recovery. Without this, custom logos, modem
    // images, and other signed partitions would be rejected.
    FORCE_RETURN(0x48018518, 0);
}

void board_late_init(void) {
    printf("Entering late init for Motorola Moto G Pure 2021\n");
}
