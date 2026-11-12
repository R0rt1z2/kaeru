//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include "include/mt8169-common.h"

#define BOOT_ARG_MAGIC    0x504C504C
#define BOOT_ARG_MODE_OFF 0x4

#define LOG_STORE_OFF_PL  0x8
#define LOG_STORE_SIZE_PL 0xC
#define LOG_STORE_OFF_LK  0x14
#define LOG_STORE_SIZE_LK 0x18

enum mode_reason {
    MODE_REASON_NONE = 0,
    MODE_REASON_MISC = 1,
    MODE_REASON_FACTORY = 2,
    MODE_REASON_KEY = 4,
};

static struct {
    enum mode_reason reason;
    bool unlocked_critical;
    int banner_row;
} gd;

static const char *modereason2str(enum mode_reason reason) {
    switch (reason) {
        case MODE_REASON_NONE:
            return "None";
        case MODE_REASON_MISC:
            return "BCB";
        case MODE_REASON_FACTORY:
            return "Factory";
        case MODE_REASON_KEY:
            return "Key";
        default:
            return NULL;
    }
}

static char *cmdline_hook(void) {
    char *cl = get_cmdline();
    if (!cl) {
        return cl;
    }

    for (char *p = strstr(cl, "androidboot.veritymode="); p;
         p = strstr(p, "androidboot.veritymode=")) {
        while (*p && *p != ' ') {
            *p++ = ' ';
        }
    }

    cmdline_append("androidboot.veritymode=disabled");

    char *r = cl;
    char *w = cl;
    while (*r) {
        if (*r == ' ' && (w == cl || w[-1] == ' ')) {
            r++;
            continue;
        }
        *w++ = *r++;
    }
    while (w > cl && w[-1] == ' ') {
        w--;
    }
    *w = '\0';
    WRITE32(CMDLINE_END_PTR_ADDR, (uint32_t)(uintptr_t)w);

    return cl;
}

static bool is_partition_protected(const char* partition) {
    // These partitions are critical, flashing them incorrectly can lead to a
    // hard brick. To prevent accidental damage, we mark them as protected and
    // block write access.
    if (strcasecmp(partition, "boot0") == 0 ||
        strcasecmp(partition, "preloader") == 0 ||
        strcasecmp(partition, "singlebootloader") == 0 ||
        strcasecmp(partition, "bootloader") == 0) {
        return !gd.unlocked_critical;
    }

    return false;
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
    const char *part = skip_spaces(arg);

    if (is_partition_protected(part)) {
        critical_op_fail("You are attempting to flash to a critical partition.");
        return;
    }

    cmd_flash(arg, data, sz);
}

static void cmd_erase_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = skip_spaces(arg);

    if (is_partition_protected(part)) {
        critical_op_fail("You are attempting to erase a critical partition.");
        return;
    }

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

static void dump_log(uint32_t base, uint32_t max) {
    char line[FASTBOOT_INFO_MAX + 1];
    const char *p = (const char *)(uintptr_t)base;
    uint32_t col = 0;

    for (uint32_t i = 0; i < max && p[i]; i++) {
        char c = p[i];

        if (c == '\n' || col == FASTBOOT_INFO_MAX) {
            line[col] = '\0';
            fastboot_info(line);
            col = 0;

            if (c == '\n') {
                continue;
            }
        }

        line[col++] = (c >= 0x20 && c < 0x7F) ? c : (c == '\r' ? ' ' : '.');
    }

    if (col) {
        line[col] = '\0';
        fastboot_info(line);
    }
}

static void cmd_oem_dmesg(const char *arg, void *data, unsigned sz) {
    uint32_t hdr = READ32(LOG_STORE_HEADER_PTR_ADDR);
    uint32_t base = READ32(LOG_STORE_DRAM_BASE_ADDR);

    if (!hdr || !base) {
        fastboot_fail("Log buffers are not set up");
        return;
    }

    const char *what = skip_spaces(arg);

    bool lk = strcmp(what, "pl") != 0;
    bool pl = strcmp(what, "pl") == 0 || strcmp(what, "all") == 0;

    if (pl) {
        dump_log(base + READ32(hdr + LOG_STORE_OFF_PL), READ32(hdr + LOG_STORE_SIZE_PL));
    }

    if (lk) {
        dump_log(base + READ32(hdr + LOG_STORE_OFF_LK), READ32(hdr + LOG_STORE_SIZE_LK));
    }

    fastboot_okay("");
}

FASTBOOT_CMD(dmesg, "oem dmesg", cmd_oem_dmesg, 1);

static void cmd_oem_shutdown(const char *arg, void *data, unsigned sz) {
    fastboot_info("The device is about to power off.");
    fastboot_info("Unplug it once the screen goes black, or it will reboot.");
    fastboot_okay("");

    // Let the USB stack push the reply out before the PMIC drops the
    // rails from under it, otherwise the host never sees the OKAY...
    mdelay(100);

    mt_power_off();

    // We only get here if the PMIC refused to go down.
    fastboot_fail("Power off failed");
}

FASTBOOT_CMD(shutdown, "oem shutdown", cmd_oem_shutdown, 1);
FASTBOOT_CMD(poweroff, "oem poweroff", cmd_oem_shutdown, 1);

static void blank_string(char *s, const char *name) {
    if (!s) {
        printf("Could not find the %s string\n", name);
        return;
    }

    printf("Found the %s string at 0x%08X\n", name, (uint32_t)(uintptr_t)s);
    WRITE8(s, '\0');
}

static void cmd_kaeru_version(const char *arg, void *data, unsigned sz) {
    int rows = video_get_rows();
    int row = rows > 24 ? (rows - 24) / 2 : 0;

    video_set_cursor(row, 0);
    cmd_version(arg, data, sz);
}

static void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz) {
    cmd_reboot_write_message(arg);

    if (!strcmp(arg, "-bootloader"))
        cmd_reboot_bootloader("", data, sz);
    else
        cmd_reboot("", data, sz);
}

static void fastboot_init_hook(const char *) {
    // Save where LK left the cursor before we touch it.
    gd.banner_row = (int)READ32(VIDEO_CURSOR_ROW_ADDR);

    // The console is drawn rotated here, so no column pulls this flush
    // left. Leave it where LK had it.
    video_set_cursor(gd.banner_row, 0);
    video_printf(" => HACKED FASTBOOT mode - R0rt1z2\n");
    fastboot_publish("boot-reason", modereason2str(gd.reason));

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);

    // Registered last so it shadows the one common_early_init() put in.
    fastboot_register("oem kaeru-version", cmd_kaeru_version, 1);

#ifdef HAVE_FASTBOOT_INIT
    device_fastboot_init();
#endif
}

void board_early_init(void) {
    printf("Entering early init for %s\n", BOARD_NAME);

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Amazon decides whether the device is unlocked by validating the unlock
    // code stored in IDME. Every gate in LK, from boot to the fastboot command
    // handlers, goes through this one predicate, so forcing it to report an
    // unlocked device is enough to get unrestricted fastboot access.
    FORCE_RETURN(UNLOCK_CHECK_FUNC_ADDR, 1);

    // LK only lets an unlocked device off the AVB hook when IDME carries
    // the right fos_flags, and hands everyone else a red state and a reboot
    // five seconds later. Skip the check so we always land on orange, which
    // is what makes libavb tolerate verification errors.
    FORCE_RETURN(VERITY_CHECK_FUNC_ADDR, 1);

    // Get rid of the stock mode string LK draws during boot. It is
    // confusing, tells the user nothing useful, and lands right before
    // our banner.
    blank_string(SEARCH_STRING(" => FASTBOOT mode...\n"), "fastboot mode");

    // LK draws flash and download progress all over the framebuffer. The
    // first two are the format strings, so they take the timer and the
    // percentage with them. The rest are just labels.
    blank_string(SEARCH_STRING("\n%s  Time:%d s Vel:%d MB/s \n"), "flash summary");
    blank_string(SEARCH_STRING("%s > %3d%% Time:%4d s Vel:%3d MB/s             "),
                 "flash progress");
    blank_string(SEARCH_STRING("\nWriting Flash ... "), "writing flash");
    blank_string(SEARCH_STRING("\rWrite Data"), "write data");
    blank_string(SEARCH_STRING("USB Transferring... "), "usb transferring");
    blank_string(SEARCH_STRING("USB Bulk Transferring... "), "usb bulk transferring");
    blank_string(SEARCH_STRING("\n\nOK"), "flash ok");

    // Both of those paths wipe the framebuffer before they print. Blanking
    // the strings only bought a clear screen, so drop the clear as well.
    const uint32_t clean_screen_calls[] = { FB_CLEAN_SCREEN_CALL_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(clean_screen_calls); i++)
        NOP(clean_screen_calls[i], 2);

    // A crash leaves the watchdog to reset the board, and the boot after
    // that runs LK's crash dumper, which spends the better part of a second
    // pushing everything into expdb and then resets us a second time...
    FORCE_RETURN(KEDUMP_FUNC_ADDR, 0);

    // LK appends androidboot.selinux=enforcing and enforcing=1 unless IDME
    // dev_flags asks otherwise. Drop the call and leave the cmdline alone.
    NOP(SELINUX_CMDLINE_CALL_ADDR, 2);

    // Disable dm-verity, we won't be needing it anymore :)
    PATCH_CALL(CMDLINE_GET_CALL_ADDR, &cmdline_hook, TARGET_THUMB);

    // Redirect the call that prints "fastboot_init()\n" to our hook, so
    // we can register custom fastboot commands and other things.
    PATCH_CALL(FASTBOOT_INIT_PRINTF_CALL_ADDR, &fastboot_init_hook, TARGET_THUMB);

    // Disable the built in flash and erase commands. Ours are registered
    // from the hook below, which runs before LK gets to register its own,
    // and the first match in the command list is the one that serves.
    NOP(FB_REGISTER_FLASH_ADDR, 2);
    NOP(FB_REGISTER_ERASE_ADDR, 2);

    // Same mechanic for reboot, reboot-bootloader, reboot-recovery and
    // reboot-fastboot, so our own reboot handler is the one that serves
    // the whole prefix and every target goes through the BCB.
    NOP(FB_REGISTER_REBOOT_ADDR, 2);
    NOP(FB_REGISTER_REBOOT_BOOTLOADER_ADDR, 2);
    NOP(FB_REGISTER_REBOOT_RECOVERY_ADDR, 2);
    NOP(FB_REGISTER_REBOOT_FASTBOOT_ADDR, 2);

    // Set up our getvar handler to make fastboot fetch work.
    NOP(FB_REGISTER_GETVAR_ADDR, 2);

    // Get rid of the stock fastboot commands used to read logs. We have
    // our own that are more useful anyway.
    const uint32_t log_cmds[] = { FB_REGISTER_LOG_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(log_cmds); i++)
        NOP(log_cmds[i], 2);

    // LK registers its own 'oem idme' through a small wrapper that tail
    // branches into fastboot_register. Kill the call to that wrapper so
    // our idmelib backed handler is the one that serves.
    NOP(LK_IDME_REGISTER_CALL_ADDR, 2);

#ifdef HAVE_EARLY_INIT
    device_early_init();
#endif
}

static bootmode_t get_preloader_bootmode(void) {
    // The preloader hands over either the old boot_arg block or a list of
    // boot tags. LK copies both into this block early on, so read it from
    // here rather than from the pointer the preloader left in r4.
    if (READ32(BOOT_ARG_ADDR) != BOOT_ARG_MAGIC) {
        printf("Preloader boot args are not set up, asking LK instead\n");
        return get_bootmode();
    }

    return (bootmode_t)READ32(BOOT_ARG_ADDR + BOOT_ARG_MODE_OFF);
}

static void boot_mode_select(void) {
    // A held key wins over everything else, so there is always a way into
    // recovery or fastboot whatever the preloader or the BCB asked for.
    if (mtk_detect_key(KEY_VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
        gd.reason = MODE_REASON_KEY;
        return;
    }

    // LK uses volume down as its own recovery key and has already picked
    // recovery by the time we get here. Overriding that is what sends us to
    // fastboot instead.
    if (mtk_detect_key(KEY_VOLUME_DOWN)) {
        set_bootmode(BOOTMODE_FASTBOOT);
        gd.reason = MODE_REASON_KEY;
        return;
    }

    // Act on the preloader's boot mode before anything else, forcing
    // fastboot on a factory boot and recovery on an ATE factory boot.
    bootmode_t bootmode = get_preloader_bootmode();
    printf("Preloader boot mode: %s\n", bootmode2str(bootmode));
    if (bootmode == BOOTMODE_FACTORY) {
        set_bootmode(BOOTMODE_FASTBOOT);
        gd.reason = MODE_REASON_FACTORY;
        return;
    } else if (bootmode == BOOTMODE_ATEFACT) {
        set_bootmode(BOOTMODE_RECOVERY);
        gd.reason = MODE_REASON_FACTORY;
        return;
    }

    read_and_set_bootmode_from_message();
    if (get_bootmode() != BOOTMODE_NORMAL) {
        gd.reason = MODE_REASON_MISC;
    }
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && mode != BOOTMODE_FASTBOOT
        && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal
        // boot.
        show_bootmode(mode);
    }

#ifdef HAVE_LATE_INIT
    device_late_init();
#endif
}
