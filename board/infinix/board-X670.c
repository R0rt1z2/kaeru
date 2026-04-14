//
// SPDX-FileCopyrightText: 2026 Your name <your.email@example.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define CMDLINE1_ADDR 0x4c50f600
#define CMDLINE2_ADDR 0x4c50fe0c

static void patch_cmdline(char *cmdline) {
    cmdline_replace(cmdline, "androidboot.verifiedbootstate=",
                    "green", "orange");
    cmdline_replace(cmdline, "androidboot.secureboot=",
                    "1", "0");
    cmdline_replace(cmdline, "androidboot.vbmeta.device_state=",
                    "locked", "unlocked");
}

static void handle_recovery_boot(void) {
    if (get_bootmode() != BOOTMODE_RECOVERY || !is_spoofing_enabled())
        return;

    printf("Recovery boot detected, modifying cmdline for unlocked state.\n");

    static const uint32_t cmdline_addrs[] = { CMDLINE1_ADDR, CMDLINE2_ADDR };
    for (int i = 0; i < ARRAY_SIZE(cmdline_addrs); i++) {
        printf("Patching cmdline at 0x%08X\n", cmdline_addrs[i]);
        patch_cmdline((char *)cmdline_addrs[i]);
    }
}

int sec_usbdl_enabled(void) {
    return 0;
}

unsigned int seclib_sec_boot_enabled(unsigned int) {
    return 0;
}

unsigned get_unlocked_status(void) {
    return 1;
}

static void spoof_lock_state(void) {
    uint32_t addr = 0;

    // When we spoof the lock state to appear "locked", fastboot
    // starts rejecting commands with "not support on security" and
    // "not allowed in locked state" errors. Since the device is
    // actually unlocked underneath, the security checks are just
    // being overly paranoid.
    //
    // This patch removes both security gates so fastboot commands
    // work regardless of what the spoofed lock state reports.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF03F, 0xFEE3, 0xB318, 0x68FB);// 3F F0 E3 FE 18 B3 FB 68 @ 4c429d1e
    if (addr) {
        printf("Found sec_usbdl_enabled call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)sec_usbdl_enabled, TARGET_THUMB);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF03F, 0xFECC, 0xB190, 0x68FB);// 3f f0 cc fe 90 b1 fb 68 @ 4c429d4c
    if (addr) {
        printf("Found sec_usbdl_enabled call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)sec_usbdl_enabled, TARGET_THUMB);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF040, 0xF97B, 0x2800, 0xD1D6);// 40 F0 7B F9 00 28 D6 D1 @ 4c429d6e
    if (addr) {
        printf("Found seclib_sec_boot_enabled call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)seclib_sec_boot_enabled, TARGET_THUMB);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF040, 0xF975, 0x2800, 0xD0E9);// 40 F0 75 F9 00 28 E9 D0 @ 4c429d7a
    if (addr) {
        printf("Found seclib_sec_boot_enabled call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)seclib_sec_boot_enabled, TARGET_THUMB);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF042, 0xFF12, 0xB148, 0x482C);// 42 F0 12 FF 48 B1 2C 48 @ 4C429D2C
    if (addr) {
        printf("Found get_unlocked_status call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)get_unlocked_status, TARGET_THUMB);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF042, 0xFEFB, 0xB1E8, 0x4821);// 42 F0 FB FE E8 B1 21 48 @ 4c429d5a
    if (addr) {
        printf("Found get_unlocked_status call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)get_unlocked_status, TARGET_THUMB);
    }

    // AVB adds device state info to the kernel cmdline, but it
    // keeps showing "unlocked" even when we want it to say "locked".
    // This patch forces the cmdline to always use the "locked"
    // string instead of checking the actual device state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4691, 0xF102);// 2D E9 F0 4F 91 46 02 F1 @ 4c4596ac
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);

        // NOP out the code that checks the actual device state,
        // forcing libavb to always use the "locked" string.
        NOP(addr + 0x9C, 4);
    }

    // Need to spoof the LKS_STATE as "locked" for certain scenarios, but still
    // return success so other parts of the system don't freak out. This makes
    // seccfg_get_lock_state always report lock_state=1 and return 2.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);// @ 4c46af04
    if (addr) {
        printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
        PATCH_MEM(addr + 6, 
            0x2301,  // movs r3, #1
            0x6023,  // str r3, [r4, #0]
            0x2002,  // movs r0, #2
            0xbd10   // pop {r4, pc}
        );
    }

    // Add kaeru props in fastboot props list.
    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");
    fastboot_publish("kaeru-ported-by", "qiratdahaf");

    if (!spoofing) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    // Force the secure boot state to ATTR_SBOOT_ENABLE (0x11). This controls whether
    // secure boot verification is enabled and is separate from the LKS_STATE above.
    // Setting it to 0x11 indicates secure boot is properly enabled.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4604, 0x2001, 0xF7FF);// @ 4c46a138
    if (addr) {
        printf("Found get_sboot_state at 0x%08X\n", addr);
        PATCH_MEM(addr,
            0x2311,  // movs r3, #0x11   - set value to 0x11
            0x6003,  // str r3, [r0,#0]  - store to *param_1
            0x2000,  // movs r0, #0      - return 0
            0x4770   // bx lr            - return
        );
    }

    // When booting into recovery, we need to ensure verifiedbootstate
    // is set to "orange" so fastbootd detects the device as unlocked
    // and allows flashing. We also patch a few other cmdline params
    // (secureboot, device_state) as a precaution in case stock
    // recovery checks them as well.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF00E, 0xFABE, 0xF001, 0xF90A);// 0E F0 BE FA 01 F0 0A F9 @ 4c428184
    if (addr) {
        printf("Found cmdline_pre_process at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)handle_recovery_boot, TARGET_THUMB);
    }

    // AVB verifies vbmeta public keys in two places: once for the main
    // vbmeta image (validate_vbmeta_public_key) and once for chained
    // vbmeta images (avb_safe_memcmp against the expected key). Both
    // reject the boot if the key doesn't match, causing the "Public key
    // used to sign data rejected" error. We patch both checks so any
    // key is accepted regardless.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF47F, 0xAE6B, 0xE688, 0xF8DD);// 7F F4 6B AE 88 E6 DD F8 @ 4c45c148
    if (addr) {
        printf("Found load_and_verify_vbmeta at 0x%08X\n", addr);

        // The chain key check first compares key lengths before calling
        // memcmp. If lengths differ, it skips memcmp and falls straight
        // to the error path. Change "cmp r2, r3" to "cmp r3, r3" so the
        // length check always succeeds, allowing execution to reach the
        // memcmp path (which we NOP below).
        PATCH_MEM(addr - 0x32C, 0x451B);

        // NOP the bne.w that rejects mismatched chained vbmeta keys,
        // falling through to the success path unconditionally.
        NOP(addr, 2);

        // Replace "cmp r3, #0" with "movs r3, #1" so key_is_trusted
        // is always nonzero and the following bne.w takes the success
        // branch.
        PATCH_MEM(addr + 0x72, 0x2301);
    }
}

void board_early_init(void) {
    printf("Entering early init for Infinix Note 12 G96 4G\n");

    uint32_t addr = 0;

    // Regardless of whether spoofing is enabled, we always need to disable
    // image authentication. The user may just be using this custom LK to
    // unlock (bypassing Xiaomi's RPMB lock), or they may be spoofing where
    // the locked state would enforce verification.
    //
    // Forcing get_vfy_policy to return 0 skips certificate verification for
    // all partitions and firmware images (boot, recovery, dtbo, SCP, etc.)
    // so the device can boot with modified or unsigned images.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF67, 0xF3C0);// 08 B5 FF F7 67 FF C0 F3 @ 4c418b20
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Same idea but for download policy, forcing get_dl_policy to return
    // 0 ensures no partition is marked as download-forbidden, so flashing
    // via fastboot works for all partitions.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF61, 0xF000);// 08 B5 FF F7 61 FF 00 F0 @ 4c418b2c
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // This function handles certificate chain and hash verification for
    // modem-related images (md1rom, md3rom, etc.) during the modem loading
    // process. Same idea as above — force it to return 0 so modem images
    // can be loaded without passing signature verification.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0x460A, 0x4604);// 2D E9 F0 41 0A 46 04 46 @ 4c44e168
    if (addr) {
        printf("Found ccci_ld_md_sec_ptr_hdr_verify at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // The environment area isn't initialized yet when board_early_init
    // runs, so any get_env calls would return NULL at this stage. We
    // hook a printf call in platform_init that runs right after env
    // initialization completes, it's a convenient entry point since
    // the call itself is non-essential and we need the env to be ready
    // before applying our lock state patches.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF033, 0xFE9E, 0x6823, 0x2000);// 33 F0 9E FE 23 68 00 20 @ 4c4029c4
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)spoof_lock_state, TARGET_THUMB);
    }

    // Register our custom fastboot commands.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 0);
}

void board_late_init(void) {
    printf("Entering late init for Infinix Note 12 G96 4G\n");

    uint32_t addr = 0;

    // Disables the dm-verity corruption warning shown during boot when
    // the device is unlocked. Without this patch, the user gets a scary
    // "Your device is corrupt" screen that waits for a power button
    // press and powers off after 5 seconds if ignored.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200);// 30 B5 83 B0 02 AB 00 22 @ 4C45F330
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // On unlocked devices, LK shows an orange state warning during boot
    // that also introduces an unnecessary 5 second delay. Forcing the
    // function to return 0 skips both the warning and the delay.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B);// 08 B5 0E 4B 7B 44 @ 4c44a3fc
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}



