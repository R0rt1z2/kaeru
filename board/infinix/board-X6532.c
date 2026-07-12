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

    // Makes seccfg_get_lock_state always report LKS_LOCK (lock_state=1) and
    // return 0 (success). This function is called via a tail-call from
    // 0x4C4264D4, which is called from 0x4C46EB00 at +0x15A in the fastboot
    // command processor.
    //
    // IMPORTANT: We return r0=0, NOT r0=2. The caller (0x4C46EB00) checks
    // 'cbnz r0, #error' — any non-zero return triggers an error log path!
    // Returning 0 avoids that path entirely.
    //
    // PATCH_MEM at addr+6 overwrites the body after the prologue (cbz, push,
    // mov r4, r0):
    //   0x2301 = movs r3, #1      -- lock_state = LKS_LOCK
    //   0x6023 = str r3, [r4, #0]  -- *arg = lock_state
    //   0x2000 = movs r0, #0       -- return 0 (NOT 2!)
    //   0xBD10 = pop {r4, pc}      -- return
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);
    if (addr) {
        printf("Found seccfg_get_lock_state at 0x%08X\n", addr);
        PATCH_MEM(addr + 6,
            0x2301,  // movs r3, #1
            0x6023,  // str r3, [r4, #0]
            0x2000,  // movs r0, #0   (NOT 2!)
            0xBD10   // pop {r4, pc}
        );
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

    // Register fastboot OEM commands.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);
    fastboot_register("oem env", cmd_env, 1);

    // Publish kaeru version info.
    fastboot_publish("kaeru-version", "kaeru v2.0.0");
}

void board_late_init(void) {
    printf("Entering late init for Infinix Smart 9 (X6532)\n");
}
