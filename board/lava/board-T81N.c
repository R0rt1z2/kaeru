//
// SPDX-FileCopyrightText: 2026 fantom3031 <mpiven69@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for Lava T81N\n");

    uint32_t addr = 0;

    // On unlocked devices, LK shows an orange state warning during boot
    // that also introduces an unnecessary 5 second delay. Forcing the
    // function to return 0 skips both the warning and the delay.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB538, 0x4605, 0x4c28, 0x447c);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Patch the function to always return 0 and thus not display the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2802, 0xd000, 0x4770, 0xb510, 0x2432);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}

void board_late_init(void) {
    printf("Entering late init for Lava T81N\n");
}
