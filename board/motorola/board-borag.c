//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void cmd_reboot_emergency(const char* arg, void* data, unsigned sz) {
    fastboot_info("The device will reboot into bootrom mode...");
    fastboot_okay("");
    reboot_emergency();
}

void cmd_signature(const char* arg, void* data, unsigned sz) {
    WRITE32(0x480FEF1C, 0x1);
    fastboot_okay("");
}

void board_early_init(void) {
    printf("Entering early init for Motorola E22\n");

    uint32_t addr = 0;

    // Borag has a lock function called in app() that forces the device
    // to relock based on the following conditions:
    // * If seccfg reports as unlocked
    // * If the flag in proinfo at 0xCD9 is set to 'a'
    //
    // To prevent this, we force the function to not run by making it
    // just return 0
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0x2300, 0xB083, 0xF8AD, 0x3004);
    if (addr) {
        printf("Found lock function at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Register our custom signature command that always sets the flag to 1.
    // Not necessarily needed, but at least we are sure the flag won't
    // get changed to 0
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF7FF, 0xFB16, 0xF8DF, 0x1564);
    if (addr) {
        printf("Found cmd_signature at 0x%08X\n", addr);
        NOP(addr, 2);
        fastboot_register("signature", cmd_signature, 1);
    }

    // Even though we patch signature checks above, this lk contains multiple checks
    // sparsed around various functions.
    // All of them are based around sbc and signature.
    // By making get_hw_sbc return 0 (unfused), we can bypass all these checks, allowing
    // to perform basics operations like flashing and erasing partitions
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2360, 0xF2C1, 0x13C5, 0x6818);
    if (addr) {
        printf("Found get_hw_sbc at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    fastboot_register("oem reboot-emergency", cmd_reboot_emergency, 1);
}

void board_late_init(void) {
    printf("Entering late init for Motorola E22\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    //
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B, 0x681B, 0x681B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Ontim (Borag ODM) handles permissions in fastboot by checking this flag.
    // This flag controls erase, flash and more.
    WRITE32(0x480FEF1C, 0x1);
}
