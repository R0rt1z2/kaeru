//
// SPDX-FileCopyrightText: 2025-2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 1
#define VOLUME_DOWN 17

void mt_power_off(void) {
    ((void (*)(void))(0x4822C04C | 1))();
}

void cmd_shutdown(const char* arg, void* data, unsigned sz) {
    fastboot_info("The device will power off...");
    fastboot_info("Make sure to unplug the USB cable!");
    fastboot_okay("");
    mt_power_off();
}

void board_early_init(void) {
    printf("Entering early init for Honor 60 SE\n");

    // Huawei manages the lock state via the OEMINFO partition. On devices running
    // EMUI 10, only RDLock (Research & Development Lock) is relevant. RDLock is
    // intended for use by Authorized Service Centers and the factory to permanently
    // unlock a device. When active, it allows flashing to any partition and loading
    // insecure images.
    //
    // During boot, LK reads OEMINFO and uses the RDLock state to determine whether
    // FBLOCK should be set to unlocked. Forcing this function to return 1 basically
    // tells LK that RDLock is active, keeping FBLOCK unlocked.
    FORCE_RETURN(0x48242928, 1);

    // Huawei also tracks the root state via OEMINFO. During boot, LK calls this
    // function to check whether the device has been rooted or tampered with, and
    // may alter its behavior accordingly (e.g. displaying warnings or blocking boot).
    //
    // Forcing the return value to 0 makes LK always treat the device as unrooted,
    // suppressing any root-related checks or warnings.
    FORCE_RETURN(0x482449A0, 0);

    // Cosmetic patch to suppress the default Android logo with lock state info
    // shown in fastboot. This has no functional impact, purely visual.
    //
    // To restore the original behavior, simply remove this patch.
    FORCE_RETURN(0x48214690, 0);
}

void board_late_init(void) {
    printf("Entering late init for Honor 60 SE\n");

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x482B3300, 0);

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

    // Honor registers a wrapper around all OEM commands, effectively blocking
    // their execution, even those explicitly registered by LK.
    //
    // This patch disables the wrapper registration entirely, allowing OEM commands
    // to run as intended without interference.
    NOP(0x4824D780, 2);

    bootmode_t mode = get_bootmode();
    if (mode == BOOTMODE_FASTBOOT) {
        // There is no easy way to power off the device from fastboot mode.
        // Holding the power button simply reboots the device, forcing you to
        // boot into the OS to shut it down properly. This is not ideal, so
        // we add a fastboot command that allows powering off directly.
        //
        // This was shamelessly borrowed from Motorola's (or Huaqin's) LK image.
        // It simply calls mt_power_off().
        fastboot_register("oem shutdown", cmd_shutdown, 1);
        fastboot_register("oem poweroff", cmd_shutdown, 1);

        // Huawei stores a copy of the FBLOCK & USRLOCK states at fixed addresses and
        // uses them to check the lock state when issuing the fastboot oem lock-state
        // info command.
        //
        // This is merely a cosmetic issue, but we patch it anyway so it's consistent.
        WRITE32(0x4857710C, 1);
        WRITE32(0x48577110, 1);
    }

    if (mode != BOOTMODE_NORMAL 
        && mode != BOOTMODE_POWEROFF_CHARGING && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        show_bootmode(mode);
    }
}