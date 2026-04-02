//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define CMDLINE1_ADDR 0x483c52b8
#define CMDLINE2_ADDR 0x483c5abc

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
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4B60, 0x4D61, 0x447B, 0x4C61);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);

        // "not support on security" call
        NOP(addr + 0x162, 2);

        // "not allowed in locked state" call
        NOP(addr + 0x17c, 2);

        // Jump directly to the command handler
        PATCH_MEM(addr + 0xF8, 0xE006);
    }

    // This is an unused function in this LK binary whose body we
    // repurpose as a trampoline area. Each call target that sits
    // beyond BL's ±16 MB range gets a small relay here:
    //   ldr.w ip, [pc, #4]  (0xF8DF, 0xC004)
    //   bx  ip              (0x4760)
    //   nop                 (0xBF00)
    //   .word <target | 1>
    // 12 bytes per trampoline, laid out at stub+4, +16, +28…
    //
    // We use ip (r12) instead of r0 because r0-r3 carry function
    // arguments, clobbering r0 corrupts the first parameter
    // (e.g. the lock_state pointer), causing a data abort.
    // ip is designated as a scratch register by the ARM calling
    // convention, so it's safe to trash here.
    uint32_t stub = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x1E44, 0xB085);

    // Xiaomi uses a custom lock state implementation based on RPMB.
    // They expect the device to have a signature stored inside the
    // RPMB partition that indicates whether the device has been
    // officially unlocked or not. This signature has priority over
    // MediaTek's standard lock state mechanism (seccfg lock state),
    // so if you try to unlock the device through tools like MTKClient,
    // it will still remain locked.
    //
    // Most LK APIs call sec_get_lock_state_adapter(), which is a
    // wrapper that calls both seccfg_get_lock_state() and
    // custom_get_lock_state(), then, based on these results,
    // determines the final lock state.
    //
    // The idea is that people can benefit from this custom LK to
    // unlock their devices without having to worry about the custom
    // lock state, but at the same time, we need to provide the
    // ability to spoof the lock state for Play Integrity related
    // uses.
    uint32_t get_lock_state_tramp = 0;
    if (stub) {
        get_lock_state_tramp = stub + 4;
        uint32_t target = (uint32_t)get_lock_state | 1;
        WRITE32(get_lock_state_tramp,     0xC004F8DF);  // ldr.w ip, [pc, #4]
        WRITE16(get_lock_state_tramp + 4, 0x4760);      // bx ip
        WRITE16(get_lock_state_tramp + 6, 0xBF00);      // nop
        WRITE32(get_lock_state_tramp + 8, target);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF051, 0xF958, 0x4605, 0xB9B8);
    if (addr && get_lock_state_tramp) {
        printf("Found seccfg_get_lock_state call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)get_lock_state_tramp, TARGET_THUMB);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF000, 0xF9FD, 0xB9E0, 0x9B01);
    if (addr && get_lock_state_tramp) {
        printf("Found custom_get_lock_state call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)get_lock_state_tramp, TARGET_THUMB);
    }

    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");

    if (!spoofing) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    printf("Bootloader lock status spoofing enabled, applying patches.\n");

    // AVB adds device state info to the kernel cmdline, but it
    // keeps showing "unlocked" even when we want it to say "locked".
    // This patch forces the cmdline to always use the "locked"
    // string instead of checking the actual device state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4691, 0xB0AB);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);

        // NOP out the code that checks the actual device state,
        // forcing libavb to always use the "locked" string.
        NOP(addr + 0x9E, 4);
    }

    // When booting into recovery, we need to ensure verifiedbootstate
    // is set to "orange" so fastbootd detects the device as unlocked
    // and allows flashing. We also patch a few other cmdline params
    // (secureboot, device_state) as a precaution in case stock
    // recovery checks them as well.
    uint32_t recovery_boot_tramp = 0;
    if (stub) {
        recovery_boot_tramp = stub + 16;
        uint32_t target = (uint32_t)handle_recovery_boot | 1;
        WRITE32(recovery_boot_tramp,     0xC004F8DF);  // ldr.w ip, [pc, #4]
        WRITE16(recovery_boot_tramp + 4, 0x4760);      // bx ip
        WRITE16(recovery_boot_tramp + 6, 0xBF00);      // nop
        WRITE32(recovery_boot_tramp + 8, target);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF017, 0xF93D, 0xF001, 0xFAB9);
    if (addr && recovery_boot_tramp) {
        printf("Found cmdline_pre_process at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)recovery_boot_tramp, TARGET_THUMB);
    }

    // AVB verifies vbmeta public keys in two places: once for the main
    // vbmeta image (validate_vbmeta_public_key) and once for chained
    // vbmeta images (avb_safe_memcmp against the expected key). Both
    // reject the boot if the key doesn't match, causing the "Public key
    // used to sign data rejected" error. We patch both checks so any
    // key is accepted regardless.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF47F, 0xAE71, 0xE68D, 0xF8DD);
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
    printf("Entering early init for Redmi Note 12 Pro/Pro+/Discovery 5G/Pro+ 5G\n");


    uint32_t addr = 0;

    // Regardless of whether spoofing is enabled, we always need to disable
    // image authentication. The user may just be using this custom LK to
    // unlock (bypassing Xiaomi's RPMB lock), or they may be spoofing where
    // the locked state would enforce verification.
    //
    // Forcing get_vfy_policy to return 0 skips certificate verification for
    // all partitions and firmware images (boot, recovery, dtbo, SCP, etc.)
    // so the device can boot with modified or unsigned images.
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

    // This function handles certificate chain and hash verification for
    // modem-related images (md1rom, md3rom, etc.) during the modem loading
    // process. Same idea as above — force it to return 0 so modem images
    // can be loaded without passing signature verification.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0x460A, 0x4604);
    if (addr) {
        printf("Found ccci_ld_md_sec_ptr_hdr_verify at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // We need to hook platform_init right after env initialization to apply
    // lock state patches as early as possible. Problem is, our payload is
    // ~1.9GB away from the call site, way beyond BL's ±16MB range.
    //
    // To get around this, we use a two-hop trampoline. The profiling printf
    // after ENV init is just timing noise, so we replace its BL with one
    // that jumps to a relay stub. The stub sits in the dead code left over
    // from the previous patch above (those bytes will never execute), and
    // does the actual long jump to spoof_lock_state.
    uint32_t hook = SEARCH_PATTERN(LK_START, LK_END, 0xF03E, 0xF82E, 0x6823, 0x2000);
    if (hook && addr) {
        printf("Found env_init_done at 0x%08X\n", hook);

        uint32_t stub = addr + 4;
        uint32_t target = (uint32_t)spoof_lock_state | 1;
        WRITE16(stub,     0x4800);  // ldr r0, [pc, #0]
        WRITE16(stub + 2, 0x4700);  // bx r0
        WRITE32(stub + 4, target);

        PATCH_CALL(hook, (void *)stub, TARGET_THUMB);
    }

    // Register our custom fastboot commands.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 0);
}

void board_late_init(void) {
    printf("Entering late init for Redmi Note 12 Pro/Pro+/Discovery 5G/Pro+ 5G\n");

    uint32_t addr = 0;

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
