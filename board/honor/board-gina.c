//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void board_early_init(void) {
    printf("Entering early init for Honor 60 SE\n");

    // Cosmetic patch to suppress the default Android logo with lock state info
    // shown during boot. This has no functional impact, purely visual.
    //
    // To restore the original behavior, simply remove this patch.
    FORCE_RETURN(0x48214690, 0);
}

void board_late_init(void) {
    printf("Entering late init for Honor 60 SE\n");

    // Honor registers a wrapper around all OEM commands, effectively blocking
    // their execution, even those explicitly registered by LK.
    //
    // This patch disables the wrapper registration entirely, allowing OEM commands
    // to run as intended without interference.
    NOP(0x4824D780, 2);

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x482B3300, 0);

    // This device has a struct whose pointer is returned by getter function(s). Then
    // the callers add specific offsets to the pointer to access particular fields.
    //
    // From my analysis, these offsets appear to control FBLOCK, USRLOCK, and WIDEVINE lock
    // states. Forcing these fields to 1 (unlocked) bypasses the lock state checks.
    WRITE32(0x48577008 + 0x104, 1);
    WRITE32(0x48577008 + 0x108, 1);
    WRITE32(0x48577008 + 0x10C, 1);
    WRITE32(0x48577008 + 0x114, 1);

    // Honor overrides standard bootmode selection with their own logic,
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

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_NORMAL) {
        show_bootmode(get_bootmode());
    }
}
