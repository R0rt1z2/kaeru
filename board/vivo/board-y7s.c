//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for Vivo Y7s\n");

    // This function would normally return the seccfg lock state
    // from the security configuration partition. However, Vivo
    // decided to relock seccfg at some point during the bootchain,
    // meaning that even if the device was previously unlocked, it
    // will appear as locked by the time LK checks the state.
    //
    // To work around this, we force the function to always return
    // the unlocked state.
    PATCH_MEM(0x4C463208,
        0x2303,  // movs  r3, #0x3
        0x6003,  // str   r3, [r0]
        0x2000,  // movs  r0, #0x0
        0x4770,  // bx    lr
        0xBF00   // nop
    );

    // Forcing get_vfy_policy to return 0 skips certificate
    // verification for all partitions and firmware images (boot,
    // recovery, dtbo, SCP, etc.) so the device can boot with
    // modified or unsigned images.
    FORCE_RETURN(0x4C415A1C, 0);

    // Same idea but for download policy, forcing get_dl_policy to return
    // 0 ensures no partition is marked as download-forbidden, so flashing
    // via fastboot works for all partitions.
    FORCE_RETURN(0x4C415A28, 0);

    // This is some sort of Vivo backdoor that seems to control whether AVB
    // verification is enforced or not. Forcing it to return 0 relaxes AVB
    // verification and allows the device to boot with modified boot and
    // recovery images.
    FORCE_RETURN(0x4C459C80, 0);
}

void board_late_init(void) {
    printf("Entering late init for Vivo Y7s\n");

    // On unlocked devices, LK shows an orange state warning during boot
    // that also introduces an unnecessary 5 second delay. Forcing the
    // function to return 0 skips both the warning and the delay.
    FORCE_RETURN(0x4C43F300, 0);

    // Disables the dm-verity corruption warning shown during boot when
    // the device is unlocked. Without this patch, the user gets a scary
    // "Your device is corrupt" screen that waits for a power button
    // press and powers off after 5 seconds if ignored.
    FORCE_RETURN(0x4C45A298, 0);
}
