//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

static void* malloc(size_t size) {
    return ((void* (*)(size_t))(CONFIG_MALLOC_ADDRESS | 1))(size);
}

// Motorola's OEM command dispatcher passes (arg_count, arg_array) rather
// than the standard (arg, data, sz) used by vanilla MediaTek bootloaders.
// This means OEM commands must be defined separately from common ones.
static int cmd_kaeru_version(int arg_count, const char** args) {
    (void)arg_count;
    (void)args;

    char buf[64];
    npf_snprintf(buf, sizeof(buf), "kaeru v%s", KAERU_VERSION);
    fastboot_info(buf);
    print_kaeru_info(OUTPUT_VIDEO);
    return 0;
}

static int cmd_device_mode(int arg_count, const char** args) {
    const char* subcmd = (arg_count > 1) ? args[1] : NULL;

    if (!subcmd) {
        char* mode = get_env("kaeru.device_mode");
        char buf[64];
        npf_snprintf(buf, sizeof(buf), "device_mode=%s", mode ? mode : "unlocked");
        fastboot_info(buf);
        fastboot_info("");
        fastboot_info("Usage: fastboot oem device-mode <engineering|unlocked>");
        return 0;
    }

    if (strcmp(subcmd, "engineering") == 0) {
        set_env("kaeru.device_mode", "engineering");
        fastboot_info("Device mode set to engineering.");
        fastboot_info("Reboot for changes to take effect.");
        return 0;
    }

    if (strcmp(subcmd, "unlocked") == 0) {
        set_env("kaeru.device_mode", "unlocked");
        fastboot_info("Device mode set to unlocked.");
        fastboot_info("Reboot for changes to take effect.");
        return 0;
    }

    fastboot_info("Unknown mode. Valid options: engineering, unlocked");
    return 3;
}

// Yes, we malloc a whole new table and copy everything just to add one
// command. Hooking the jump table directly crashes LK, so here we are.
static void fastboot_inject(const char* name, void* handler) {
    volatile uint32_t* got_start = (volatile uint32_t*)0x48143524;
    volatile uint32_t* got_end = (volatile uint32_t*)0x48143194;

    uint32_t old_start = *got_start;
    uint32_t old_end = *got_end;
    int old_size = old_end - old_start;
    int new_size = old_size + 20;

    uint32_t* new_table = (uint32_t*)malloc(new_size);
    if (!new_table)
        return;

    memcpy(new_table, (void*)old_start, old_size);

    uint32_t* entry = (uint32_t*)((uint8_t*)new_table + old_size);
    entry[0] = 0;
    entry[1] = 1;
    entry[2] = 0;
    entry[3] = (uint32_t)name;
    entry[4] = (uint32_t)handler | 1;

    *got_start = (uint32_t)new_table;
    *got_end = (uint32_t)new_table + new_size;

    arch_clean_invalidate_cache_range((uint32_t)got_start, sizeof(uint32_t));
    arch_clean_invalidate_cache_range((uint32_t)got_end, sizeof(uint32_t));
    arch_clean_invalidate_cache_range((uint32_t)new_table, new_size);
}

void sp_init_hook(void) {
    char* mode = get_env("kaeru.device_mode");

    if (mode && strcmp(mode, "engineering") == 0) {
        *(volatile uint32_t*)0x481E1E84 = 0x7070;
    } else {
        *(volatile uint32_t*)0x481E1E84 = 0x77EE;
        *(volatile uint32_t*)0x481E1E64 = 0x66CC;
        FORCE_RETURN(0x4807FD14, 1); // gvb_unlocked check
    }
}

void board_early_init(void) {
    printf("Entering early init for Motorola Moto G Pure 2021\n");

    // Motorola sets the device lock state from the SP partition during
    // boot. Hook after environment initialization so we can read the
    // persisted device mode and override the SP-derived globals.
    PATCH_CALL(0x48002DB0, (void*)sp_init_hook, TARGET_THUMB);

    // Prevent the phone from refusing to boot if the SP data has been
    // tampered with.
    FORCE_RETURN(0x4807ED40, 1);

    // Disable image verification so we can boot or flash any partition,
    // not just boot and recovery. Without this, custom logos, modem
    // images, and other signed partitions would be rejected.
    FORCE_RETURN(0x48018518, 0);
}

void board_late_init(void) {
    printf("Entering late init for Motorola Moto G Pure 2021\n");

    fastboot_inject("kaeru-version", cmd_kaeru_version);
    fastboot_inject("device-mode", cmd_device_mode);
}