//
// Author: kelexine <https://github.com/kelexine>
// Date: 2026-10-19
// Purpose: Board support and LK runtime hooks for CUBOT Marlon (P50 EEA / MT6765)
// SPDX-FileCopyrightText: 2026 kelexine <https://github.com/kelexine>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>
#include <lib/spoof.h>

// Set the LK boot mode. Arg: normal|meta|recovery|factory|fastboot.
static void cmd_bootmode_set(const char* arg, void* data, unsigned sz) {
    (void)data;
    (void)sz;

    while (*arg == ' ') arg++;

    bootmode_t mode;
    if (strncmp(arg, "normal", 6) == 0) {
        mode = BOOTMODE_NORMAL;
    } else if (strncmp(arg, "meta", 4) == 0) {
        mode = BOOTMODE_META;
    } else if (strncmp(arg, "recovery", 8) == 0) {
        mode = BOOTMODE_RECOVERY;
    } else if (strncmp(arg, "factory", 7) == 0) {
        mode = BOOTMODE_FACTORY;
    } else if (strncmp(arg, "fastboot", 8) == 0) {
        mode = BOOTMODE_FASTBOOT;
    } else {
        fastboot_fail("unknown mode; use: normal|meta|recovery|factory|fastboot");
        return;
    }

    set_bootmode(mode);
    show_bootmode(get_bootmode());

    fastboot_info("boot mode overridden");
    fastboot_okay("");
}

// Print device, SoC, and boot mode information.
static void cmd_marlon_info(const char* arg, void* data, unsigned sz) {
    (void)arg;
    (void)data;
    (void)sz;

    print_kaeru_info(printf);

    fastboot_info("device : CUBOT Marlon (P50 EEA / v956)");
    fastboot_info("soc    : MediaTek MT6765 (Helio P22/P35)");
    fastboot_info(bootmode2str(get_bootmode()));

    fastboot_okay("");
}

FASTBOOT_CMD(bldr_spoof, "oem bldr_spoof", cmd_spoof_bootloader_lock, 1);
FASTBOOT_CMD(kaeru_info, "oem kaeru-info", cmd_marlon_info, 1);
FASTBOOT_CMD(kaeru_bootmode, "oem kaeru-bootmode", cmd_bootmode_set, 1);

// LK fetches the kernel cmdline buffer through a tiny getter immediately
// before committing it into the FDT /chosen "bootargs" property. The getter
// is resolved at runtime from the BL inside the commit-site pattern below.
static char* (*cmdline_buf_getter)(void);

static char* recovery_cmdline_hook(void) {
    handle_recovery_boot();
    return cmdline_buf_getter();
}

// Apply lock-state spoofing patches.
static void spoof_lock_state(void) {
    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");

    if (!spoofing) {
        printf("kaeru: bootloader lock spoofing disabled\n");
        return;
    }

    printf("kaeru: bootloader lock spoofing enabled, applying patches\n");

    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);
    if (addr) {
        printf("kaeru: patching seccfg_get_lock_state @ 0x%08X\n", addr);
        PATCH_MEM(addr + 6,
                  0x2304,  // movs r3, #4 (LKS_LOCK)
                  0x6023,  // str r3, [r4]
                  0x2000,  // movs r0, #0
                  0xBD10); // pop {r4, pc}
    } else {
        printf("kaeru: seccfg_get_lock_state pattern not found\n");
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF003, 0xFD30, 0xF003, 0xFD28, 0x4605);
    if (addr) {
        uint32_t getter = DECODE_BL_TARGET(addr + 4);
        printf("kaeru: hooking cmdline commit @ 0x%08X (getter @ 0x%08X)\n",
               addr + 4, getter);
        cmdline_buf_getter = (char* (*)(void))(getter | 1);
        PATCH_CALL(addr + 4, (void*)recovery_cmdline_hook, TARGET_THUMB);
    } else {
        printf("kaeru: cmdline commit hook point not found\n");
    }
}

// Pattern-patch LK in RAM: bypass unlock/verify/download policy checks,
// silence bootloader warnings, and unrestrict the fastboot command processor.
void board_early_init(void) {
    printf("kaeru: early init - CUBOT Marlon (MT6765)\n");

    uint32_t addr = 0;

    // fastboot_unlock_verify: return 0 to bypass OEM unlock verification.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xF5AD, 0x5DAC);
    if (addr) {
        printf("kaeru: patching fastboot_unlock_verify @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: fastboot_unlock_verify pattern not found\n");
    }

    // get_vfy_policy: return 0 to accept unsigned boot/recovery images.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF75, 0xF3C0);
    if (!addr) {
        addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4C00, 0x447C, 0x6824);
    }
    if (addr) {
        printf("kaeru: patching get_vfy_policy @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: get_vfy_policy pattern not found\n");
    }

    // get_dl_policy: return 0 to allow flashing regardless of lock state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF6F, 0xF000);
    if (addr) {
        printf("kaeru: patching get_dl_policy @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: get_dl_policy pattern not found (may be inlined)\n");
    }

    // orange_state_warning: skip the unlocked bootloader warning screen.
    addr = SEARCH_PATTERN(LK_START, LK_END,
                           0xB508, 0x4B0E, 0x447B, 0x681B, 0x681B, 0x2B02);
    if (addr) {
        printf("kaeru: patching orange_state_warning @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: orange_state_warning pattern not found\n");
    }

    // dm_verity_corruption_warning: skip the dm-verity warning screen.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2802, 0xD000, 0x4770, 0xB538, 0x2432);
    if (addr) {
        printf("kaeru: patching dm_verity_corruption_warning @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: dm_verity_corruption_warning pattern not found\n");
    }

    // red_state_warning: skip the red-state warning and halt loop.
    addr = SEARCH_PATTERN(LK_START, LK_END,
                           0xB508, 0x4B0C, 0x447B, 0x681B, 0x681B, 0x2B03);
    if (!addr) {
        addr = SEARCH_PATTERN(LK_START, LK_END, 0x2803, 0xD001, 0x4770);
    }
    if (addr) {
        printf("kaeru: patching red_state_warning @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: red_state_warning pattern not found\n");
    }

    // Fastboot command processor: NOP security gates and jump to command dispatch.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4880, 0xB087, 0x4D5A);
    if (addr) {
        printf("kaeru: patching fastboot command processor @ 0x%08X\n", addr);
        NOP(addr + 0x15A, 2);
        NOP(addr + 0x166, 2);
        PATCH_MEM(addr + 0xF0, 0xE006);
    } else {
        printf("kaeru: fastboot command processor pattern not found\n");
    }

    // Env init done: printf call in platform_init right after env_init().
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4946, 0x3046, 0x4FF0, 0x000B, 0x7A44);
    if (addr) {
        printf("kaeru: hooking env init done @ 0x%08X\n", addr);
        PATCH_CALL(addr + 0xA, (void*)spoof_lock_state, TARGET_THUMB);
    } else {
        printf("kaeru: env init hook point not found\n");
    }

    // Patch USB QMU driver to disable ZLP on bulk IN endpoints.
    // MTK QMU sends a 0-length packet on transfers that are multiples of 512 bytes,
    // which causes the host Linux usbdevfs / fastboot client to read 0 bytes and
    // fail with "Device sent unknown status code: ".
    // At 0x48044C66: orr r3, r3, #0x80 (sets ZLP bit). We NOP it.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB365, 0xF043, 0x0380, 0x4D20, 0x4631, 0x7023);
    if (addr) {
        printf("kaeru: patching QMU ZLP bit @ 0x%08X\n", addr + 2);
        NOP(addr + 2, 2);
    } else {
        printf("kaeru: QMU ZLP pattern not found\n");
    }

    printf("kaeru: early init complete\n");
}

void board_late_init(void) {
    printf("kaeru: late init - CUBOT Marlon (MT6762/MT6765)\n");

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && !is_unknown_mode(mode)) {
        show_bootmode(mode);
    }

    printf("kaeru: late init complete\n");
}
