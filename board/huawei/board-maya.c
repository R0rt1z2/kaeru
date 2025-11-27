//
// SPDX-FileCopyrightText: 2025 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

int verified_boot_hook(void) {
    WRITE32(0x41E70A4C, 0); // BOOTSTATE_GREEN
    return 0;
}

int strcmp_hook(const char* s1, const char* s2) {
    return 0;
}

int allow_partition_hook(const char* partition, int* allowed) {
    *allowed = 1;
    return 0;
}

void board_early_init(void) {
    printf("Entering early init for Huawei Y5/Y6 2017\n");

    // This function is used extensively throughout the fastboot implementation to
    // determine whether the device is a development unit. Until this check passes,
    // fastboot treats the device as locked, even if the bootloader is already unlocked.
    //
    // To ensure flashing works as expected, we patch this function to always return 0,
    // effectively marking the device as non-secure.
    FORCE_RETURN(0x41E3DB4C, 0);

    // This device stores unlock status in multiple partitions (secro, seccfg, proinfo, rootm).
    // Different parts of the bootloader check different partitions, so we need to patch all
    // the check functions to return unlocked status.
    PATCH_MEM(0x41E37BE0,        // secro
        0x2300,  // movs r3, #0
        0x6003,  // str r3, [r0, #0]
        0x2000,  // movs r0, #0
        0x4770   // bx lr
    );
    PATCH_MEM(0x41E03848,        // rootm
        0x2300,  // movs r3, #0
        0x6003,  // str r3, [r0, #0]
        0x2000,  // movs r0, #0
        0x4770   // bx lr
    );
    FORCE_RETURN(0x41E384FC, 1); // seccfg
    FORCE_RETURN(0x41E22764, 1); // proinfo

    // Remove fastboot mode indicators and FRP/lock status warnings from the screen
    // to keep it clean.
    NOP(0x41E1EC76, 2); // FASTBOOT logo
    NOP(0x41E1F466, 2); // FRP Lock
    NOP(0x41E1F45A, 2); // FRP Unlock
    NOP(0x41E1F3E4, 2); // Phone Lock/Unlock
    NOP(0x41E1FFD4, 2); // Lock Status
    strcpy((char*)0x41e3ef58, " => FASTBOOT mode (kaeru)...\n");

    // Bypass partition whitelist check. The bootloader only allows flashing
    // boot/recovery/system partitions.
    // 
    // This hook makes all strcmp calls from the partition validation code
    // return 0 (match), allowing any partition to be flashed.
    PATCH_CALL(0x41E21444, (void*)strcmp_hook, TARGET_THUMB);
    PATCH_CALL(0x41E21450, (void*)strcmp_hook, TARGET_THUMB);
    PATCH_CALL(0x41E2145C, (void*)strcmp_hook, TARGET_THUMB);
    PATCH_CALL(0x41E21578, (void*)strcmp_hook, TARGET_THUMB);
    PATCH_CALL(0x41E21584, (void*)strcmp_hook, TARGET_THUMB);
    PATCH_CALL(0x41E21606, (void*)strcmp_hook, TARGET_THUMB);

    PATCH_CALL(0x41E215A8, (void*)allow_partition_hook, TARGET_THUMB);
    PATCH_CALL(0x41E21434, (void*)allow_partition_hook, TARGET_THUMB);
}

void board_late_init(void) {
    printf("Entering late init for Huawei Y5/Y6 2017\n");

    // This function is used to check if the "Allow OEM Unlocking" toggle has been
    // enabled under Android's Developer Options. If not enabled, when you issue
    // the bootloader unlock command, fastboot will refuse to proceed.
    //
    // Even if we're already forcefully setting the lock state to unlocked, let's
    // also patch this for the sake of completeness.
    FORCE_RETURN(0x41E1EED4, 1);

    // There's some weird check in fastboot_init() that probably reads the
    // unlock state from the proinfo partition and based on that it decides
    // whether to register fastboot commands or not.
    //
    // Since we want fastboot commands to always be available, we patch it
    // to always register them.
    FORCE_RETURN(0x41E1FF48, 0);

    // In order to get rid of the verified boot warning during boot, we basically
    // install a hook that replaces the function that gets called to print the
    // current verified boot state to UART and instead, we jump to our own shim.
    //
    // The shim overrides whatever verified boot state the bootloader tries to set
    // and forces it to green.
    PATCH_CALL(0x41E039B4, (void*)verified_boot_hook, TARGET_THUMB);

    // Show the current boot mode on screen when not performing a normal boot.
    // This is standard behavior in many LK images, but not in this one by default.
    //
    // Displaying the boot mode can be helpful for developers, as it provides
    // immediate feedback and can prevent debugging headaches.
    if (get_bootmode() != BOOTMODE_NORMAL
        && get_bootmode() != BOOTMODE_FASTBOOT) {
        show_bootmode(get_bootmode());
    }
}
