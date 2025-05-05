//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void board_early_init(void) {
    printf("Entering early init for Realme C3/Narzo 10A\n");

    uint32_t addr = 0;

    // BBK added a verification check to ensure the device was officially unlocked.
    // If the check fails, the bootloader exits fastboot mode and reboots.
    //
    // This is unnecessary, seccfg-based unlocks are already valid, so we patch
    // the check to always return true, ensuring fastboot remains accessible.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7CC, 0xFF0F);
    if (addr) {
        printf("Found fastboot_unlock_verify at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Cosmetic patch to disable the broken boot mode selection menu shown
    // when entering fastboot mode. It serves no real purpose and it just
    // clutters the screen. We can display the boot mode later if needed.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF00B, 0xFAD8, 0xF03E);
    if (addr) {
        printf("Found fastboot_mode_show at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // On unlocked Realme devices, volume key detection is broken, making it
    // difficult to enter fastboot or recovery mode through key combos.
    //
    // This patch restores expected behavior:
    // - Volume Up → Recovery
    // - Volume Down → Fastboot
    if (mtk_detect_key(VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
    } else if (mtk_detect_key(VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
    }
}

void board_late_init(void) {
    printf("Entering late init for Realme C3/Narzo 10A\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay. This patch removes
    // both by disabling the calls to video_printf() and mdelay().
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7ED, 0xFF4D, 0x48A0);
    if (addr) {
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7ED, 0xFF49, 0x4809);
    if (addr) {
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7ED, 0xFF45, 0xF00F);
    if (addr) {
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF00F, 0xF8F1, 0xF00F);
    if (addr) {
        NOP(addr, 2);
    }

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2802, 0xD000, 0x4770);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_RECOVERY
        && get_bootmode() != BOOTMODE_NORMAL) {
        show_bootmode(get_bootmode());
    }
}
