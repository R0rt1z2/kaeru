//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

// Currently unused, but potentially useful for this specific device:
// booting from slot B causes the device to brick due to issues with
// battery temperature readings. This function forces the boot slot
// to either "_a" or "_b" by directly modifying memory addresses.
void force_slot(char slot) {
    const char* suffix = (slot == 'a') ? "_a" : "_b";

    // TODO: Use a more robust way to find these addresses.
    char *a = (char *)0x4807BAC0;
    char *b = (char *)0x4807BE24;
    
    strcpy(a, suffix);
    strcpy(b, suffix);
    
    printf("Forced all suffixes to '%s'\n", suffix);
}

void board_early_init(void) {
    printf("Entering early init for Rabbit R1\n");

    uint32_t addr = 0;

    // When booting from slot B, the battery temperature reading is broken
    // and always reports the device as overheating. As a result, the device
    // becomes unusable and fails to boot. As a workaround, we force the
    // function responsible for reading the battery temperature to always
    // return 20ºC, which is safely below the thermal shutdown threshold (50ºC).
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B5B, 0x2200, 0xE92D, 0x4FF0);
    if (addr) {
        printf("Found force_get_tbat at 0x%08X\n", addr);
        FORCE_RETURN(addr, 20); // 20ºC
    }

    // force_slot('a'); // Uncomment to force booting from slot A
}

void board_late_init(void) {
    printf("Entering late init for Rabbit R1\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B, 0x681B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200, 0x4604);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}
