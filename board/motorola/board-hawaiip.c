//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for Motorola G22\n");

    uint32_t addr = 0;

    // This bootloader will only register flash & erase commands if the device
    // has secure boot disabled. This is read from fuses and can't be changed.
    //
    // Thankfully, LK has a getter function so we can just patch it to always
    // return 0, which makes LK think secure boot is disabled and register the
    // commands regardless of the actual SBC state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2360, 0xF2C1, 0x13C5, 0x6818);
    if (addr) {
        printf("Found get_hw_sbc at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // In addition to gated flash / erase commands, this bootloader also has
    // a hardcoded partition name blacklist, which prevents flashing certain
    // partitions like proinfo, nvram or nvdata.
    //
    // We patch the entry point of the first check to unconditionally jump to
    // the actual flashing routine, so we don't hit any restriction at all.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4989, 0x4620, 0x2207, 0x4479);
    if (addr) {
        printf("Found partition blacklist check at 0x%08X\n", addr);
        PATCH_MEM(addr, 0xE01B); // b #after the blacklist check
    }

    // Forcing get_vfy_policy to return 0 skips certificate
    // verification for all partitions and firmware images (boot,
    // recovery, dtbo, SCP, etc.) so the device can boot with
    // modified or unsigned images.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF63, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Same idea but for download policy, forcing get_dl_policy to return
    // 0 ensures no partition is marked as download-forbidden, so flashing
    // via fastboot works for all partitions.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF5D, 0xF000);
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}

void board_late_init(void) {
    printf("Entering late init for Motorola G22\n");

    uint32_t addr = 0;

    // On unlocked devices, LK shows an orange state warning during boot
    // that also introduces an unnecessary 5 second delay. Forcing the
    // function to return 0 skips both the warning and the delay.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B, 0x681B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Disables the dm-verity corruption warning shown during boot when
    // the device is unlocked. Without this patch, the user gets a scary
    // "Your device is corrupt" screen that waits for a power button
    // press and powers off after 5 seconds if ignored.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}