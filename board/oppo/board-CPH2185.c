//
// SPDX-FileCopyrightText: 2026 fantom3031 <mpiven69@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for oppo A15/A15s\n");

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

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF75, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF6F, 0xF000);
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

     addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B, 0x681B, 0x681B, 0x2B02);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2802, 0xd000, 0x4770, 0xb538, 0x2432);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
    
}

void board_late_init(void) {
    printf("Entering late init for oppo A15/A15s\n");
}
