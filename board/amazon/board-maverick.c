//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_DOWN 1

void board_early_init(void) {
    printf("Entering early init for Fire HD 10 (2019)\n");

    // Amazon uses IDME to check whether the device is locked or unlocked.
    // These functions verify if the unlock code matches the one stored in
    // the IDME partition and perform additional unlock verification checks.
    //
    // We patch them to bypass security checks and allow unrestricted fastboot
    // access regardless of the actual device lock state.
    FORCE_RETURN(0x56001914, 1);

    // The following patches disable specific video_printf calls that display
    // mode-specific messages during boot. These default messages can be
    // confusing to end users and don't provide useful information.
    //
    // We disable them here and will display our own custom messages in
    // board_late_init instead..
    NOP(0x56012202, 2); // => FASTBOOT / META mode
}

void board_late_init(void) {
    printf("Entering late init for Fire HD 10 (2019)\n");

    // Amazon removed the ability to enter fastboot mode with the volume
    // down key, we restore that functionality here.
    if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    if (get_bootmode() != BOOTMODE_NORMAL && get_bootmode() != BOOTMODE_FASTBOOT) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        //
        // Displaying the boot mode can be helpful for developers, as it provides
        // immediate feedback and can prevent debugging headaches.
        show_bootmode(get_bootmode());
    }

    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        video_printf(" => HACKED FASTBOOT mode - R0rt1z2 and bengris32\n");
    }
}
