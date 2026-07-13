//
// SPDX-FileCopyrightText: 2026 XTENSEI <xtensei.is.not.in.the.sudoers.file@protonmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

// ── Environment helpers (runtime SEARCH_PATTERN) ──

int set_env(char *name, char *value) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0x2200, 0xF7FF, 0xBF0D, 0xBF00);
    if (addr) {
        printf("Found set_env at 0x%08X\n", addr);
        return ((int (*)(char *name, char *value))(addr | 1))(name, value);
    }
    return -1;
}

char *get_env(char *name) {
    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4602, 0x4604, 0x4909);
    if (addr) {
        printf("Found get_env at 0x%08X\n", addr);
        return ((char* (*)(char *name))(addr | 1))(name);
    }
    return NULL;
}

// ── Direct LK fastboot_info for displaying text (uses separate state struct) ──
#define LK_INFO(msg) \
    ((void (*)(const char*))(CONFIG_FASTBOOT_INFO_ADDRESS | 1))(msg)

#define ENV_KEY_MAX_LEN 64
#define ENV_VAL_MAX_LEN 256
#define ENV_MSG_MAX_LEN (ENV_KEY_MAX_LEN + ENV_VAL_MAX_LEN + 16)

// ── Environment command (fastboot oem env get/set) ──

static int parse_env_args(const char *arg, char *key, size_t key_sz,
                          char *val, size_t val_sz) {
    int count = 0;
    while (*arg == ' ') arg++;
    if (*arg == '\0') return 0;
    size_t i = 0;
    while (*arg && *arg != ' ' && i < key_sz - 1) key[i++] = *arg++;
    key[i] = '\0'; count = 1;
    while (*arg == ' ') arg++;
    if (*arg == '\0') return count;
    i = 0;
    while (*arg && i < val_sz - 1) val[i++] = *arg++;
    val[i] = '\0'; count = 2;
    return count;
}

static int is_env_key_valid(const char *key) {
    if (!key || *key == '\0') return 0;
    for (const char *p = key; *p; p++) {
        char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')
            continue;
        return 0;
    }
    return 1;
}

static int match_subcmd(const char *arg, const char *cmd, int len) {
    return !strncmp(arg, cmd, len) && (arg[len] == ' ' || arg[len] == '\0');
}

static void cmd_env(const char *arg, void *data, unsigned sz) {
    char key[ENV_KEY_MAX_LEN] = {0};
    char val[ENV_VAL_MAX_LEN] = {0};
    char msg[ENV_MSG_MAX_LEN] = {0};
    (void)data; (void)sz;

    while (*arg == ' ') arg++;
    if (strlen(arg) > ENV_MSG_MAX_LEN) {
        fastboot_fail("Argument too long");
        return;
    }

    if (match_subcmd(arg, "get", 3)) {
        arg += 3;
        if (parse_env_args(arg, key, sizeof(key), val, sizeof(val)) < 1) {
            fastboot_fail("Usage: fastboot oem env get <key>");
            return;
        }
        if (!is_env_key_valid(key)) {
            fastboot_fail("Invalid key name");
            return;
        }
        char *result = get_env(key);
        if (!result) {
            npf_snprintf(msg, sizeof(msg), "'%s' not found", key);
            fastboot_fail(msg);
            return;
        }
        npf_snprintf(msg, sizeof(msg), "%s=%s", key, result);
        LK_INFO(msg);
        fastboot_okay("Done");
        return;
    }

    if (match_subcmd(arg, "set", 3)) {
        arg += 3;
        if (parse_env_args(arg, key, sizeof(key), val, sizeof(val)) < 2) {
            fastboot_fail("Usage: fastboot oem env set <key> <value>");
            return;
        }
        if (!is_env_key_valid(key)) {
            fastboot_fail("Invalid key name");
            return;
        }
        if (set_env(key, val) < 0) {
            npf_snprintf(msg, sizeof(msg), "Failed to set '%s'", key);
            fastboot_fail(msg);
            return;
        }
        npf_snprintf(msg, sizeof(msg), "%s=%s", key, val);
        LK_INFO(msg);
        fastboot_okay("Done");
        return;
    }

    LK_INFO("Subcommands: get <key>, set <key> <value>");
    fastboot_fail("Usage: fastboot oem env <get|set>");
}

// ── Bootloader lock spoofing command ──

static void cmd_spoof_bootloader_lock(const char* arg, void* data, unsigned sz) {
    uint32_t status = 0;
    const char* env_value = get_env(KAERU_ENV_BLDR_SPOOF);
    const char *option = arg + 1;
    status = (env_value && strcmp(env_value, "1") == 0) ? 1 : 0;

    if (option) {
        if (!strcmp(option, "off")) {
            if (status) {
                set_env(KAERU_ENV_BLDR_SPOOF, "0");
                fastboot_publish("is-spoofing", "0");
                LK_INFO("Bootloader spoofing disabled.");
            } else {
                LK_INFO("Bootloader spoofing is already disabled.");
            }
            fastboot_okay("Done");
            return;
        }

        if (!strcmp(option, "on")) {
            if (!status) {
                set_env(KAERU_ENV_BLDR_SPOOF, "1");
                fastboot_publish("is-spoofing", "1");
                LK_INFO("Bootloader spoofing enabled.");
            } else {
                LK_INFO("Bootloader spoofing is already enabled.");
            }
            fastboot_okay("Done");
            return;
        }

        if (!strcmp(option, "status")) {
            LK_INFO(status ?
                "Bootloader spoofing is currently enabled." :
                "Bootloader spoofing is currently disabled.");
            LK_INFO(status ?
                "Device is currently spoofed as bootloader locked." :
                "Device is not being spoofed as bootloader locked.");
            fastboot_okay("Done");
            return;
        }
    }

    LK_INFO("Commands: on|off|status");
    fastboot_fail("Usage: fastboot oem bldr_spoof <on|off|status>");
}

// ── Environment init hook ──

// Runs after environment initialization completes (via PATCH_CALL on a
// printf in platform_init). At this point get_env() returns real values,
// so we can conditionally apply lock state patches based on the spoofing
// env variable.
static void spoof_lock_state(void) {
    uint32_t addr = 0;

    const char* env_value = get_env(KAERU_ENV_BLDR_SPOOF);
    int spoofing = (env_value && strcmp(env_value, "1") == 0) ? 1 : 0;

    fastboot_publish("is-spoofing", spoofing ? "1" : "0");

    if (!spoofing) {
        printf("Bootloader lock status spoofing disabled.\n");
        return;
    }

    printf("Bootloader lock status spoofing enabled, applying patches.\n");

    // Makes seccfg_get_lock_state report LKS_LOCK (4) so TEE and Android
    // detect the device as locked. This triggers the "format data" prompt
    // on next boot when the lock state changes from unlocked to locked.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);
    if (addr) {
        printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
        PATCH_MEM(addr + 6,
            0x2304,  // movs r3, #4      - LKS_LOCK
            0x6023,  // str r3, [r4, #0]  - *arg = lock_state
            0x2000,  // movs r0, #0       - return 0
            0xBD10   // pop {r4, pc}      - return
        );
    }

    // AVB adds device state info to the kernel cmdline based on the
    // actual lock state. When we're spoofing locked, AVB would now
    // correctly write "locked" to the cmdline, which is what we want.
    // This patch ensures the cmdline always reflects the correct state
    // by NOP-ing out a redundant state check.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0x4691, 0xF102);
    if (addr) {
        printf("Found AVB cmdline function at 0x%08X\n", addr);
        NOP(addr + 0x9C, 4);
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

    // (Recovery boot cmdline hook is not implemented for X6532 because
    // the CMDLINE buffer addresses could not be found in this LK build.
    // The cmdline_append function at 0x4C42ED6C has no BL callers,
    // making it impossible to trace cmdline_pre_process.)
}

// NOTE: Warning suppression patches are in board_early_init(), NOT
// board_late_init(). This LK build doesn't have the app() pointer needed
// by kaeru to hook apps_init(), so kaeru_late_init() never runs.
// board_early_init() runs from kaeru_early_init() which is always reached.

void board_early_init(void) {
    printf("Entering early init for Infinix Smart 9 (X6532)\n");

    uint32_t addr = 0;

    // Suppresses the bootloader unlock warning shown during boot on
    // unlocked devices. In addition to the visual warning, it also
    // introduces an unnecessary 5-second delay.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0x4B0E, 0x447B, 0x681B, 0x681B);
    if (addr) {
        printf("Found orange_state_warning at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Disables the dm-verity corruption warning during boot.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB530, 0xB083, 0xAB02, 0x2200);
    if (addr) {
        printf("Found dm_verity_corruption at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Forces get_vfy_policy to return 0 ("signature valid").
    // Verified pattern at 0x4C417F0C: B508 F7FF FF63 F3C0
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF63, 0xF3C0);
    if (addr) {
        printf("Found get_vfy_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Forces get_dl_policy to return 0 ("download allowed").
    // Verified pattern at 0x4C417F18: B508 F7FF FF5D F000
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF5D, 0xF000);
    if (addr) {
        printf("Found get_dl_policy at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Forces ccci_ld_md_sec to return 0 ("modem loading allowed").
    // Verified pattern at 0x4C452AC0: E92D 41F0 460A 4604
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0x460A, 0x4604);
    if (addr) {
        printf("Found ccci_ld_md_sec at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    }

    // Bypass the secure boot state gate in the fastboot command processor.
    // This function (0x4C46B20C) reads the sboot state from a global struct
    // and returns 1 only if the state is 0x11 (ATTR_SBOOT_ENABLE). If the state
    // is anything else, it may block fastboot commands. FORCE_RETURN(1) makes
    // it always report "sboot state OK" regardless of the actual state.
    // This serves as a safety net for when we spoof the lock state later.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB538, 0x4B18, 0x447B, 0x681B);
    if (addr) {
        printf("Found sboot_state_check at 0x%08X\n", addr);
        FORCE_RETURN(addr, 1);
    }

    // Hook into a printf call in platform_init that runs right after
    // environment initialization. At this point get_env() returns real
    // env values, allowing us to conditionally apply seccfg lock state
    // patches based on the bldr_spoof env variable.
    //
    // Pattern at 0x4C403C16 (platform_init + 0x01EA):
    //   F036 F8C1 = bl dprintf
    //   6823      = ldr r3, [r4, #0]
    //   2000      = movs r0, #0
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF036, 0xF8C1, 0x6823, 0x2000);
    if (addr) {
        printf("Found env_init_done at 0x%08X\n", addr);
        PATCH_CALL(addr, (void*)spoof_lock_state, TARGET_THUMB);
    }

    // Forces get_sboot_state to store ATTR_SBOOT_ENABLE (0x11) in the
    // output parameter and return 0. This controls whether secure boot
    // verification is enabled and is checked by 0x4C46B20C (the sboot
    // state gate we bypassed above). Setting it to 0x11 indicates secure
    // boot is properly enabled, which other code paths may check.
    //
    // Verified pattern at 0x4C46B85C: B510 4604 2001 F7FF
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4604, 0x2001, 0xF7FF);
    if (addr) {
        printf("Found get_sboot_state at 0x%08X\n", addr);
        PATCH_MEM(addr,
            0x2311,  // movs r3, #0x11   - ATTR_SBOOT_ENABLE
            0x6003,  // str r3, [r0, #0]  - store to *param_1
            0x2000,  // movs r0, #0       - return 0
            0x4770   // bx lr             - return
        );
    }

    // The fastboot command processor at 0x4C42C1B0 calls a lock state gate
    // at +0x15A (bl 0x4c46eb00). The function returns 1 when unlocked and 0
    // when locked. At +0x15E (cmp r0, #0) and +0x160 (beq.w 0x4c42c518), the
    // branch to locked path is taken when r0==0. We replace the bl with
    // movs r0, #1 so the command dispatch at +0x164 is always reached.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xB0A7, 0xAA1C);
    if (addr) {
        printf("Found fastboot command processor at 0x%08X\n", addr);
        // Bypass lock gate: movs r0, #1; nop  (r0=1 = unlocked path)
        PATCH_MEM(addr + 0x15A, 0x2001, 0xBF00);
    }

    // Register fastboot OEM commands.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);
    fastboot_register("oem env", cmd_env, 1);

    // Publish kaeru version info.
    fastboot_publish("kaeru-version", "kaeru v2.0.0");
}

void board_late_init(void) {
    printf("Entering late init for Infinix Smart 9 (X6532)\n");
}
