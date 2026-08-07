//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include "include/mt8163-common.h"

static struct {
    bool unlocked_critical;
    bool bypass_remap;
} gd;

static uint8_t page[BOOTIMG_HDR_SZ] __attribute__((aligned(64)));

static inline void cmd_flash(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_FLASH_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_erase(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_ERASE_FUNC_ADDR|1))(arg, data, sz);
}

static bool advance_partition_name(const char** partition) {
    if (!partition || !*partition) {
        return false;
    }

    while (**partition != '\0' && ISSPACE(**partition)) {
        (*partition)++;
    }

    return (**partition != '\0');
}

static bool is_partition_protected(const char* partition, bool erase) {
    // These partitions are critical, flashing them incorrectly can lead to a
    // hard brick. To prevent accidental damage, we mark them as protected and
    // block write access.
    if (strcmp(partition, "lk") == 0 || strcmp(partition, "preloader") == 0 ||
        strcmp(partition, "tee1") == 0 || strcmp(partition, "tee2") == 0) {
        return !gd.unlocked_critical;
    }

    // Erasing the partition where kaeru would be installed is also a bad idea...
    if (erase && strcmp(partition, CONFIG_BOOTLOADER_PARTITION_NAME) == 0) {
        return !gd.unlocked_critical;
    }

    return false;
}

static const char* remap_partition(const char* partition) {
    if (gd.bypass_remap)
        return NULL;

    if (strcmp(partition, "lk") == 0)
        // Redirect to where LK is actually loaded from.
        return CONFIG_BOOTLOADER_PARTITION_NAME;

    if (strcmp(partition, "misc") == 0)
        // For whatever reason Amazon decided to use uppercase.
        return CONFIG_MISC_PARTITION_NAME;

    return NULL;
}

static void critical_op_fail(const char *msg) {
    fastboot_info("");
    fastboot_info(msg);
    fastboot_info("This may BRICK your device with NO WAY TO RECOVER!");
    fastboot_info("You may allow this if you know what you're doing with:");
    fastboot_info("'fastboot flashing unlock_critical'");
    fastboot_info("You will be on your own from then on.");
    fastboot_fail("Partition is protected");
}

static void cmd_flash_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = arg;
    advance_partition_name(&part);

    const char *remap = remap_partition(part);
    if (remap) {
        arg = remap;
    } else if (is_partition_protected(part, false)) {
        critical_op_fail("You are attempting to flash to a critical partition.");
        return;
    }

    cmd_flash(arg, data, sz);
}

static void cmd_erase_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = arg;
    advance_partition_name(&part);

    if (is_partition_protected(part, true)) {
        critical_op_fail("You are attempting to erase a critical partition.");
        return;
    }

    const char *remap = remap_partition(part);
    if (remap)
        arg = remap;

    cmd_erase(arg, data, sz);
}

static void cmd_unlock_critical(const char *arg, void *data, unsigned sz) {
    gd.unlocked_critical = true;
    fastboot_okay("");
}

static void cmd_lock_critical(const char *arg, void *data, unsigned sz) {
    gd.unlocked_critical = false;
    fastboot_okay("");
}

static void cmd_oem_bypass_remap(const char *arg, void *data, unsigned sz) {
    // Allow the user to disable partition mapping if required
    // (because it most certainly will be, eventually).
    const char *mode = arg + 1;

    if (!arg)
        goto usage;

    if (*mode == '1')
        gd.bypass_remap = false;
    else if (*mode == '0')
        gd.bypass_remap = true;
    else
        goto usage;

    fastboot_okay("");
    return;

usage:
    fastboot_fail("Usage: fastboot oem part-remap [0|1]");
}

static void cmd_remove_microloader(const char *arg, void *data, unsigned sz) {
    static const char *names[] = { "boot", "recovery" };
    char msg[64];
    int fixed = 0;

    for (uint32_t i = 0; i < ARRAY_SIZE(names); i++) {
        const struct part_info *part = storage_part_find(names[i]);
        if (!part)
            continue;

        if (storage_part_read(part, page, 0, sizeof(page)) != sizeof(page)) {
            fastboot_fail("Failed to read partition");
            return;
        }

        if (memcmp(page, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SZ) == 0 ||
            memcmp(page + 0x400, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SZ) != 0)
            continue;

        memcpy(page, page + 0x400, 0x400);
        memset(page + 0x400, 0, 0x400);

        if (storage_part_write(part, page, 0, sizeof(page)) != sizeof(page)) {
            fastboot_fail("Failed to write partition");
            return;
        }

        npf_snprintf(msg, sizeof(msg), "Removed the microloader from '%s'",
                     names[i]);
        fastboot_info(msg);
        fixed++;
    }

    if (!fixed)
        fastboot_info("No microloader found to remove");

    fastboot_okay("");
}

static void blank_string(char *s, const char *name) {
    if (!s) {
        printf("Could not find the %s string\n", name);
        return;
    }

    printf("Found the %s string at 0x%08X\n", name, (uint32_t)(uintptr_t)s);
    WRITE8(s, '\0');
}

static void neuter_cmdline_format(char *fmt, const char *name) {
    if (!fmt) {
        printf("Could not find the %s cmdline format\n", name);
        return;
    }

    printf("Found the %s cmdline format at 0x%08X\n", name,
           (uint32_t)(uintptr_t)fmt);
    WRITE8(fmt + 2, '\0');
}

static void bootimg_cmdline_hook(const char *fmt, const char *tag,
                                 const char *cmdline) {
    printf(fmt, tag, cmdline);

    if (cmdline && strstr(cmdline, "bootopt=64")) {
        printf("Boot image requests a 64-bit kernel, forcing it\n");
        WRITE32(KERNEL_64BIT_FLAG_ADDR, 1);
    }
}

static void cmd_kaeru_version(const char *arg, void *data, unsigned sz) {
    video_set_cursor(video_get_rows() / 12, 0);
    cmd_version(arg, data, sz);
}

static void fastboot_init_hook(const char *) {
    video_printf(" => HACKED FASTBOOT mode - xyz, k4y0z, R0rt1z2, bengris32\n");

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);
    fastboot_register("oem part-remap", cmd_oem_bypass_remap, 1);
    fastboot_register("oem remove-microloader", cmd_remove_microloader, 1);

    // Registered last so it shadows the one common_early_init() put in.
    fastboot_register("oem kaeru-version", cmd_kaeru_version, 1);
}

void board_early_init(void) {
    printf("Entering early init for %s\n", BOARD_NAME);

    uint32_t addr = 0;

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Amazon decides whether the device is unlocked by validating the unlock
    // code stored in IDME. Every gate in LK, from boot to the fastboot command
    // handlers, goes through this one predicate, so forcing it to report an
    // unlocked device is enough to get unrestricted fastboot access.
    addr = SEARCH_PATTERN(LK_START, LK_END, UNLOCK_CHECK_PATTERN);
    if (addr) {
        printf("Found unlock check at 0x%08X\n", addr);
        FORCE_RETURN(addr, 1);
    }

    // Do not let LK force a SELinux state on the cmdline. It does this from
    // two places, the dkernel path inline and selinux_cmdline() proper, but
    // both go through these formats.
    neuter_cmdline_format(SEARCH_STRING("%s androidboot.selinux=permissive"),
                          "selinux permissive");
    neuter_cmdline_format(SEARCH_STRING("%s androidboot.selinux=enforce"),
                          "selinux enforce");
    neuter_cmdline_format(SEARCH_STRING("%s androidboot.selinux=%s"), "selinux");

    // LK only puts androidboot.veritymode=disabled on the cmdline for eng
    // devices carrying the right fos_flags, and hands everyone else eio
    // mode. Skip both checks so we always land on the disabled branch and
    // get it from LK's own string.
    PATCH_BRANCH(VERITY_CMDLINE_CHECK_ADDR, (void*)VERITY_CMDLINE_DISABLED_ADDR);

    // Disable the built in flash and erase commands. Ours are registered
    // from the hook below, which runs before LK gets to register its own,
    // and the first match in the command list is the one that serves.
    NOP(FB_REGISTER_FLASH_ADDR, 2);
    NOP(FB_REGISTER_ERASE_ADDR, 2);

    // Redirect the call that prints "fastboot_init()\n" to our hook, so
    // we can register custom fastboot commands and other things.
    PATCH_CALL(FASTBOOT_INIT_PRINTF_CALL_ADDR, &fastboot_init_hook, TARGET_THUMB);

    // Stock LK always boots a 32-bit kernel. Hook the spot where it reads the
    // boot image command line so we can honor a 'bootopt=64...' request and
    // force the 64-bit kernel flag before the kernel is prepared.
    PATCH_CALL(BOOTIMG_CMDLINE_PRINT_CALL_ADDR, &bootimg_cmdline_hook,
               TARGET_THUMB);

    // Get rid of the stock mode strings LK draws during boot. They are
    // confusing, tell the user nothing useful, and the fastboot one lands
    // right before our banner. Both call sites that print the fastboot one
    // share the string, so blanking it covers both.
    blank_string(SEARCH_STRING(" => FASTBOOT mode...\n"), "fastboot mode");
    blank_string(SEARCH_STRING(" => FACTORYRESET mode...\n"), "factory reset mode");
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

    // Act on any boot command left in misc and consume it. The stock
    // bootloader reads misc too, but never clears the command, so the
    // device would keep coming back here.
    read_and_set_bootmode_from_message();

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && mode != BOOTMODE_FASTBOOT
        && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal
        // boot. Fastboot is left out since our own banner covers it.
        show_bootmode(mode);
    }
}
