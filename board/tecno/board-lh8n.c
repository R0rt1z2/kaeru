//
// SPDX-FileCopyrightText: 2026 naden01 <naden.irsyad01@gmail.com>
// SPDX-FileCopyrightText: 2026 KanagawaYamada <albert.wesley.dion@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {}

void board_late_init(void) {
    // ---------------------------------------------------------
    // DYNAMIC PATCHING (OTA SURVIVABLE)
    // ---------------------------------------------------------
    // Instead of using hardcoded memory addresses which break on OTA updates,
    // we use Kaeru's SEARCH_PATTERN to dynamically scan the bootloader in RAM.
    
    // Disable Orange State Warning dynamically
    uint32_t orange_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                          0xb508, 0xf7ea, 0xff7f, 0xf7ea, 0xff77, 0x2100);
    if (orange_addr) {
        FORCE_RETURN(orange_addr, 0);
    }

    bootmode_t mode = get_bootmode();

    // Hardware Volume Key Boot Mode Overrides
    if (mode == BOOTMODE_RECOVERY) {
        // Vol+ detected -> Force Bootloader (Fastboot)
        set_bootmode(BOOTMODE_FASTBOOT);
        video_printf("\n>>> VOL+ DETECTED: FORCING BOOTLOADER <<<\n\n");
    }

    // Refresh mode in case it was modified
    mode = get_bootmode();

    if (mode != BOOTMODE_RECOVERY) {
        // Dynamically search for the Green State check and spoof it
        uint32_t green_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                             0x4b18, 0x447b, 0x681b, 0x681b, 0x2b03, 0xd807);
        if (green_addr) {
            WRITE16(green_addr + 6, 0x2300); // Spoof boot state to green
        }

        // Dynamically search for the VBMeta device_state generator and spoof it
        uint32_t lock_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                            0x2801, 0xd0f5, 0xb9a0, 0x9b09);
        if (lock_addr) {
            WRITE16(lock_addr + 8, 0xBF00);  // Spoof VBMeta to locked
        }
    }
}
