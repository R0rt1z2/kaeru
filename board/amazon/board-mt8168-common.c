//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include "include/mt8168-common.h"

#define RTC_PDN1                0x05B4
#define RTC_PDN1_FAST_BOOT      0x2000
#define RTC_PDN1_RECOVERY_MASK  0x0030
#define RTC_PDN1_RECOVERY_VAL   0x0010

#define BOOT_ARG_MAGIC          0x504C504C

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
    bool kpoc;
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

static inline uint32_t rtc_read(uint32_t reg) {
    return ((uint32_t (*)(uint32_t))(RTC_READ_FUNC_ADDR|1))(reg);
}

static inline void rtc_mark_fast_boot(bool enable) {
    ((void (*)(int))(RTC_MARK_FAST_BOOT_FUNC_ADDR|1))(enable ? 1 : 0);
}

static inline bool mtk_detect_pmic_just_rst(void) {
    return ((bool (*)(void))(MTK_DETECT_PMIC_JUST_RST_ADDR|1))();
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

static uint32_t get_preloader_bootmode(void) {
    uint32_t *arg = *(uint32_t **)BOOT_ARG_PTR_ADDR;

    if (!arg || arg[0] != BOOT_ARG_MAGIC)
        return BOOTMODE_NORMAL;

    // LK only ever looks at the low byte, so do the same.
    return arg[1] & 0xFF;
}

static void read_boot_keys(bool *up, bool *down) {
    *up = mtk_detect_key(KEY_VOLUME_UP);
    *down = mtk_detect_key(KEY_VOLUME_DOWN);
}

static bool is_partition_protected(const char* partition, bool erase) {
    // These partitions are critical, flashing them incorrectly can lead to a
    // hard brick. To prevent accidental damage, we mark them as protected and
    // block write access.
    if (strcmp(partition, "lk") == 0 || strcmp(partition, "lk2") == 0 ||
        strcmp(partition, "preloader") == 0 || strcmp(partition, "tee1") == 0 ||
        strcmp(partition, "tee2") == 0) {
        return !gd.unlocked_critical;
    }

    // Erasing the partition where kaeru would be installed is also a bad idea...
    if (erase && strcmp(partition, CONFIG_BOOTLOADER_PARTITION_NAME) == 0) {
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

    if (is_partition_protected(part, false)) {
        critical_op_fail("You are attempting to flash to a critical partition.");
        return;
    }

    cmd_flash(arg, data, sz);
}

static void cmd_erase_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = skip_spaces(arg);

    if (is_partition_protected(part, true)) {
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

static void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz) {
#ifdef HAVE_FASTBOOT_CMD_REBOOT
    device_fastboot_cmd_reboot(arg, data, sz);
#endif

    cmd_reboot_write_message(arg);
    cmd_reboot("", data, sz);
}

static void cmd_oem_reboot_recovery(const char *arg, void *data, unsigned sz) {
    // LK's version of this goes through RTC. Send it down the same path as
    // 'fastboot reboot recovery' instead.
    cmd_reboot_wrapper("-recovery", data, sz);
}

static void cmd_kaeru_version(const char *arg, void *data, unsigned sz) {
    video_set_cursor(video_get_rows() / 12, 0);
    cmd_version(arg, data, sz);
}

#ifdef BOOTOPT_PARSE_CALL_ADDR
#define BOOTOPT_HDR_OFFSET  64
#define BOOTOPT_PAGE_OFFSET 36
#define BOOTOPT_READ_SIZE   3696

// Word a 32-bit zImage carries at offset 36. An arm64 Image has one of
// its own, but it is no help since both arrive packed most of the time.
#define ZIMAGE_MAGIC        0x016F2818

static void bootopt_parse_hook(const char *cmdline) {
    uint32_t hdr = (uint32_t)(uintptr_t)cmdline - BOOTOPT_HDR_OFFSET;
    int bits = cmdline_kernel_bits(cmdline);

    if (bits < 0) {
        uint32_t page = READ32(hdr + BOOTOPT_PAGE_OFFSET);

        bits = 64;
        if (page && page + 40 <= BOOTOPT_READ_SIZE
            && READ32(hdr + page + 36) == ZIMAGE_MAGIC)
            bits = 32;
    }

    if (bits == 64) {
        printf("Boot image requests a 64-bit kernel, forcing it\n");
        WRITE32(KERNEL_64BIT_FLAG_ADDR, 1);
    } else if (bits == 32) {
        printf("Boot image requests a 32-bit kernel, forcing it\n");
        WRITE32(KERNEL_64BIT_FLAG_ADDR, 0);
    }
}
#endif

static void real_boot_mode_select(void) {
    // We use this opportunity to grab RTC_PDN1 for use later,
    // as RTC driver init will clear out the recovery bits,
    // which is not what we want when we have to detect
    // RTC recovery mode.
    gd.rtc_pdn1 = rtc_read(RTC_PDN1);

    // Consume the fast boot bit now, off the latched copy, so a boot claimed
    // by another signal can't leave it set and strand the next one in fastboot.
    if (gd.rtc_pdn1 & RTC_PDN1_FAST_BOOT)
        rtc_mark_fast_boot(false);

    // Set bootmode to BOOTMODE_NORMAL for good measure.
    set_bootmode(BOOTMODE_NORMAL);

#ifdef KPOC_DETECT_FUNC_ADDR
    // Unlike the Echos, the tablets charge while powered off. LK sets up the
    // charger and the backlight based on this right after we return, so it
    // can't wait until boot_mode_select(). Stock only gets this far when the
    // preloader and the keys had nothing to say, so keep it that way.
    bool up, down;
    read_boot_keys(&up, &down);
    if (get_preloader_bootmode() == BOOTMODE_NORMAL && !up && !down)
        gd.kpoc = kernel_power_off_charging_detection();
#endif
}

static void boot_mode_select(void) {
    // Clear out the reset flag from the PMIC. We really don't care
    // about the return, but not calling this function could mess
    // things up.
    mtk_detect_pmic_just_rst();

    // real_boot_mode_select() may have left us in off-mode charging. Start
    // over from a normal boot, it only comes back if nothing below objects.
    set_bootmode(BOOTMODE_NORMAL);

    // The preloader hands us its boot mode in the boot arg block LK keeps
    // a pointer to. Act on it before anything else, forcing fastboot on a
    // factory boot and recovery on an ATE factory boot.
    uint32_t pl_mode = get_preloader_bootmode();
    if (pl_mode == BOOTMODE_FACTORY) {
        set_bootmode(BOOTMODE_FASTBOOT);
        gd.reason = MODE_REASON_FACTORY;
        return;
    } else if (pl_mode == BOOTMODE_ATEFACT) {
        set_bootmode(BOOTMODE_RECOVERY);
        gd.reason = MODE_REASON_FACTORY;
        return;
    } else if (pl_mode == BOOTMODE_META || pl_mode == BOOTMODE_RECOVERY ||
               pl_mode == BOOTMODE_ADVMETA || pl_mode == BOOTMODE_ALARM ||
               pl_mode == BOOTMODE_FASTBOOT) {
        // Everything else LK would take from the preloader goes through
        // as is.
        set_bootmode((bootmode_t)pl_mode);
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
    bool up, down;
    read_boot_keys(&up, &down);
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
        // Check recovery before fast boot: a reboot to recovery can set both
        // bits, and fast boot would otherwise win. Both are already consumed.
        if ((gd.rtc_pdn1 & RTC_PDN1_RECOVERY_MASK) == RTC_PDN1_RECOVERY_VAL) {
            set_bootmode(BOOTMODE_RECOVERY);
            gd.reason = MODE_REASON_RTC;
        } else if (read_rtc_mode(RTC_PDN1_FAST_BOOT)) {
            set_bootmode(BOOTMODE_FASTBOOT);
            gd.reason = MODE_REASON_RTC;
        }
    }

    // Nobody asked for anything, so let off-mode charging have it.
    if (get_bootmode() == BOOTMODE_NORMAL && gd.kpoc)
        set_bootmode(BOOTMODE_KERNEL_POWEROFF_CHARGING);
}

static void fastboot_init_hook(const char *) {
    video_printf(" => HACKED FASTBOOT mode - R0rt1z2\n");
    fastboot_publish("boot-reason", modereason2str(gd.reason));

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("oem reboot-recovery", cmd_oem_reboot_recovery, 1);
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

    // LK appends androidboot.selinux=enforcing unless IDME flags ask
    // otherwise. Drop it and leave the cmdline alone.
    FORCE_RETURN(SELINUX_CMDLINE_FUNC_ADDR, 0);

    // LK only disables dm-verity (both on the cmdline and in the vendor
    // fstab of the DTB) when IDME carries the right flags, and hands
    // everyone else eio mode. Make that check always pass.
    FORCE_RETURN(VERITY_CHECK_FUNC_ADDR, 1);

#ifdef VBSTATE_FORCE_ADDRS
    // Both sites that write LK's boot state store 0, so verifiedbootstate
    // ends up green no matter how the device booted. Store 1 at each instead
    // so the kernel gets orange.
    static const uint32_t vbstate_sites[] = { VBSTATE_FORCE_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(vbstate_sites); i++)
        PATCH_MEM(vbstate_sites[i], 0x2201);
#endif

#ifdef ORANGE_STATE_WARN_FUNC_ADDR
    // An orange boot state also buys a warning screen and a two second wait
    // on the way to the kernel. We wanted the state, not the wait.
    FORCE_RETURN(ORANGE_STATE_WARN_FUNC_ADDR, 0);
#endif

#ifdef BOOTOPT_PARSE_CALL_ADDR
    // LK does read 'bootopt=' here, but it gives up on images that carry
    // none and leaves the kernel 32-bit. Take the call over so we can fall
    // back to what the kernel image itself says.
    PATCH_CALL(BOOTOPT_PARSE_CALL_ADDR, &bootopt_parse_hook, TARGET_THUMB);
#endif

    // Disable the built in flash and erase commands. Ours are registered
    // from the hook below, which runs before LK gets to register its own,
    // and the first match in the command list is the one that serves.
    NOP(FB_REGISTER_FLASH_ADDR, 2);
    NOP(FB_REGISTER_ERASE_ADDR, 2);

    // Same treatment for reboot and reboot-bootloader, so our own reboot
    // handler (which records the target in misc) is the one that serves.
    NOP(FB_REGISTER_REBOOT_ADDR, 2);
    NOP(FB_REGISTER_REBOOT_BOOTLOADER_ADDR, 2);

    // Set up our getvar handler to make fastboot fetch work.
    NOP(FB_REGISTER_GETVAR_ADDR, 2);

    // Let our own 'oem idme' serve instead of LK's.
    FORCE_RETURN(LK_IDME_REGISTER_FUNC_ADDR, 0);

    // Silence everything LK draws while serving fastboot commands, it all
    // ends up on top of our banner.
    const uint32_t video_calls[] = { FB_VIDEO_CALL_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(video_calls); i++)
        NOP(video_calls[i], 2);

    // These are tail calls, so they have to return instead of running into
    // whatever comes behind them.
    const uint32_t video_tail_calls[] = { FB_VIDEO_TAIL_CALL_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(video_tail_calls); i++)
        PATCH_MEM(video_tail_calls[i], 0x4770, 0xBF00);

    // 'fastboot download' also wipes the screen and parks the cursor halfway
    // down it before printing, which takes our banner with it. Drop both.
    const uint32_t clean_screen_calls[] = { FB_CLEAN_SCREEN_CALL_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(clean_screen_calls); i++)
        NOP(clean_screen_calls[i], 2);

    // Redirect the call that prints "fastboot_init()\n" to our hook, so
    // we can register custom fastboot commands and other things.
    PATCH_CALL(FASTBOOT_INIT_PRINTF_CALL_ADDR, &fastboot_init_hook, TARGET_THUMB);

    // Get rid of the stock mode string LK draws at the end of platform_init().
    // It is confusing and tells the user nothing useful.
    NOP(SHOW_BOOT_MODE_CALL_ADDR, 2);

    // Override LK's default boot mode handling.
    PATCH_CALL(BOOT_MODE_SELECT_CALL_ADDR, &real_boot_mode_select, TARGET_THUMB);

#ifdef AMZN_FW_AUTH_PASS_FUNCS
    // Force coprocessor firmware (VPU, spmfw, ...) signature verify and anti-
    // rollback checks to pass, otherwise LK ASSERTs and halts the boot.
    static const uint32_t fw_auth[] = { AMZN_FW_AUTH_PASS_FUNCS };
    for (uint32_t i = 0; i < ARRAY_SIZE(fw_auth); i++)
        FORCE_RETURN(fw_auth[i], 0);
#endif

#ifdef HAVE_EARLY_INIT
    device_early_init();
#endif
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

    // LK reloads the DTB from recovery right after its own mode selection,
    // but we only know the mode by now. Do the same, otherwise recovery
    // boots with the DTB from boot.
    if (get_bootmode() == BOOTMODE_RECOVERY && recovery_dtbo_applied() != 1) {
        if (bldr_load_dtb("recovery") < 0)
            printf("Failed to load the recovery DTB\n");
    }

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && mode != BOOTMODE_FASTBOOT
        && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal
        // boot. Fastboot is left out since our own banner covers it.
        show_bootmode(mode);
    }

#ifdef HAVE_LATE_INIT
    device_late_init();
#endif
}
