//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include "include/mt8127-common.h"

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
} gd;

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
    // The preloader hands us its boot mode in a block whose magic is
    // 'PLPL'. Act on it before anything else, forcing fastboot on a
    // factory boot and recovery on an ATE factory boot.
    uint32_t *arg = *(uint32_t **)PL_BOOTARG_PTR_ADDR;
    uint32_t pl_mode = (arg && arg[0] == PL_BOOTARG_MAGIC) ? (arg[1] & 0xFF)
                                                           : BOOTMODE_NORMAL;
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
    if (strcmp(partition, "TEE1") == 0 ||
        strcmp(partition, "TEE2") == 0 ||
        strcmp(partition, "boot0") == 0 ||
        strcmp(partition, "boot1") == 0 ||
        strcmp(partition, "preloader") == 0) {
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
    const char *part = arg;
    advance_partition_name(&part);

    if (is_partition_protected(part, false)) {
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

static void fastboot_init_hook(const char *) {
    fastboot_publish("boot-reason", modereason2str(gd.reason));

    // Registered here because this hook runs before LK registers its own, and
    // the first match in the command list is the one that serves.
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
}

void board_early_init(void) {
    printf("Entering early init for %s\n", BOARD_NAME);

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Amazon uses IDME to check whether the device is locked or unlocked.
    // These functions verify if the unlock code matches the one stored in
    // the IDME partition and perform additional unlock verification checks.
    //
    // We patch them to bypass security checks and allow unrestricted fastboot
    // access regardless of the actual device lock state.
    FORCE_RETURN(UNLOCK_CODE_FUNC_ADDR, 1);
    FORCE_RETURN(UNLOCK_STATUS_FUNC_ADDR, 0);

    // The following patches disable specific video_printf calls that display
    // mode-specific messages during boot. These default messages can be
    // confusing to end users and don't provide useful information.
    //
    // We disable them here and will display our own custom messages in
    // board_late_init instead..
    NOP(FB_MODE_PRINTF_CALL_ADDR, 2);

    // Force enable UART. This *can* be enabled on a per-boot basis with the
    // "oem p2u" command, but if someone is using a UART USB cable then that
    // simply isn't going to work.
    char *disable_uart = (char *)PRINTK_DISABLE_UART_ADDR;
    disable_uart[20] = '0'; // "printk.disable_uart=1"
    disable_uart = (char *)PRINTK_DISABLE_UART_ADDR_2;
    disable_uart[21] = '0'; // " printk.disable_uart=1"

    // Override LK's default boot mode handling. Ours runs in late init
    // instead, once storage is up and misc can be read.
    PATCH_CALL(BOOT_MODE_SELECT_CALL_ADDR, &real_boot_mode_select, TARGET_THUMB);

    // Disable built in fastboot commands.
    NOP(FB_REGISTER_FLASH_ADDR, 2);
    NOP(FB_REGISTER_ERASE_ADDR, 2);
    NOP(FB_REGISTER_REBOOT_ADDR, 2);
    NOP(FB_REGISTER_REBOOT_BOOTLOADER_ADDR, 2);

    // Redirect the call that prints "fastboot_init()\n" to our
    // hook, so we can register custom fastboot commands and
    // other things.
    PATCH_CALL(FASTBOOT_INIT_PRINTF_CALL_ADDR, &fastboot_init_hook, TARGET_THUMB);

#ifdef HAVE_EARLY_INIT
    device_early_init();
#endif
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

    bootmode_t mode = get_bootmode();
    if (mode == BOOTMODE_FASTBOOT) {
        video_printf(" => HACKED FASTBOOT mode (%s) - xyz, k4y0z, bengris32\n",
                     modereason2str(gd.reason));
    } else if (mode != BOOTMODE_NORMAL) {
        video_printf(" => %s mode (%s)...\n", bootmode2str(mode), modereason2str(gd.reason));
    }

#ifdef HAVE_LATE_INIT
    device_late_init();
#endif
}
