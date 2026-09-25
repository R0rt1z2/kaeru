//
// SPDX-FileCopyrightText: 2026 hipexscape <abhinav.115260@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

static void spoof_lock_state(void) {
    uint32_t addr = 0;

    // With the device reporting as locked, the fastboot dispatcher
    // gates every command on lock/secure state, so stock commands
    // (flash, erase, ...) get rejected too. Its pre-handler gate has
    // several skip branches; replace the first one with a direct jump
    // to the handler dispatch so every command runs regardless of lock
    // state.
    //
    // The dispatch reads only the command struct and stack, so it's
    // safe to skip the gate.
    //
    // Dandelion's equivalent fastboot dispatcher has not been mapped
    // yet, so no dispatcher patch is applied here.

    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");

    if (!spoofing) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    printf("Bootloader lock status spoofing enabled, applying patches.\n");

    // On most MediaTek devices, lock state is fetched by calling
    // seccfg_get_lock_state() directly. Some vendors (e.g. Xiaomi)
    // add a wrapper that also checks a custom lock mechanism.
    //
    // Xiaomi's custom lock implementation can override MediaTek's
    // standard seccfg state, so patch both lock-state providers to
    // the common Kaeru get_lock_state() implementation. This keeps
    // all callers consistent and makes the device report the locked
    // state while spoofing is enabled.
    addr = 0x48062B70;
    printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
    PATCH_BRANCH(addr, (void *)get_lock_state);

    addr = 0x4801F664;
    printf("Found custom_get_lock_state at 0x%08X\n", addr);
    PATCH_BRANCH(addr, (void *)get_lock_state);

    NOP(0x480256BC, 1);

    // AVB adds device state info to the kernel cmdline, but it
    // keeps showing "unlocked" even when we want it to say "locked".
    // This patch forces the cmdline to always use the "locked"
    // string instead of checking the actual device state.
    addr = SEARCH_PATTERN(LK_START, LK_END,
                          0xDBF8, 0x2430, 0x5846, 0x0847,
                          0x0128, 0xF4D0, 0xA0B9, 0x079B,
                          0x3BBB);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);

        // NOP out the code that checks the actual device state,
        // forcing libavb to always use the "locked" string.
        NOP(addr + 0x10, 2);
    }

    // Hook cmdline_pre_process so handle_recovery_boot() can flip
    // verifiedbootstate before LK hands the cmdline to the kernel.
    addr = 0x48024352;
    printf("Found cmdline_pre_process at 0x%08X\n", addr);
    PATCH_CALL(addr, (void *)handle_recovery_boot, TARGET_THUMB);
}

// kaeru bootloader lock spoofing control command.
FASTBOOT_CMD(bldr_spoof, "oem bldr_spoof", cmd_spoof_bootloader_lock, 1);

void board_early_init(void) {
    printf("Entering early init for Redmi 9A/Redmi 10A/Redmi 10A Sport/9AT/9i/9A Sport\n");

    uint32_t addr = 0;

    // Regardless of whether spoofing is enabled, we always need to
    // disable image authentication. The user may just be using this
    // custom LK to unlock their device, or they may be spoofing
    // where the locked state would enforce verification.
    //
    // Forcing get_vfy_policy to return 0 skips certificate
    // verification for all partitions and firmware images (boot,
    // recovery, dtbo, SCP, etc.) so the device can boot with
    // modified or unsigned images.
    addr = 0x48018D98;
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Since we're spoofing the LKS_STATE as locked, get_dl_policy would normally
    // restrict fastboot downloads/flashing based on security policy. Force it to
    // return 0 to bypass these restrictions and allow unrestricted flashing.
    addr = 0x48018DA4;
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Since we report the device as locked, AVB treats a bad signature,
    // hash mismatch, rollback or rejected key as fatal and won't boot
    // modified or resigned images. Force it into "allow verification
    // error" mode, the same path AVB uses when unlocked, so it tolerates
    // any vbmeta and still builds slot_data and the kernel cmdline.
    //
    // We patch avb_slot_verify to force that flag on. This covers the
    // recoverable errors above. Structurally invalid vbmeta is still
    // rejected by AVB, but get_vfy_policy above already ungates boot.
    //
    // On Dandelion the flag is checked from r6 instead of r5:
    //
    //     and r3, r6, #1
    //     eor r10, r3, #1
    //
    // Replace the first instruction with mov.w r3, #1.
    addr = SEARCH_PATTERN(LK_START, LK_END,
                          0xF006, 0x0301, 0xF083, 0x0A01);
    if (addr) {
        printf("Found avb_slot_verify allow-error gate at 0x%08X\n", addr);

        // and r3, r6, #1  ->  mov.w r3, #1
        PATCH_MEM(addr, 0xF04F, 0x0301);
    }

    // The environment area isn't initialized yet when board_early_init
    // runs, so any get_env calls would return NULL at this stage. We
    // hook a printf call in platform_init that runs right after env
    // initialization completes, it's a convenient entry point since
    // the call itself is non-essential and we need the env to be ready
    // before applying our lock state patches.
    addr = 0x48002A82;
    printf("Found env_init_done at 0x%08X\n", addr);
    PATCH_CALL(addr, (void *)spoof_lock_state, TARGET_THUMB);
}

void board_late_init(void) {
    printf("Entering late init for Redmi 9A/Redmi 10A/Redmi 9AT/Redmi 9i/Redmi 9A Sport\n");

    uint32_t addr = 0;

    // Disables the warning shown during boot when the device is unlocked and
    // the dm-verity state is corrupted. This behaves like the previous lock
    // state warnings, visual only, with no real impact.
    //
    // Same approach: patch the function to always return 0.
    addr = SEARCH_PATTERN(LK_START, LK_END,
                          0x2802, 0xD000, 0x4770);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }
}
