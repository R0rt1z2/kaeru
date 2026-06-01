//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for Huawei Y5p\n");

    // Cosmetic patch to suppress the default Android logo with lock state info
    // shown during boot. This has no functional impact, purely visual.
    //
    // To restore the original behavior, simply remove this patch.
    FORCE_RETURN(0x4800BFC8, 0);
}

void board_late_init(void) {
    printf("Entering late init for Huawei Y5p\n");

    // Huawei registers a wrapper around all OEM commands, effectively blocking
    // their execution, even those explicitly registered by LK.
    //
    // This patch disables the wrapper registration entirely, allowing OEM commands
    // to run as intended without interference.
    NOP(0x48035E80, 2);

    // MediaTek added a battery level check to the flash command that blocks
    // flashing when the battery is below 3%. While this might seem like a safety
    // feature, it's unnecessary, devices in fastboot are typically USB-powered.
    //
    // This patch bypasses the check entirely, allowing flashing regardless of
    // charge level.
    FORCE_RETURN(0x4803C704, 1);

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x48088F80, 0);

    // Huawei defines two separate bootloader lock states: USRLOCK and FBLOCK.
    //
    // USRLOCK is cleared when the user unlocks the bootloader with an official code.
    // FBLOCK, however, remains locked and restricts flashing of critical partitions,
    // such as the bootloader itself.
    //
    // We patch the functions that get called when initializing the security state
    // struct to ensure the spoofed unlocked state is propagated correctly.
    FORCE_RETURN(0x4802D664, 1); // get_lock_state_for_verifyboot
    FORCE_RETURN(0x4802D39C, 1); // get_oeminfo_rdlock_type
    FORCE_RETURN(0x4802F398, 0); // get_oeminfo_root_type
    FORCE_RETURN(0x4802D840, 1); // get_oeminfo_frp_type

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL 
        && mode != BOOTMODE_POWEROFF_CHARGING && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        show_bootmode(mode);
    }
}