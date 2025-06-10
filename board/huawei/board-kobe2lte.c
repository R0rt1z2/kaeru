//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void board_early_init(void) {
    printf("Entering early init for Huawei MatePad T8 (LTE)\n");

    // Cosmetic patch to suppress the default Android logo with lock state info
    // shown during boot. This has no functional impact, purely visual.
    //
    // To restore the original behavior, simply remove this patch.
    FORCE_RETURN(0x4800F3F4, 0);
}

void board_late_init(void) {
    printf("Entering late init for Huawei MatePad T8 (LTE)\n");

    // Huawei registers a wrapper around all OEM commands, effectively blocking
    // their execution, even those explicitly registered by LK.
    //
    // This patch disables the wrapper registration entirely, allowing OEM commands
    // to run as intended without interference.
    NOP(0x48034308, 2);

    // MediaTek added a battery level check to the flash command that blocks
    // flashing when the battery is below 3%. While this might seem like a safety
    // feature, it's unnecessary, devices in fastboot are typically USB-powered.
    //
    // This patch bypasses the check entirely, allowing flashing regardless of
    // charge level.
    FORCE_RETURN(0x4803AB8C, 1);

    // This device has a struct whose pointer is returned by getter function(s). Then
    // the callers add specific offsets to the pointer to access particular fields.
    //
    // From my analysis, these offsets appear to control FBLOCK, USRLOCK, and WIDEVINE lock
    // states. Forcing these fields to 1 (unlocked) bypasses the lock state checks.
    WRITE32(0x48232008 + 0x104, 1);
    WRITE32(0x48232008 + 0x108, 1);
    WRITE32(0x48232008 + 0x10c, 1);
    WRITE32(0x48232008 + 0x114, 1);
    FORCE_RETURN(0x4802BCFC, 1);

    // Huawei defines two separate bootloader lock states: USRLOCK and FBLOCK.
    //
    // USRLOCK is cleared when the user unlocks the bootloader with an official code.
    // FBLOCK, however, remains locked and restricts flashing of critical partitions,
    // such as the bootloader itself.
    //
    // These patches force both lock state getters to always return 1 (unlocked),
    // bypassing the restrictions entirely.
    FORCE_RETURN(0x4802BAA8, 1);
    FORCE_RETURN(0x4802BA78, 1);

    // This function is used extensively throughout the fastboot implementation to
    // determine whether the device is a development unit. Until this check passes,
    // fastboot treats the device as locked, even if the bootloader is already unlocked.
    //
    // To ensure flashing works as expected, we patch this function to always return 0,
    // effectively marking the device as non-secure.
    FORCE_RETURN(0x480985C4, 1);

    // Huawei overrides standard bootmode selection with their own logic,
    // often forcing an alternative recovery mode, especially after a failed boot.
    //
    // This patch restores conventional behavior:
    // - Volume Up → Recovery
    // - Volume Down → Fastboot
    //
    // This makes bootmode selection consistent with most Android devices.
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
    } else if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x48086BE8, 0);

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_NORMAL) {
        show_bootmode(get_bootmode());
    }
}