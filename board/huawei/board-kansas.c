//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
//                         2025 DiabloSat <dev@diablosat.cc>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void board_early_init(void) {
    printf("Entering early init for Huawei Y5 2019 / Honor 8A\n");

    // Likely unnecessary, but done as a precaution.
    //
    // These writes set all known lock states—USRLOCK, FBLOCK, and Widevine—to
    // 'unlocked'. While the bootloader may overwrite these later, we patch the
    // corresponding getters in `board_late_init` to enforce the unlocked state.
    WRITE32(0x481773A8, 1);  // USERLOCK state: UNLOCKED
    WRITE32(0x481773A4, 1);  // FBLOCK   state: UNLOCKED
    WRITE32(0x4817739C, 1);  // WIDEVINE state: UNLOCKED

    // Huawei uses OEMINFO to verify whether the device was officially unlocked.
    // If the check fails—but seccfg is unlocked through tools like mtkclient,
    // the bootloader triggers a manual relock.
    //
    // These patches disable that behavior, preventing an automatic relock
    // when unofficial unlock methods are used.
    NOP(0x4802DFF4, 2);

    // Cosmetic patch to suppress the default Android logo with lock state info
    // shown during boot. This has no functional impact, purely visual.
    //
    // To restore the original behavior, simply remove this patch.
    FORCE_RETURN(0x480200C4, 0);
}

void board_late_init(void) {
    printf("Entering late init for Huawei Y5 2019 / Honor 8A\n");

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x480566A8, 0);

    // Huawei registers a wrapper around all OEM commands, effectively blocking
    // their execution, even those explicitly registered by LK.
    //
    // This patch disables the wrapper registration entirely, allowing OEM commands
    // to run as intended without interference.
    NOP(0x48029FF8, 2);

    // MediaTek added a battery level check to the flash command that blocks
    // flashing when the battery is below 3%. While this might seem like a safety
    // feature, it's unnecessary, devices in fastboot are typically USB-powered.
    //
    // This patch bypasses the check entirely, allowing flashing regardless of
    // charge level.
    FORCE_RETURN(0x480309c4, 1);

    // Huawei defines two separate bootloader lock states: USRLOCK and FBLOCK.
    //
    // USRLOCK is cleared when the user unlocks the bootloader with an official code.
    // FBLOCK, however, remains locked and restricts flashing of critical partitions,
    // such as the bootloader itself.
    //
    // These patches force both lock state getters to always return 1 (unlocked),
    // bypassing the restrictions entirely.
    FORCE_RETURN(0x4802E370, 1);
    FORCE_RETURN(0x4802E484, 1);

    // Huawei overrides standard bootmode selection with their own logic,
    // often forcing an alternative recovery mode—especially after a failed boot.
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

    // Remove the useless commands for unlocking/locking/relocking the bootloader.
    //
    // Unlock is done by editing seccfg, locking by flashing stock LK.
    // These commands shouldn’t harm the device, but it’s safer to avoid them.
    NOP(0x4802a2e8, 2); // oem relock
    NOP(0x4802a2da, 2); // oem unlock
    NOP(0x4802a27a, 2); // flashing lock
    NOP(0x4802a264, 2); // flashing unlock

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_NORMAL) {
        show_bootmode(get_bootmode());
    }
}
