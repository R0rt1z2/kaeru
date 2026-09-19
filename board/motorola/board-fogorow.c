//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <roger@r0rt1z2.com>
//                         2026 Shomy <git@itssho.my>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

static int is_brom_cmd_disabled(void) {
    return (*(volatile uint32_t*)(0x11CE0060) >> 8) & 1;
}

int is_partition_protected(const char* partition) {
    if (!partition || *partition == '\0') return 1;

    while (*partition && ISSPACE(*partition)) {
        partition++;
    }

    if (*partition == '\0') return 1;

    // These partitions are critical—flashing them incorrectly can lead to a hard brick.
    // To prevent accidental damage, we mark them as protected and block write access.
    if (strcmp(partition, "boot0") == 0 || strcmp(partition, "boot1") == 0 ||
        strcmp(partition, "boot2") == 0 || strcmp(partition, "partition") == 0 ||
        strcmp(partition, "preloader") == 0 || strcmp(partition, "preloader_a") == 0 ||
        strcmp(partition, "preloader_b") == 0) {
        return 1;
    }

    return 0;
}

void cmd_flash(const char* arg, void* data, unsigned sz) {
    if (is_partition_protected(arg)) {
        fastboot_fail("Partition is protected");
        return;
    }

    ((void (*)(const char* arg, void* data, unsigned sz))(0x4C439728 | 1))(arg, data, sz);
}

void cmd_erase(const char* arg, void* data, unsigned sz) {
    if (is_partition_protected(arg)) {
        fastboot_fail("Partition is protected");
        return;
    }

    ((void (*)(const char* arg, void* data, unsigned sz))(0x4C439308 | 1))(arg, data, sz);
}

static void post_env_process(void) {
    // On most MediaTek devices, lock state is fetched by calling
    // seccfg_get_lock_state() directly. Some vendors (e.g. Xiaomi)
    // add a wrapper that also checks a custom lock mechanism, but
    // this device does not have one. All callers reach
    // seccfg_get_lock_state() through a single b.w thunk.
    //
    // Rather than patching the function body directly, we redirect
    // the thunk to our own get_lock_state(), keeping the original
    // function intact while covering all call sites with a single
    // patch.
    PATCH_BRANCH(0x4C425A64, (void*)get_lock_state);

    // LK has two security gates in the fastboot command processor that
    // reject commands with "not support on security" and "not allowed
    // in locked state" errors. When spoofing lock state, these would
    // block all fastboot operations despite the device being actually
    // unlocked underneath.
    //
    // Even without spoofing, we patch these out as a safety measure
    // since OEM-specific checks could still interfere with fastboot
    // commands in unexpected ways.

    // "not support on security" call
    NOP(0x4C42ACE6, 2);

    // "not allowed in locked state" call
    NOP(0x4C42ACF2, 2);

    // Jump directly to command handler
    PATCH_MEM(0x4C42AC7C, 0xE006);

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

    // NOP out the code that checks the actual device state,
    // forcing libavb to always use the "locked" string.
    NOP(0x4C46ECEC, 4);

    // Hook cmdline_pre_process so handle_recovery_boot() can flip
    // verifiedbootstate before LK hands the cmdline to the kernel.
    PATCH_CALL(0x4C4290F4, (void*)handle_recovery_boot, TARGET_THUMB);

    // AVB verifies vbmeta public keys in two places: once for the main
    // vbmeta image (validate_vbmeta_public_key) and once for chained
    // vbmeta images (avb_safe_memcmp against the expected key). Both
    // reject the boot if the key doesn't match, causing the "Public key
    // used to sign data rejected" error. We patch both checks so any
    // key is accepted regardless.

    // The chain key check first compares key lengths before calling
    // memcmp. If lengths differ, it skips memcmp and falls straight
    // to the error path. Change "cmp r2, r3" to "cmp r3, r3" so the
    // length check always succeeds, allowing execution to reach the
    // memcmp path (which we NOP below).
    PATCH_MEM(0x4C4713BC, 0x451B);

    // NOP the bne.w that rejects mismatched chained vbmeta keys,
    // falling through to the success path unconditionally.
    NOP(0x4C4716E8, 2);

    // Replace "cmp r3, #0" with "movs r3, #1" so key_is_trusted
    // is always nonzero and the following bne.w takes the success
    // branch.
    PATCH_MEM(0x4C47175A, 0x2301);
}

void board_early_init(void) {
    printf("Entering early init for Motorola G24\n");

    // The default flash and erase commands perform no safety checks, allowing
    // writes to critical partitions, like the Preloader, which can easily brick
    // the device.
    //
    // To prevent this, we disable the original handlers and replace them with
    // custom wrappers that verify whether the target partition is protected.
    NOP(0x4C42B264, 2);
    NOP(0x4C42B278, 2);

    // Disables the `fastboot flashing lock` command to prevent accidental hard bricks.
    //
    // Locking while running a custom or modified LK image can leave the device in an
    // unbootable state after reboot, since the expected secure environment is no longer
    // present.
    NOP(0x4C42B4BE, 2);

    // Tinno (the ODM) added a post-`app()` check that forcefully relocks the device
    // if it was previously unlocked, completely defeating the purpose of unlocking.
    //
    // Fortunately, they don’t verify LK integrity, so we can bypass this check entirely
    // by patching the function to return immediately before it does anything.
    FORCE_RETURN(0x4C437430, 0);

    // Regardless of whether spoofing is enabled, we always need to
    // disable image authentication. The user may just be using this
    // custom LK to unlock their device, or they may be spoofing
    // where the locked state would enforce verification.
    //
    // Forcing get_vfy_policy to return 0 skips certificate
    // verification for all partitions and firmware images (boot,
    // recovery, dtbo, SCP, etc.) so the device can boot with
    // modified or unsigned images.
    FORCE_RETURN(0x4C417A60, 0);

    // This function determines whether the device is in a secure state.
    // When it returns true, fastboot operations such as flash, erase, and
    // lock/unlock are blocked with "[secure] not allow".
    //
    // We patch it to always return false so that all fastboot commands
    // remain accessible regardless of the device's actual secure state.
    FORCE_RETURN(0x4C433BC0, 0);

    // tinno_is_facmode() reads a flag from oem_mfd to determine whether
    // factory mode is active. It is checked in over 20 call sites across
    // LK, gating fastboot commands and influencing boot behavior.
    //
    // Forcing it to always return false prevents factory mode from ever
    // being considered active, regardless of what oem_mfd contains.
    FORCE_RETURN(0x4C434DF8, 0);

    // This function updates the oem_mfd partition, which is later used to
    // decide whether the device should boot into factory mode, even if the user
    // requested a normal boot.
    //
    // The update is triggered based on a hardware register (likely a secure fuse
    // or similar). If that register isn't set to 1, which won't happen after
    // patching get_hw_sbc, the bootloader assumes the device is insecure and
    // repeatedly forces factory mode.
    //
    // To prevent this loop, we disable the function entirely.
    FORCE_RETURN(0x4C434EA0, 0);

    // The environment area isn't initialized yet when board_early_init
    // runs, so any get_env calls would return NULL at this stage. We
    // hook a printf call in platform_init that runs right after env
    // initialization completes, it's a convenient entry point since
    // the call itself is non-essential and we need the env to be ready
    // before applying our lock state patches.
    PATCH_CALL(0x4C403AE8, (void*)post_env_process, TARGET_THUMB);

    // Register our custom flash and erase commands to replace the original ones.
    fastboot_register("flash:", cmd_flash, 1);
    fastboot_register("erase:", cmd_erase, 1);
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 0);
    fastboot_publish("brom-usbdl-disabled", is_brom_cmd_disabled() == 1 ? "yes" : "no");
}

void board_late_init(void) {
    printf("Entering late init for Motorola G24\n");

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    //
    // This patch get rid of the delay and the warning by forcing the
    // function that holds the logic to always return 0 and therefore
    // not executing the code that shows the warning.
    FORCE_RETURN(0x4C45079C, 0);

    // As an extra safeguard, we manually disable factory mode by calling the
    // relevant update function directly. This ensures the mode is turned off,
    // even if it was set by another part of the bootloader.
    ((int (*)(int, int))(0x4C434E14 | 1))(0, 0);
}
