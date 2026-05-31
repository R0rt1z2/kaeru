//
// SPDX-FileCopyrightText: 2026 fantom3031 <mpiven69@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for lava T81N\n");

   uint32_t addr = 0;
     addr = SEARCH_PATTERN(LK_START, LK_END, 0xB538, 0x4605, 0x4c28, 0x447c);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2802, 0xd000, 0x4770, 0xb510, 0x2432);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }


}

void board_late_init(void) {
    printf("Entering late init for lava T81N\n");
    video_printf("hello from fantom3031, me in your phone hehe");
}
