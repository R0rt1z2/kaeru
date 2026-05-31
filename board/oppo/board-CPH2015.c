//
// SPDX-FileCopyrightText: 2026 fantom3031 <mpiven69@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for oppo A31\n");
    
}

void board_late_init(void) {
    printf("Entering late init for oppo A31\n");

    if (mtk_detect_key(17)) {
        set_bootmode(BOOTMODE_RECOVERY);
        show_bootmode(get_bootmode());
    }
}
