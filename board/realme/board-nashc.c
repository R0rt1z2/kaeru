//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void board_early_init(void) {
    printf("Entering early init for Realme 8\n");

    uint32_t addr = 0;

    // BBK added a verification check to ensure the device was officially unlocked.
    // If the check fails, the bootloader exits fastboot mode and reboots.
    //
    // This is unnecessary, seccfg-based unlocks are already valid, so we patch
    // the check to always return true, ensuring fastboot remains accessible.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7C7, 0xFAA9);
    if (addr) {
        printf("Found fastboot_unlock_verify at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF012, 0xFEB5, 0xF8DF);
    if (addr) {
        printf("Found fastboot_screen_clear at 0x%08X\n", addr);
        NOP(addr, 2);
    }

    // Cosmetic patch to disable the broken boot mode selection menu shown
    // when entering fastboot mode. It serves no real purpose and it just
    // clutters the screen. We can display the boot mode later if needed.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF012, 0xFe24, 0xF7FF);
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
    printf("Entering late init for Realme 8\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0A, 0x447B, 0x681B, 0x681B, 0x2B02);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200, 0x4604);
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