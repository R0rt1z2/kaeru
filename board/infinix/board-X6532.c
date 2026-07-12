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

    // Register bootloader lock spoofing command.
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);

    // Publish kaeru version info.
    fastboot_publish("kaeru-version", "kaeru v2.0.0");
}

void board_late_init(void) {
    printf("Entering late init for Infinix Smart 9 (X6532)\n");
}
