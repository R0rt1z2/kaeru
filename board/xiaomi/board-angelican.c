//
// SPDX-FileCopyrightText: 2025 TendingStream73 <sasasabaev679@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for Xiaomi Redmi 9C NFC\n");
}

void board_late_init(void) {
    printf("Entering late init for Xiaomi Redmi 9C NFC\n");
    uint32_t addr = 0;

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2802, 0xD000, 0x4770);
    if (addr) {
        video_printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}
