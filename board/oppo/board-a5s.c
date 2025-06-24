//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void board_early_init(void) {
    printf("Entering early init for Oppo A5s\n");

    // Patch to enable:
    // - Volume Up → Fastboot
    // - Volume Down → Recovery
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    } else if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_RECOVERY);
    }
}

void board_late_init(void) {
    printf("Entering late init for Oppo A5s\n");

    uint32_t addr = 0;

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Patch the function to always return 0 and thus not display the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xA802);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    if (get_bootmode() != BOOTMODE_NORMAL) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        //
        // Displaying the boot mode can be helpful for developers, as it provides
        // immediate feedback and can prevent debugging headaches.
        show_bootmode(get_bootmode());
    }
}
