//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for Huawei Y5 Prime 2018\n");

    // Huawei tracks two separate lock states, FBLOCK and USRLOCK, each read
    // through its own getter where 1 means unlocked.
    //
    // Forcing both getters to return 1 makes the device read as fully
    // unlocked everywhere the state is checked.
    FORCE_RETURN(0x6002FD3C, 1);  // FBLOCK -> unlocked
    FORCE_RETURN(0x6002FD30, 1);  // USRLOCK -> unlocked

    // Skip the boot-state warning screen and its 5 second delay shown on
    // unlocked devices.
    FORCE_RETURN(0x60004E94, 0);
}

void board_late_init(void) {
    printf("Entering late init for Huawei Y5 Prime 2018\n");
}
