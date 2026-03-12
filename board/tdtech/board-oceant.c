//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void mt_power_off(void) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x2001, 0xF000, 0xFD42);
    if (addr) {
        printf("Found mt_power_off at 0x%08X\n", addr);
        ((void (*)(void))(addr | 1))();
    }
}

void cmd_shutdown(const char* arg, void* data, unsigned sz) {
    fastboot_info("The device will power off...");
    fastboot_info("Make sure to unplug the USB cable!");
    fastboot_okay("");
    mt_power_off();
}

void board_early_init(void) {
    printf("Entering early init for TD Tech M40\n");

    uint32_t addr = 0;

    // Huawei manages the lock state via the OEMINFO partition. On devices running
    // EMUI 10, only RDLock (Research & Development Lock) is relevant. RDLock is
    // intended for use by Authorized Service Centers and the factory to permanently
    // unlock a device. When active, it allows flashing to any partition and loading
    // insecure images.
    //
    // During boot, LK reads OEMINFO and uses the RDLock state to determine whether
    // FBLOCK should be set to unlocked. Forcing this function to return 1 basically
    // tells LK that RDLock is active, keeping FBLOCK unlocked.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB570, 0xB0C4, 0xAE04, 0xF44F);
    if (addr) {
        printf("Found get_oeminfo_rdlock_type at 0x%08X\n", addr);
        FORCE_RETURN(addr, 1);
    }

    // Huawei also tracks the root state via OEMINFO. During boot, LK calls this
    // function to check whether the device has been rooted or tampered with, and
    // may alter its behavior accordingly (e.g. displaying warnings or blocking boot).
    //
    // Forcing the return value to 0 makes LK always treat the device as unrooted,
    // suppressing any root-related checks or warnings.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB500, 0xB085, 0x2043, 0xA901);
    if (addr) {
        printf("Found get_oeminfo_root_type at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Cosmetic patch to suppress the default Android logo with lock state info
    // shown in fastboot. This has no functional impact, purely visual.
    //
    // To restore the original behavior, simply remove this patch.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4C09, 0x447C, 0x6824);
    if (addr) {
        printf("Found show_fastboot_logo at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}

void board_late_init(void) {
    printf("Entering late init for TD Tech M40\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    //
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0x4606, 0xB082);
    if (addr) {
        printf("Found show_boot_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Huawei registers a wrapper around all OEM commands, effectively blocking
    // their execution, even those explicitly registered by LK.
    //
    // This patch disables the wrapper registration entirely, allowing OEM commands
    // to run as intended without interference.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7FF, 0xF96E, 0xF8DF, 0x06D8);
    if (addr) {
        printf("Found OEM command wrapper at 0x%08X\n", addr);
        NOP(addr, 2);
    }

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
        WRITE32(0x4844410C, 1);
        WRITE32(0x48444110, 1);
    }

    if (mode != BOOTMODE_NORMAL 
        && mode != BOOTMODE_POWEROFF_CHARGING && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        show_bootmode(mode);
    }
}