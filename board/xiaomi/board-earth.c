//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define CMDLINE1_ADDR 0x4C56FCF4
#define CMDLINE2_ADDR 0x4C5704F8

static int set_env(char *name, char *value) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x2200, 0xF7FF, 0xBF0D, 0xBF00, 0xE92D);
    if (addr) {
        printf("Found set_env at 0x%08X\n", addr);
        return ((int (*)(char *name, char *value))(addr | 1))(name, value);
    }
    return -1;
}

static char *get_env(char *name) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4602, 0x4604, 0x4909, 0x2300);
    if (addr) {
        printf("Found get_env at 0x%08X\n", addr);
        return ((char* (*)(char *name))(addr | 1))(name);
    }
    return NULL;
}

static int is_spoofing_enabled(void) {
    const char *env_value = get_env(KAERU_ENV_BLDR_SPOOF);
    return env_value && strcmp(env_value, "1") == 0;
}

static int get_lock_state(uint32_t *lock_state) {
    int spoofing = is_spoofing_enabled();
    printf("Attempted to get lock state, spoofing is %s\n",
            spoofing ? "enabled" : "disabled");

    *lock_state = spoofing ? LKS_LOCK : LKS_UNLOCK;
    return 0;
}

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

static void cmd_spoof_bootloader_lock(const char* arg, void* data, unsigned sz) {
    int status = is_spoofing_enabled();
    const char *option = arg + 1;
    int target = -1;

    if (!strcmp(option, "on"))
        target = 1;
    else if (!strcmp(option, "off"))
        target = 0;

    if (target != -1) {
        if (status != target) {
            set_env(KAERU_ENV_BLDR_SPOOF, target ? "1" : "0");
            fastboot_publish("is-spoofing", target ? "1" : "0");
            fastboot_info(target ?
                "Bootloader spoofing enabled." :
                "Bootloader spoofing disabled.");
            fastboot_info("A factory reset may be required.");
        } else {
            fastboot_info(target ?
                "Bootloader spoofing is already enabled." :
                "Bootloader spoofing is already disabled.");
        }
        fastboot_okay("");
        return;
    }

    if (!strcmp(option, "status")) {
        fastboot_info(status ?
            "Bootloader spoofing is currently enabled." :
            "Bootloader spoofing is currently disabled.");
        fastboot_info(status ?
            "Device is currently spoofed as bootloader locked." :
            "Device is not being spoofed as bootloader locked.");
        fastboot_okay("");
        return;
    }

    fastboot_info("kaeru bootloader lock spoofing control");
    fastboot_info("");
    fastboot_info("When enabled, device reports as 'locked' to TEE");
    fastboot_info("while maintaining full fastboot and root capabilities.");
    fastboot_info("");
    fastboot_info("Commands:");
    fastboot_info("  on     - Enable spoofing (reboot required)");
    fastboot_info("  off    - Disable spoofing (reboot required)");
    fastboot_info("  status - Show current state");
    fastboot_fail("Usage: fastboot oem bldr_spoof <on|off|status>");
}

static void spoof_lock_state(void) {
    uint32_t addr = 0;

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
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF048, 0xFE72, 0x4604, 0xB9C0);
    if (addr) {
        printf("Found seccfg_get_lock_state call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)get_lock_state, TARGET_THUMB);
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF000, 0xFA13, 0xB9A0, 0x9B01);
    if (addr) {
        printf("Found custom_get_lock_state call at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)get_lock_state, TARGET_THUMB);
    }

    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");

    if (!spoofing) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    printf("Bootloader lock status spoofing enabled, applying patches.\n");

    // When we spoof the lock state to appear "locked", fastboot
    // starts rejecting commands with "not support on security" and
    // "not allowed in locked state" errors. Since the device is
    // actually unlocked underneath, the security checks are just
    // being overly paranoid.
    //
    // This patch removes both security gates so fastboot commands
    // work regardless of what the spoofed lock state reports.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4C94, 0xE92D, 0x4880, 0xB08D);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);

        // "not support on security" call
        NOP(addr + 0x22A, 2);

        // "not allowed in locked state" calls
        NOP(addr + 0x24A, 2);
        NOP(addr + 0x23E, 2);

        // Jump directly to the command handler
        PATCH_MEM(addr + 0x19A, 0xE008);
    }

    // AVB adds device state info to the kernel cmdline, but it
    // keeps showing "unlocked" even when we want it to say "locked".
    // This patch forces the cmdline to always use the "locked"
    // string instead of checking the actual device state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4691, 0xF102);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);

        // NOP out the code that checks the actual device state,
        // forcing libavb to always use the "locked" string.
        NOP(addr + 0x9C, 4);
    }

    // When booting into recovery, we need to ensure verifiedbootstate
    // is set to "orange" so fastbootd detects the device as unlocked
    // and allows flashing. We also patch a few other cmdline params
    // (secureboot, device_state) as a precaution in case stock
    // recovery checks them as well.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF012, 0xFA54, 0xF001, 0xFB04);
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
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF47F, 0xAE6B, 0xE688, 0xF8DD);
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
    printf("Entering early init for Redmi 12C / POCO C55\n");

    uint32_t addr = 0;

    // Regardless of whether spoofing is enabled, we always need to disable
    // image authentication. The user may just be using this custom LK to
    // unlock (bypassing Xiaomi's RPMB lock), or they may be spoofing where
    // the locked state would enforce verification.
    //
    // Forcing get_vfy_policy to return 0 skips certificate verification for
    // all partitions and firmware images (boot, recovery, dtbo, SCP, etc.)
    // so the device can boot with modified or unsigned images.
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

    // This function handles certificate chain and hash verification for
    // modem-related images (md1rom, md3rom, etc.) during the modem loading
    // process. Same idea as above — force it to return 0 so modem images
    // can be loaded without passing signature verification.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0x460A, 0x4604);
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
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF037, 0xF9A3, 0x6823, 0x2001);
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void *)spoof_lock_state, TARGET_THUMB);
    }

    // Register our custom fastboot commands.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);
}

void board_late_init(void) {
    printf("Entering late init for Redmi 12C / POCO C55\n");

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