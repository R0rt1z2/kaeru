//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for BQ Edison 3\n");

    uint32_t addr = 0;

    // MediaTek added a battery level check to the flash command that blocks
    // flashing when the battery is below 3%. While this might seem like a safety
    // feature, it's unnecessary, devices in fastboot are typically USB-powered.
    //
    // This patch bypasses the check entirely, allowing flashing regardless of
    // charge level.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x2005, 0xF7E7);
    if (addr) {
        printf("Found power_check at 0x%08X\n", addr);
        FORCE_RETURN(addr, 1);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B25, 0x4A26, 0xB570);
    if (addr) {
        printf("Found sec_dl_permission_chk at 0x%08X\n", addr);
        PATCH_MEM(addr,
                  0x2001,  // movs r1, #1
                  0x6008,  // str r1,[r1,#0x0]
                  0x2000,  // movs r0, #0
                  0x4770   // bx lr
        );
    }
}

void board_late_init(void) {
    printf("Entering late init for BQ Edison 3\n");
}
