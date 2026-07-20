// SPDX-FileCopyrightText: 2026 ryanistr
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <board_ops.h>
#include <lib/spoof.h>
#include <lib/environment.h>
#include <lib/common.h>
#include <lib/security/seccfg.h>

#define KAERU_ENV_BLDR_SPOOF "kaeru_bootloader_spoof_status"
volatile uint32_t g_sec_lr;
volatile uint32_t g_sec_lr_prev;

// TODO: Add volume down -> bootloader key combo

char *get_env(char *name) {
    (void)name;
    return NULL;
}

int set_env(char *name, char *value) {
    (void)name;
    (void)value;
    return -1;
}

static void u32_to_hex(uint32_t v, char *buf);
int is_spoofing_enabled(void) {
    const char *val = get_env(KAERU_ENV_BLDR_SPOOF);
    if (!val) return 1;
    return strcmp(val, "1") == 0;
}

void cmd_spoof_bootloader_lock(const char *arg, void *data, unsigned sz) {
    int status = is_spoofing_enabled();
    const char *option = arg + 1;
    int target = -1;

    if (!strcmp(option, "on"))       target = 1;
    else if (!strcmp(option, "off")) target = 0;

    if (target != -1) {
        if (status != target) {
            set_env(KAERU_ENV_BLDR_SPOOF, target ? "1" : "0");
            fastboot_info(target ?
                "Bootloader spoofing enabled." :
                "Bootloader spoofing disabled.");
            fastboot_info("A factory reset may be required.");
        } else {
            fastboot_info(target ?
                "Bootloader spoofing is already enabled." :
                "Bootloader spoofing is already disabled.");
        }
        fastboot_publish("is-spoofing", target ? "1" : "0");
        fastboot_okay("");
        return;
    }

    if (!strcmp(option, "status")) {
        fastboot_info(status ?
            "Bootloader spoofing is currently enabled." :
            "Bootloader spoofing is currently disabled.");
        char buf[48];
        u32_to_hex(g_sec_lr_prev, buf);
        fastboot_info(buf);
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

static void u32_to_hex(uint32_t v, char *buf) {
    buf[0] = '0';
    buf[1] = 'x';
    int shift;
    for (shift = 28; shift >= 0; shift -= 4) {
        uint32_t nibble = (v >> shift) & 0xF;
        buf[2 + (28 - shift) / 4] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    buf[10] = 0;
}

static void cmd_flash_toggle(const char *arg, void *data, unsigned sz) {
    (void)data;
    (void)sz;
    const char *option = arg + 1;

    if (!strcmp(option, "unlock")) {
        // seccfg_unlock
        WRITE16(0x48272400, 0x0321);
        fastboot_okay("Flash unlocked (seccfg returns UNLOCKED)");
    } else if (!strcmp(option, "lock")) {
        // seccfg_lock
        WRITE16(0x48272400, 0x0221);
        fastboot_okay("Flash locked (seccfg returns LOCKED)");
    } else if (!strcmp(option, "status")) {
        uint16_t insn = *(volatile uint16_t *)0x48272400;
        fastboot_info(insn == 0x0321 ?
            "seccfg: UNLOCKED (3)" :
            "seccfg: LOCKED (2)");
        fastboot_okay("");
    } else {
        fastboot_info("Usage: fastboot oem flash <unlock|lock|status>");
        fastboot_info("  unlock  - Temporarily allow flashing (until reboot)");
        fastboot_info("  lock    - Re-lock flash access");
        fastboot_info("  status  - Show current seccfg state");
        fastboot_fail("Unknown option");
    }
}

void board_early_init(void) {
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);
    fastboot_register("oem flash", cmd_flash_toggle, 1);

    // nop_command_gate
    NOP(0x4822E94C, 1);
    // seccfg_v2
    FORCE_RETURN(0x48228C5C, 3);
    // force_avb_allow_error
    PATCH_MEM(0x48266B06, 0xF04F, 0x0901);
    // bypass_modem_verify
    FORCE_RETURN(0x48254850, 0);
    // bypass_vbmeta_verification
    PATCH_MEM(0x482632D4, 0x451B);
    NOP(0x48263600, 2);
    PATCH_MEM(0x48263672, 0x2301);
    // force_green_state
    PATCH_MEM(0x48250A04, 0x2300);
    // force_lock_state (movs r1,#2; str r1,[r4]; bx lr)
    PATCH_MEM(0x48272400, 0x2102);
    PATCH_MEM(0x48272402, 0x6001);
    PATCH_MEM(0x48272404, 0x4770);
    // device_unlocked_no
    FORCE_RETURN(0x48273CE8, 0);
    // device_locked_yes
    FORCE_RETURN(0x48273CB0, 1);
    // bypass_avb_verification (get_vfy_policy)
    FORCE_RETURN(0x4821B344, 1);
    // boot_state_strings_green (replaces "orange", "yellow", "red" with "green\0")
    PATCH_MEM(0x482C42D8, 0x7267, 0x6565, 0x006E);
    PATCH_MEM(0x482C42EC, 0x7267, 0x6565, 0x006E);
    PATCH_MEM(0x482C4300, 0x7267, 0x6565, 0x006E);
    // verifiedbootstate_strings_green (replaces verifiedbootstate strings with "green\0")
    PATCH_MEM(0x482C457E, 0x7267, 0x6565, 0x006E);
    PATCH_MEM(0x482C45A6, 0x7267, 0x6565, 0x006E);
    PATCH_MEM(0x482C45CE, 0x7267, 0x6565, 0x006E);
    // append_flash_locked
    const char* suffix = " androidboot.flash.locked=1";
    volatile uint8_t* suffix_ptr = (volatile uint8_t*)(0x482C45F7);
    for (int i = 0; i <= 27; i++) {
        suffix_ptr[i] = suffix[i];
    }
    arch_sync_cache_range(0x482C45F7, 28);
    // allow_download_policy (get_dl_policy)
    FORCE_RETURN(0x4821B3C8, 1);
    // avb_cmdline_nop_lock_state
    NOP(0x48260A04, 2);
}

void __attribute__((used)) board_late_init(void) {
}
