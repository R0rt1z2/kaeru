//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include "include/mt8163-common.h"

#define RTC_PDN1                0x802C
#define RTC_PDN1_FAST_BOOT      0x2000
#define RTC_PDN1_RECOVERY_MASK  0x0030

// devinfo[6] bit 8
#define DEVINFO_BROM_CMD_DIS    0x10206060

#define VOLUME_UP   0
#define VOLUME_DOWN 1

enum mode_reason {
    MODE_REASON_NONE = 0,
    MODE_REASON_MISC = 1,
    MODE_REASON_RTC = 2,
    MODE_REASON_KEY = 3,
    MODE_REASON_FACTORY = 4,
};

static struct {
    enum mode_reason reason;
    uint32_t rtc_pdn1;
    bool unlocked_critical;
    bool bypass_remap;
} gd;

static inline void cmd_flash(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_FLASH_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_erase(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_ERASE_FUNC_ADDR|1))(arg, data, sz);
}

static inline void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(FB_CMD_REBOOT_FUNC_ADDR|1))(arg, data, sz);
}

static inline void pwrap_read(uint32_t reg, uint32_t *val) {
    ((void (*)(uint32_t, uint32_t *))(PWRAP_READ_FUNC_ADDR|1))(reg, val);
}

static inline void pwrap_write(uint32_t reg, uint32_t val) {
    ((void (*)(uint32_t, uint32_t))(PWRAP_WRITE_FUNC_ADDR|1))(reg, val);
}

static inline void rtc_writeif_unlock(void) {
    ((void (*)(void))(RTC_WRITEIF_UNLOCK_FUNC_ADDR|1))();
}

static inline void rtc_write_trigger(void) {
    ((void (*)(void))(RTC_WRITE_TRIGGER_FUNC_ADDR|1))();
}

static inline bool mtk_detect_pmic_just_rst(void) {
    return ((bool (*)(void))(MTK_DETECT_PMIC_JUST_RST_ADDR|1))();
}

static inline int load_recovery_hdr(const char *partition, uint32_t addr) {
    return ((int (*)(const char *, uint32_t))(LOAD_RECOVERY_HDR_FUNC_ADDR|1))(partition, addr);
}

static inline int load_recovery_img(const char *partition, uint32_t addr) {
    return ((int (*)(const char *, uint32_t))(LOAD_RECOVERY_IMG_FUNC_ADDR|1))(partition, addr);
}

static const char *modereason2str(enum mode_reason reason) {
    switch (reason) {
        case MODE_REASON_MISC:
            return "BCB";
        case MODE_REASON_RTC:
            return "RTC";
        case MODE_REASON_KEY:
            return "Volume Keys";
        case MODE_REASON_FACTORY:
            return "Factory";
        default:
            return "None";
    }
}

static inline bool read_rtc_mode(uint32_t mask) {
    // This comes from the saved RTC_PDN1 we got
    // from real_boot_mode_select().
    return !!(gd.rtc_pdn1 & mask);
}

static void clear_rtc_mode(uint32_t clr_bits) {
    uint32_t pdn1;

    rtc_writeif_unlock();
    pwrap_read(RTC_PDN1, &pdn1);
    pwrap_write(RTC_PDN1, pdn1 & ~clr_bits);
    rtc_write_trigger();
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

    if (strcmp(partition, "recovery") == 0)
        // Redirect to the actual recovery partition.
        return RECOVERY_PARTITION;

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

static bool brom_cmd_disabled(void) {
    return (READ32(DEVINFO_BROM_CMD_DIS) >> 8) & 1;
}

static void cmdline_append(const char *arg) {
    char *cl = (char *)KERNEL_CMDLINE_ADDR;
    size_t len = strlen(cl);

    if (len + strlen(arg) + 2 < 0x400)
        npf_snprintf(cl + len, 0x400 - len, " %s", arg);
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

static void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz) {
    cmd_reboot_write_message(arg);
    cmd_reboot("", data, sz);
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

    // Stock LK always boots a 32-bit kernel no matter what...
    if (cmdline_kernel_bits(cmdline) == 64) {
        printf("Boot image requests a 64-bit kernel, forcing it\n");
        WRITE32(KERNEL_64BIT_FLAG_ADDR, 1);
    }
}

static int load_recovery_hdr_hook(const char *, uint32_t addr) {
    return load_recovery_hdr(RECOVERY_PARTITION, addr);
}

static int load_recovery_img_hook(const char *, uint32_t addr) {
    return load_recovery_img(RECOVERY_PARTITION, addr);
}

static void cmd_kaeru_version(const char *arg, void *data, unsigned sz) {
    video_set_cursor(video_get_rows() / 12, 0);
    cmd_version(arg, data, sz);
}

static void real_boot_mode_select(void) {
    // We use this opportunity to grab RTC_PDN1 for use later,
    // as RTC driver init will clear out the recovery bits,
    // which is not what we want when we have to detect
    // RTC recovery mode.
    pwrap_read(RTC_PDN1, &gd.rtc_pdn1);

    // Set bootmode to BOOTMODE_NORMAL for good measure.
    set_bootmode(BOOTMODE_NORMAL);
}

static void boot_mode_select(void) {
    // Clear out the reset flag from the PMIC. We really don't care
    // about the return, but not calling this function could mess
    // things up.
    mtk_detect_pmic_just_rst();

    // The preloader hands us its boot mode in the boot arg block at
    // BOOTLOADER_BASE + 0x20. Act on it before anything else, forcing
    // fastboot on a factory boot and recovery on an ATE factory boot.
    uint32_t *arg = *(uint32_t **)(CONFIG_BOOTLOADER_BASE + 0x20);
    uint32_t pl_mode = arg ? arg[1] : BOOTMODE_NORMAL;
    if (pl_mode == BOOTMODE_FACTORY) {
        set_bootmode(BOOTMODE_FASTBOOT);
        gd.reason = MODE_REASON_FACTORY;
        return;
    } else if (pl_mode == BOOTMODE_ATEFACT) {
        set_bootmode(BOOTMODE_RECOVERY);
        gd.reason = MODE_REASON_FACTORY;
        return;
    }

    // Act on any boot command left in misc before anything else, so a
    // key press can still override it below.
    read_and_set_bootmode_from_message();
    if (get_bootmode() != BOOTMODE_NORMAL)
        gd.reason = MODE_REASON_MISC;

    // Amazon removed the ability to enter fastboot / recovery mode with
    // the volume keys, we restore that here. Require an exclusive hold so
    // holding both does nothing.
    bool up = mtk_detect_key(VOLUME_UP);
    bool down = mtk_detect_key(VOLUME_DOWN);
    if (up && !down) {
        set_bootmode(BOOTMODE_RECOVERY);
        gd.reason = MODE_REASON_KEY;
    } else if (down && !up) {
        set_bootmode(BOOTMODE_FASTBOOT);
        gd.reason = MODE_REASON_KEY;
    }

    // If our bootmode is STILL normal after all that, give a chance for
    // RTC to select the boot mode, for compatibility with stock OS.
    if (get_bootmode() == BOOTMODE_NORMAL) {
        if (read_rtc_mode(RTC_PDN1_FAST_BOOT)) {
            clear_rtc_mode(RTC_PDN1_FAST_BOOT);
            set_bootmode(BOOTMODE_FASTBOOT);
            gd.reason = MODE_REASON_RTC;
        } else if (read_rtc_mode(RTC_PDN1_RECOVERY_MASK)) {
            clear_rtc_mode(RTC_PDN1_RECOVERY_MASK);
            set_bootmode(BOOTMODE_RECOVERY);
            gd.reason = MODE_REASON_RTC;
        }
    }

    // We would normally handle KPOC here too, but Amazon disable that
    // as well ¯\_(ツ)_/¯
}

static void fastboot_init_hook(const char *) {
    video_printf(" => HACKED FASTBOOT mode - xyz, k4y0z, R0rt1z2, bengris32\n");
    fastboot_publish("boot-reason", modereason2str(gd.reason));
    fastboot_publish("brom-cmd-dis", brom_cmd_disabled() ? "1" : "0");

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);
    fastboot_register("oem part-remap", cmd_oem_bypass_remap, 1);

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

    // Same treatment for reboot and reboot-bootloader, so our own reboot
    // handler (which records the target in misc) is the one that serves.
    NOP(FB_REGISTER_REBOOT_ADDR, 2);
    NOP(FB_REGISTER_REBOOT_BOOTLOADER_ADDR, 2);

    // Silence everything LK draws while serving fastboot commands, it all
    // ends up on top of our banner.
    const uint32_t video_calls[] = { FB_VIDEO_CALL_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(video_calls); i++)
        NOP(video_calls[i], 2);

    // This one is a tail call, so it has to return instead of running into
    // the literals behind it.
    PATCH_MEM(FB_VIDEO_TAIL_CALL_ADDR, 0x4770, 0xBF00);

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

    // Load recovery from a dedicated partition instead of the stock one. LK
    // reads the header and the image in two passes, so hook both.
    PATCH_CALL(LOAD_RECOVERY_HDR_CALL_ADDR, &load_recovery_hdr_hook, TARGET_THUMB);
    PATCH_CALL(LOAD_RECOVERY_IMG_CALL_ADDR, &load_recovery_img_hook, TARGET_THUMB);

    // Override LK's default boot mode handling.
    PATCH_CALL(BOOT_MODE_SELECT_CALL_ADDR, &real_boot_mode_select, TARGET_THUMB);

    // Kill LK's factory reset key combo, it wipes userdata when held.
    FORCE_RETURN(FACTORY_RESET_CHECK_ADDR, 0);

    // Same for LK's own fastboot key check, it overrides our selection.
    FORCE_RETURN(FASTBOOT_KEY_CHECK_ADDR, 0);
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

    // Expose brom_cmd_dis to the OS as ro.boot.brom_cmd_dis.
    cmdline_append(brom_cmd_disabled() ? "androidboot.brom_cmd_dis=1"
                                       : "androidboot.brom_cmd_dis=0");

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && mode != BOOTMODE_FASTBOOT
        && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal
        // boot. Fastboot is left out since our own banner covers it.
        show_bootmode(mode);
    }
}
