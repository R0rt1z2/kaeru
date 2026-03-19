//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for Meizu MX6\n");

    // LK has one security gate in the fastboot command processor that
    // reject commands with "not support on security" and "not allowed
    // in locked state" errors. When spoofing lock state, these would
    // block all fastboot operations despite the device being actually
    // unlocked underneath.
    //
    // Even without spoofing, we patch these out as a safety measure
    // since OEM-specific checks could still interfere with fastboot
    // commands in unexpected ways.
    NOP(0x46020F52, 2); // "not support on security" call
    PATCH_MEM(0x46020EFC, 0xE001); // Jump directly to command handler

    // This is for visual consistency
    strcpy((char*)0x46034948, " => FASTBOOT mode (kaeru)...\n");

    // This function would normally return the seccfg lock state
    // from the security configuration partition. However, Meizu
    // decided to relock seccfg at some point during the bootchain,
    // meaning that even if the device was previously unlocked, it
    // will appear as locked by the time LK checks the state.
    //
    // To work around this, we force the function to always return
    // the unlocked state.
    PATCH_MEM(0x4602B614,
        0x2303,  // movs  r3, #0x3
        0x6003,  // str   r3, [r0]
        0x2000,  // movs  r0, #0x0
        0x4770,  // bx    lr
        0xBF00   // nop
    );

    // This function is some kind of security check that gets called
    // during the flash handler. If it returns non-zero, flashing is
    // either blocked entirely with "Permission Denied!" or goes
    // through per-partition download permission checks.
    //
    // Forcing it to return zero skips all of that and lets us flash
    // any partition without restrictions.
    FORCE_RETURN(0x4601AF98, 0);

    // Forcing get_vfy_policy to return 0 skips certificate
    // verification for all partitions and firmware images (boot,
    // recovery, dtbo, SCP, etc.) so the device can boot with
    // modified or unsigned images.
    FORCE_RETURN(0x4601AC50, 0);

    // Same idea but for download policy, forcing get_dl_policy to return
    // 0 ensures no partition is marked as download-forbidden, so flashing
    // via fastboot works for all partitions.
    FORCE_RETURN(0x4601AC64, 0);
}

void board_late_init(void) {
    printf("Entering late init for Meizu MX6\n");

    bootmode_t mode = get_bootmode();

    if (mode != BOOTMODE_NORMAL 
        && mode != BOOTMODE_POWEROFF_CHARGING 
        && mode != BOOTMODE_FASTBOOT && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal boot.
        // This is standard behavior in many LK images, but not in this one by default.
        show_bootmode(mode);
    }
}
