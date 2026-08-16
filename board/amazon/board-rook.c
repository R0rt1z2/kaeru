//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define RTC_PDN1                0x802c
#define RTC_PDN1_FAST_BOOT      0x2000
#define RTC_PDN1_RECOVERY_MASK	0x0030

// devinfo[6] bit 8
#define DEVINFO_BROM_CMD_DIS    0x10206060

#define GPIO_KEY_VOLUME_UP   50
#define GPIO_KEY_VOLUME_DOWN 37

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
    ((void (*)(const char *, void *, unsigned))(0x4BD3C960|1))(arg, data, sz);
}

static inline void cmd_erase(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4BD3C9B9|1))(arg, data, sz);
}

static inline void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4BD3AA90|1))(arg, data, sz);
}

static inline void rtc_writeif_unlock(void) {
    return ((void (*)(void))(0x4BD244B4|1))();
}

static inline void rtc_write_trigger(void) {
    return ((void (*)(void))(0x4BD24488|1))();
}

static inline void pwrap_read(uint32_t reg, uint32_t *val) {
    return ((void (*)(uint32_t, uint32_t *))(0x4BD23EDC|1))(reg, val);
}

static inline void pwrap_write(uint32_t reg, uint32_t val) {
    return ((void (*)(uint32_t, uint32_t))(0x4BD23EEC|1))(reg, val);
}

static inline bool mtk_detect_pmic_just_rst(void) {
    return ((bool (*)(void))(0x4BD23408|1))();
}

static inline bool gpio_input_get(uint32_t pin) {
    return ((uint32_t (*)(uint32_t))(0x4BD1F37C|1))(pin) == 1;
}

static const char *modereason2str(enum mode_reason reason) {
    switch (reason) {
        case MODE_REASON_NONE:
            return "None";
        case MODE_REASON_MISC:
            return "BCB";
        case MODE_REASON_RTC:
            return "RTC";
        case MODE_REASON_KEY:
            return "Volume Keys";
        case MODE_REASON_FACTORY:
            return "Factory";
        default:
            return NULL;
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
	pdn1 = pdn1 & ~clr_bits;
	pwrap_write(RTC_PDN1, pdn1);
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
    // These partitions are critical, flashing them incorrectly can lead to a hard brick.
    // To prevent accidental damage, we mark them as protected and block write access.
    if (strcmp(partition, "lk") == 0 || strcmp(partition, "tee1") == 0 ||
        strcmp(partition, "boot0") == 0 || strcmp(partition, "boot1") == 0 ||
        strcmp(partition, "preloader") == 0) {
        return !gd.unlocked_critical;
    }

    // Erasing the partition where kaeru would be installed is also a bad idea...
    if (erase && strcmp(partition, CONFIG_BOOTLOADER_PARTITION_NAME) == 0) {
        return !gd.unlocked_critical;
    }

    return 0;
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

static void cmd_flash_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = arg;
    advance_partition_name(&part);

    if (!gd.bypass_remap && strcmp(part, "lk") == 0) {
        // Redirect to where LK is actually loaded from.
        arg = CONFIG_BOOTLOADER_PARTITION_NAME;
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

    cmd_erase(arg, data, sz);
}

static void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz) {
    cmd_reboot_write_message(arg);
    cmd_reboot("", data, sz);
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

    // We invalidated LK's own combos, so do the detection ourselves.
    bool up = !gpio_input_get(GPIO_KEY_VOLUME_UP);
    bool down = !gpio_input_get(GPIO_KEY_VOLUME_DOWN);
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
    video_printf(" => HACKED FASTBOOT mode...\n");

    // Register our custom command(s).
    fastboot_publish("boot-reason", modereason2str(gd.reason));
    fastboot_publish("brom-cmd-dis", brom_cmd_disabled() ? "1" : "0");
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);
    fastboot_register("oem part-remap", cmd_oem_bypass_remap, 1);
}

void board_early_init(void) {
    printf("Entering early init for Echo Spot 1st Gen (2017)\n");

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Amazon uses IDME to check whether the device is locked or unlocked.
    // These functions verify if the unlock code matches the one stored in
    // the IDME partition and perform additional unlock verification checks.
    //
    // We patch them to bypass security checks and allow unrestricted fastboot
    // access regardless of the actual device lock state.
    FORCE_RETURN(0x4BD1D51C, 1);

    // The following patches disable specific video_printf calls that display
    // mode-specific messages during boot. These default messages can be
    // confusing to end users and don't provide useful information.
    //
    // We disable them here and will display our own custom messages in
    // board_late_init instead..
    PATCH_MEM(0x4BD20754, 0x4770, 0xBF00);
    PATCH_MEM(0x4BD20738, 0x4770, 0xBF00);

    // Do not forcefully override SELinux state on the kernel
    // command line.
    char *selinux_permissive = (char *)0x4BD5EAAC;
    selinux_permissive[2] = '\0';
    char *selinux_enforce = (char *)0x4BD5EAD0;
    selinux_enforce[2] = '\0';

    // Override LK's default boot mode handling.
    PATCH_CALL(0x4BD20D5A, &real_boot_mode_select, TARGET_THUMB);

    // Kill LK's factory reset key combo, it wipes userdata when held.
    FORCE_RETURN(0x4BD1DAB8, 0);

    // Same for LK's own fastboot key check, it overrides our selection.
    FORCE_RETURN(0x4BD1DA60, 0);

    // Get rid of the stock mode strings LK draws during boot. They are
    // confusing, tell the user nothing useful, and the fastboot one lands
    // right before our banner. Both call sites that print the fastboot one
    // share the string, so blanking it covers both.
    WRITE8(0x4BD56980, '\0'); // " => FASTBOOT mode...\n"
    WRITE8(0x4BD5EB80, '\0'); // " => RECOVERY mode...\n"

    // Silence everything LK draws while serving fastboot commands, it all
    // ends up on top of our banner.
    NOP(0x4BD3C1D8, 2); // screen clear
    NOP(0x4BD3C0A8, 2); // fastboot_fail echo
    PATCH_MEM(0x4BD3C0F2, 0x4770, 0xBF00); // "USB Transmission OK  Time:%dms Vel:%dKB/s"
    NOP(0x4BD3C232, 2); // "USB Transferring... "
    NOP(0x4BD3C59A, 2); // "Writing Flash ... "
    NOP(0x4BD3C6E2, 2); // "Write Data" progress
    NOP(0x4BD3C898, 2); // "OK"

    // Disable built in fastboot commands.
    NOP(0x4BD3A44A, 2); // fastboot flash
    NOP(0x4BD3A458, 2); // fastboot erase
    NOP(0x4BD3A482, 2); // fastboot reboot
    NOP(0x4BD3A490, 2); // fastboot reboot-bootloader

    // Redirect the call that prints "fastboot_init()\n" to our
    // hook, so we can register custom fastboot commands and
    // other things.
    PATCH_CALL(0x4BD3A3C2, &fastboot_init_hook, TARGET_THUMB);
}

void board_late_init(void) {
    printf("Entering late init for Echo Spot 1st Gen (2017)\n");

    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && mode != BOOTMODE_FASTBOOT
        && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal
        // boot. Fastboot is left out since our own banner covers it.
        show_bootmode(mode);
    }
}
