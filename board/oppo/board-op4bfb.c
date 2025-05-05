//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define VOLUME_UP 17
#define VOLUME_DOWN 1

void board_early_init(void) {
    printf("Entering early init for OPPO A11K/A12\n");

    uint32_t addr = 0;

    // BBK added a verification check to ensure the device was officially unlocked.
    // If the check fails, the bootloader exits fastboot mode and reboots.
    //
    // This is unnecessary, seccfg-based unlocks are already valid, so we patch
    // the check to always return true, ensuring fastboot remains accessible.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xF5AD, 0x5DAC);
    if (addr) {
        printf("Found fastboot_unlock_verify at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // On unlocked OPPO devices, volume key detection is broken, making it
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
    printf("Entering late init for OPPO A11K/A12\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay. This patch removes
    // both by disabling the calls to video_printf() and mdelay().
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7F0, 0xF849, 0x480A);
    if (addr) {
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7F0, 0xF845, 0x4809);
    if (addr) {
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7F0, 0xF841, 0xF7C5);
    if (addr) {
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7F0, 0xF845, 0x4809);
    if (addr) {
        NOP(addr, 2);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7C2, 0xF86D, 0xF7C5);
    if (addr) {
        NOP(addr, 2);
    }

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0xB082, 0xA802, 0x2400);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}