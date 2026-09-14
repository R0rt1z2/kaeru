//
// SPDX-FileCopyrightText: 2026 naden01 <naden.irsyad01@gmail.com>
// SPDX-FileCopyrightText: 2026 KanagawaYamada <albert.wesley.dion@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {}

void board_late_init(void) {
    // Disable Orange State Warning
    FORCE_RETURN(0x48251c70, 0);

    // If booting to Normal System (Not Recovery/TWRP):
    if (mode != BOOTMODE_RECOVERY) {
        WRITE16(0x48251e6e, 0x2300);     // Spoof boot state to green

        WRITE16(0x482616dc, 0xBF00);    // Spoof VBMeta to locked

    }
    // If booting to TWRP (mode == BOOTMODE_RECOVERY):
    // We leave the boot state as orange and unlocked.
}
