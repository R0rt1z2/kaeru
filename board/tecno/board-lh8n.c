//
// SPDX-FileCopyrightText: 2026 naden01 <naden.irsyad01@gmail.com>
// SPDX-FileCopyrightText: 2026 KanagawaYamada <albert.wesley.dion@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    /* No early initialization required for LH8n */
}

void board_late_init(void) {
    // Disable Orange State Warning
    FORCE_RETURN(0x48251c70, 0);

    // Spoof boot state to green
    WRITE16(0x48251e6e, 0x2300);
    
    // Spoof VBMeta to locked
    WRITE16(0x482616dc, 0xBF00);

    // Force if Power + Vol up = bootloader
    bootmode_t mode = get_bootmode();
    
    if (mode == BOOTMODE_RECOVERY) {
        set_bootmode(BOOTMODE_FASTBOOT);
        video_printf("\n>>> VOL+ DETECTED: FORCING FASTBOOT <<<\n\n");
    } 
}
