//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_DOWN 0

void mt_power_off(void) {
    ((void (*)(void))(0x46000188 | 1))();
}

void cmd_shutdown(const char* arg, void* data, unsigned sz) {
    fastboot_info("The device will power off...");
    fastboot_info("Make sure to unplug the USB cable!");
    fastboot_okay("");
    mt_power_off();
}

void board_early_init(void) {
    printf("Entering early init for Energy Phone Pro 3\n");

    // Despite SBC being off globally, LK for some reason still thinks the device
    // is secure and enforces lock state checks in fastboot.
    //
    // To fix this, we patch the functions invoked in the fastboot command handler
    // to always mark the device as non-secure.
    FORCE_RETURN(0x4602E320, 0);
    FORCE_RETURN(0x4602CF64, 0);
    FORCE_RETURN(0x4602E05C, 1);
    PATCH_MEM(0x4602E080, 0x2001,  // movs r1, #1
                          0x6008,  // str r1,[r1,#0x0]
                          0x2000,  // movs r0, #0
                          0x4770   // bx lr
    );
}

void board_late_init(void) {
    printf("Entering late init for Energy Phone Pro 3\n");

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    //
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x460023B8, 0);

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    FORCE_RETURN(0x4600235C, 0);

    // MediaTek added a battery level check to the flash command that blocks
    // flashing when the battery is below 3%. While this might seem like a safety
    // feature, it's unnecessary, devices in fastboot are typically USB-powered.
    //
    // This patch bypasses the check entirely, allowing flashing regardless of
    // charge level.
    FORCE_RETURN(0x460202D8, 1);

    // There's no sane way to enter fastboot mode on this device using hardware
    // keys. The only method I found involves using ADB to reboot into those modes
    // which is inconvenient.
    //
    // To improve this, we repurpose the volume keys to allow entering those modes
    // during early init.
    if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
        show_bootmode(BOOTMODE_FASTBOOT);

        // There is no easy way to power off the device from fastboot mode.
        // Holding the power button simply reboots the device, forcing you to
        // boot into the OS to shut it down properly. This is not ideal, so
        // we add a fastboot command that allows powering off directly.
        //
        // This was shamelessly borrowed from Motorola’s (or Huaqin’s) LK image.
        // It simply calls mt_power_off().
        fastboot_register("oem shutdown", cmd_shutdown, 1);
        fastboot_register("oem poweroff", cmd_shutdown, 1);
    }
}
