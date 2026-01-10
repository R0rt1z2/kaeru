//
// SPDX-FileCopyrightText: 2026 Ryo "evilMyQueen" Yamada <evilMyQueen@mainlining.org>
//                         2026 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_DOWN 0

void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4BD29370|1))(arg, data, sz);
}

void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4601FE44|1))(arg, data, sz);
}

void cmd_reboot_recovery(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4601FBF0|1))(arg, data, sz);
}

void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz)
{
    if (arg && strstr(arg, "recovery")) {
        cmd_reboot_recovery(arg, data, sz);
        return;
    }

    if (arg && strstr(arg, "bootloader")) {
        cmd_reboot_bootloader(arg, data, sz);
        return;
    }

    cmd_reboot("", data, sz);
}

void board_early_init(void) {
    printf("Entering early init for Meizu M6\n");

    // Despite SBC being off globally, LK for some reason still thinks the device
    // is secure and enforces lock state checks in fastboot.
    //
    // To fix this, we patch the functions invoked in the fastboot command handler
    // to always mark the device as non-secure.
    FORCE_RETURN(0x46035550, 0);
    FORCE_RETURN(0x46034194, 0);
    FORCE_RETURN(0x4603528C, 1);
    PATCH_MEM(0x460352B0, 0x2001,  // movs r1, #1
                          0x6008,  // str r1,[r1,#0x0]
                          0x2000,  // movs r0, #0
                          0x4770   // bx lr
    );

    // This is for visual consistency
    strcpy((char*)0x4603AEA0, " => FASTBOOT mode (kaeru)...\n");
}

void board_late_init(void) {
    printf("Entering late init for Meizu M6\n");

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    //
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x460029B0, 0);

    // MediaTek added a battery level check to the flash command that blocks
    // flashing when the battery is below 3%. While this might seem like a safety
    // feature, it's unnecessary, devices in fastboot are typically USB-powered.
    //
    // This patch bypasses the check entirely, allowing flashing regardless of
    // charge level.
    FORCE_RETURN(0x46024830, 1);

    // There's no sane way to enter fastboot mode on this device using hardware
    // keys. The only method I found involves using ADB to reboot into those modes
    // which is inconvenient.
    //
    // To improve this, we repurpose the volume keys to allow entering those modes
    // during early init.
    if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        // Meizu managed to screw up reboot commands in two different ways:
        //
        // 1. The command parser has a bug where "reboot-recovery" gets matched
        //    as just "reboot" (probably some dumb string comparison issue),
        //    so the device just does a normal reboot instead of going to recovery.
        //
        // 2. Even when recovery mode is triggered properly, their implementation
        //    doesn't write the bootloader message correctly to the misc partition,
        //    so recovery boot fails anyway.
        //
        // We work around both issues by disabling their broken command handlers
        // and registering our own. The reboot wrapper does a nasty string check
        // to intercept "recovery" and redirect it to our working implementation.
        NOP(0x4601F6C8, 2); // register fastboot reboot bootloader.
        NOP(0x4601F6B8, 2); // register fastboot reboot.

        // Register our non-broken reboot command handlers. For the recovery one,
        // we use "reboot-recovery", which matches the fastboot standard.
        fastboot_register("reboot", cmd_reboot_wrapper, 1);
    }

    bootmode_t mode = get_bootmode();

    if (mode != BOOTMODE_NORMAL 
        && mode != BOOTMODE_POWEROFF_CHARGING 
        && mode != BOOTMODE_FASTBOOT && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        show_bootmode(mode);
    }
}