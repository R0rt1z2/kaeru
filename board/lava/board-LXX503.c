//
// SPDX-FileCopyrightText: 2026 fantom3031 <mpiven69@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define CMDLINE1_ADDR 0x482E5D08
#define CMDLINE2_ADDR 0x482E650C

static void patch_cmdline(char *cmdline) {
    cmdline_replace(cmdline, "androidboot.verifiedbootstate=",
                    "green", "orange");
    cmdline_replace(cmdline, "androidboot.secureboot=",
                    "1", "0");
    cmdline_replace(cmdline, "androidboot.vbmeta.device_state=",
                    "locked", "unlocked");
}

static void handle_recovery_boot(void) {
    if (get_bootmode() != BOOTMODE_RECOVERY)
        return;

    printf("Recovery boot detected, modifying cmdline for unlocked state.\n");

    static const uint32_t cmdline_addrs[] = { CMDLINE1_ADDR, CMDLINE2_ADDR };
    for (int i = 0; i < ARRAY_SIZE(cmdline_addrs); i++) {
        printf("Patching cmdline at 0x%08X\n", cmdline_addrs[i]);
        patch_cmdline((char *)cmdline_addrs[i]);
    }
}

static void spoof_lock_state(void) {
    uint32_t addr = 0;

    // When we spoof the lock state to appear "locked", fastboot starts rejecting 
    // commands with "not support on security" and "not allowed in locked state" 
    // errors. This is annoying since the device is actually unlocked underneath, 
    // the security checks are just being overly paranoid.
    //
    // This patch removes both security gates so fastboot commands work regardless
    // of what the spoofed lock state reports.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4880, 0xB087, 0x4D5A);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);
        
        // NOP the error message calls
        NOP(addr + 0x15A, 2);  // "not support on security" call
        NOP(addr + 0x166, 2);  // "not allowed in locked state" call
        
        // Jump directly to command handler
        PATCH_MEM(addr + 0xF0, 0xE006);  // b +12 (branch to command handler)
    }

    // Need to spoof the LKS_STATE as "locked" for certain scenarios, but still
    // return success so other parts of the system don't freak out. This makes
    // seccfg_get_lock_state always report lock_state=1 and return 2.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);
    if (addr) {
        printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
        PATCH_MEM(addr + 6, 
            0x2301,  // movs r3, #1
            0x6023,  // str r3, [r4, #0]
            0x2002,  // movs r0, #2
            0xbd10   // pop {r4, pc}
        );
    }

    // Force the secure boot state to ATTR_SBOOT_ENABLE (0x11). This controls whether
    // secure boot verification is enabled and is separate from the LKS_STATE above.
    // Setting it to 0x11 indicates secure boot is properly enabled.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4604, 0x2001, 0xF7FF);
    if (addr) {
        printf("Found get_sboot_state at 0x%08X\n", addr);
        PATCH_MEM(addr,
            0x2311,  // movs r3, #0x11
            0x6003,  // str r3, [r0,#0]
            0x2000,  // movs r0, #0
            0x4770   // bx lr
        );
    }

    // Since we're spoofing the LKS_STATE as locked, get_vfy_policy would normally
    // require partition verification during boot. Force it to return 0 to disable
    // this verification and allow booting with modified partitions.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF5F, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Same idea but for download policy, forcing get_dl_policy to return
    // 0 ensures no partition is marked as download-forbidden, so flashing
    // via fastboot works for all partitions.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF59, 0xF000);
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // AVB adds device state info to the kernel cmdline, but it keeps showing
    // "unlocked" even when we want it to say "locked". This patch forces
    // the cmdline to always use the "locked" string instead of checking
    // the actual device state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4691, 0xB0AB);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);
        
        // Find where in libavb the device state is first fetched and then stored,
        // then Nop out the code that checks the actual device state.
        // This forces libavb to always use the "locked" string.
        NOP(addr + 0x9E, 4);
    }

    // When booting into recovery, we need to ensure verifiedbootstate
    // is set to "orange" so fastbootd detects the device as unlocked
    // and allows flashing. We also patch a few other cmdline params
    // (secureboot, device_state) as a precaution in case stock
    // recovery checks them as well.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF00e, 0xFd79, 0xF001, 0xFa2d);
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
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF47F, 0xAE71, 0xE68d, 0xF8DD);
    if (addr) {
        printf("Found load_and_verify_vbmeta at 0x%08X\n", addr);

        // The chain key check first compares key lengths before calling
        // memcmp. If lengths differ, it skips memcmp and falls straight
        // to the error path. Change "cmp r2, r3" to "cmp r3, r3" so the
        // length check always succeeds, allowing execution to reach the
        // memcmp path (which we NOP below).
        PATCH_MEM(addr - 0x320, 0x451B);

        // NOP the bne.w that rejects mismatched chained vbmeta keys,
        // falling through to the success path unconditionally.
        NOP(addr, 2);

        // Replace "cmp r3, #0" with "movs r3, #1" so key_is_trusted
        // is always nonzero and the following bne.w takes the success
        // branch.
        PATCH_MEM(addr + 0x70, 0x2301);
    }
}

void board_early_init(void) {
    printf("Entering early init for Lava Blaze 5G\n");

    uint32_t addr = 0;

    // This function handles certificate chain and hash verification for
    // modem-related images (md1rom, md3rom, etc.) during the modem loading
    // process. Same idea as above — force it to return 0 so modem images
    // can be loaded without passing signature verification.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0x460A, 0x4604);
    if (addr) {
        printf("Found ccci_ld_md_sec_ptr_hdr_verify at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // The environment area is not yet initialized when board_early_init runs,
    // so environment variable checks will always return NULL at this stage.
    // To work around this timing issue, we hook into a printf call that executes
    // after environment initialization is complete and redirect it to our
    // spoof_lock_state function.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF032, 0xFA3A, 0x6823, 0x2000);
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void*)spoof_lock_state, TARGET_THUMB);
    }

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    // 
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}

void board_late_init(void) {
    printf("Entering late init for Lava Blaze 5G\n");
}
