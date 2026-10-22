//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#ifdef CONFIG_AMAZON_MT8163_AB
#include <lib/bcb_amzn/bcblib.h>
#endif

#include "include/mt8163-common.h"

#define RTC_PDN1                0x802C
#define RTC_PDN1_FAST_BOOT      0x2000
#define RTC_PDN1_RECOVERY_MASK  0x0030

// devinfo[6] bit 8
#define DEVINFO_BROM_CMD_DIS    0x10206060

#define VOLUME_UP   0
#define VOLUME_DOWN 1

#ifdef CONFIG_AMAZON_MT8163_AB
#define GPIO_BASE           0x10005000
#define GPIO_DIN_OFFSET     0x500
#define GPIO_REG_STRIDE     0x10
#define GPIO_PINS_PER_REG   16
#endif

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
#ifdef CONFIG_AMAZON_MT8163_AB
    uint64_t misc_offset;
    struct bcb bcb;
    bool bcb_dirty;
    bool have_booted_slot;
    int booted_slot;
    uint8_t blk_buf[BLOCK_SIZE] __attribute__((aligned(64)));
#endif
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

#ifdef LOAD_RECOVERY_HDR_FUNC_ADDR
static inline int load_recovery_hdr(const char *partition, uint32_t addr) {
    return ((int (*)(const char *, uint32_t))(LOAD_RECOVERY_HDR_FUNC_ADDR|1))(partition, addr);
}

static inline int load_recovery_img(const char *partition, uint32_t addr) {
    return ((int (*)(const char *, uint32_t))(LOAD_RECOVERY_IMG_FUNC_ADDR|1))(partition, addr);
}
#endif

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

#ifdef CONFIG_AMAZON_MT8163_AB
    if (strcmp(partition, "lk_a") == 0 || strcmp(partition, "lk_b") == 0) {
        return !gd.unlocked_critical;
    }
#endif

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

#ifdef CONFIG_AMAZON_MT8163_AB
    if (strcmp(partition, "lk_a") == 0 || strcmp(partition, "lk_b") == 0)
        return CONFIG_BOOTLOADER_PARTITION_NAME;
#endif

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

#ifdef CONFIG_AMAZON_MT8163_AB
static bool gpio_key_pressed(uint32_t pin) {
    uint32_t reg = GPIO_BASE + GPIO_DIN_OFFSET +
                   ((pin / GPIO_PINS_PER_REG) * GPIO_REG_STRIDE);

    return !(READ32(reg) & (1U << (pin % GPIO_PINS_PER_REG)));
}
#endif

static bool brom_cmd_disabled(void) {
    return (READ32(DEVINFO_BROM_CMD_DIS) >> 8) & 1;
}

#ifdef KERNEL_CMDLINE_ADDR
static void cmdline_append(const char *arg) {
    char *cl = (char *)KERNEL_CMDLINE_ADDR;
    size_t len = strlen(cl);

    if (len + strlen(arg) + 2 < KERNEL_CMDLINE_SIZE)
        npf_snprintf(cl + len, KERNEL_CMDLINE_SIZE - len, " %s", arg);
}
#endif

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
#ifdef HAVE_FASTBOOT_CMD_REBOOT
    device_fastboot_cmd_reboot(arg, data, sz);
#endif

    cmd_reboot_write_message(arg);
    cmd_reboot("", data, sz);
}

#ifdef HAVE_DISPLAY
static void blank_string(char *s, const char *name) {
    if (!s) {
        printf("Could not find the %s string\n", name);
        return;
    }

    printf("Found the %s string at 0x%08X\n", name, (uint32_t)(uintptr_t)s);
    WRITE8(s, '\0');
}
#endif

static void neuter_cmdline_format(char *fmt, const char *name) {
    if (!fmt) {
        printf("Could not find the %s cmdline format\n", name);
        return;
    }

    printf("Found the %s cmdline format at 0x%08X\n", name,
           (uint32_t)(uintptr_t)fmt);
    WRITE8(fmt + 2, '\0');
}

#ifdef BOOTIMG_CMDLINE_PRINT_CALL_ADDR
static void bootimg_cmdline_hook(const char *fmt, const char *tag,
                                 const char *cmdline) {
    printf(fmt, tag, cmdline);

    int bits = cmdline_kernel_bits(cmdline);
    if (bits == 64) {
        printf("Boot image requests a 64-bit kernel, forcing it\n");
        WRITE32(KERNEL_64BIT_FLAG_ADDR, 1);
    } else if (bits == 32) {
        printf("Boot image requests a 32-bit kernel, forcing it\n");
        WRITE32(KERNEL_64BIT_FLAG_ADDR, 0);
    }
}
#endif

#ifdef LOAD_RECOVERY_HDR_CALL_ADDR
static int load_recovery_hdr_hook(const char *, uint32_t addr) {
    return load_recovery_hdr(RECOVERY_PARTITION, addr);
}

static int load_recovery_img_hook(const char *, uint32_t addr) {
    return load_recovery_img(RECOVERY_PARTITION, addr);
}
#endif

#ifdef HAVE_DISPLAY
static void cmd_kaeru_version(const char *arg, void *data, unsigned sz) {
    video_set_cursor(video_get_rows() / 12, 0);
    cmd_version(arg, data, sz);
}
#endif


#ifdef CONFIG_AMAZON_MT8163_AB
static bool get_misc_offset(uint64_t *offset) {
    if (gd.misc_offset) {
        *offset = gd.misc_offset;
        return true;
    }

    part_t* misc_part = mt_part_get_partition(CONFIG_MISC_PARTITION_NAME);
    if (!misc_part) {
        printf("%s partition not found\n", CONFIG_MISC_PARTITION_NAME);
        return false;
    }

    gd.misc_offset = misc_part->start_sect * BLOCK_SIZE;
    *offset = gd.misc_offset;
    return true;
}

static void print_bcb(void) {
    int active_slot = bcblib_bcb_get_active_slot(&gd.bcb, false, false);
    printf("BCB [Magic: 0x%0x | Ver: %d]\n", gd.bcb.magic, gd.bcb.version);
    printf("Slot A: prio=%-2d tries=%-1d success=%d\n",
           gd.bcb.slot[0].priority, gd.bcb.slot[0].tries, gd.bcb.slot[0].success);
    printf("Slot B: prio=%-2d tries=%-1d success=%d\n",
           gd.bcb.slot[1].priority, gd.bcb.slot[1].tries, gd.bcb.slot[1].success);
    if (active_slot >= 0)
        printf("Active slot: %c\n", 'a' + active_slot);
    else
        printf("Active slot: NONE; WILL FAIL TO BOOT!\n");
}

static bool commit_bcb(void) {
    if (!gd.bcb_dirty) {
        printf("%s: Current BCB is clean, not writing\n", __func__);
        return true;
    }

    struct device_t *dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("%s: Block device not initialized for misc writing\n", __func__);
        return false;
    }

    if (!gd.misc_offset) {
        printf("%s: misc offset is unset, refusing to write\n", __func__);
        return false;
    }

    uint64_t off = gd.misc_offset + BCB_OFFSET;
    uint64_t blk = off & ~(uint64_t)(BLOCK_SIZE - 1);
    uint32_t in_blk = (uint32_t)(off - blk);

    size_t r = dev->read(dev, blk, gd.blk_buf, BLOCK_SIZE, USER_PART);
    if (r != BLOCK_SIZE) {
        printf("%s: Failed to read the BCB block\n", __func__);
        return false;
    }

    memcpy(gd.blk_buf + in_blk, &gd.bcb, sizeof(struct bcb));

    size_t w = dev->write(dev, gd.blk_buf, blk, BLOCK_SIZE, USER_PART);
    if (w != BLOCK_SIZE) {
        printf("%s: Failed to commit BCB\n", __func__);
        return false;
    }

    gd.bcb_dirty = false;
    return true;
}

static bool load_bcb(void) {
    // If the current BCB is dirty, then the course of action is
    // to re-load the fresh state from storage. Otherwise just
    // make sure the current BCB in memory isn't broken.
    if (!gd.bcb_dirty) {
        printf("%s: BCB is already loaded\n", __func__);
        goto check_bcb;
    }

    struct device_t *dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("%s: Block device not initialized for misc reading\n", __func__);
        return false;
    }

    // Read in the BCB
    uint64_t misc_offset;
    if (!get_misc_offset(&misc_offset))
        return false;

    // BCB lives at 0x360 into the misc partition.
    misc_offset += BCB_OFFSET;
    size_t read = dev->read(dev, misc_offset, &gd.bcb, sizeof(struct bcb), USER_PART);
    if (read != sizeof(struct bcb)) {
        printf("%s: Failed to read BCB\n", __func__);
        return false;
    }

    gd.bcb_dirty = false;

check_bcb:
    // Sanity check BCB
    printf("%s: Sanity check BCB...\n", __func__);

    if (!bcblib_bcb_magic_valid(&gd.bcb)) {
        printf("%s: BCB magic is invalid! Re-initialising.\n", __func__);
        bcblib_bcb_init(&gd.bcb);

        // Make sure we mark a slot as successful, the defaults will
        // eventually brick the device.
        gd.bcb.slot[0] = BCB_SLOT_METADATA_ACTIVE;
        gd.bcb.slot[1] = BCB_SLOT_METADATA_EMPTY;

        gd.bcb_dirty = true;
    } else if (!bcblib_metadata_get_success(&gd.bcb.slot[0]) &&
               !bcblib_metadata_get_success(&gd.bcb.slot[1])) {
        printf("%s: Both slots are failed, fixing.\n", __func__);

        int current = bcblib_bcb_get_active_slot(&gd.bcb, false, false);
        if (current < 0)
            current = 0;

        bcblib_metadata_set_success(&gd.bcb.slot[current], true);

        gd.bcb_dirty = true;
    } else {
        printf("%s: BCB is okay\n", __func__);
    }

    return commit_bcb();
}

static uint32_t get_active_slot(void) {
    // LK will not invoke this function if booted directly
    // to fastboot mode.
    if (gd.have_booted_slot)
        return gd.booted_slot;

    if (!load_bcb()) {
        printf("Failed to load BCB\n");
        return (uint32_t)-1;
    }

    print_bcb();

    // Choose the slot we will boot from.
    gd.booted_slot = bcblib_bcb_get_active_slot(&gd.bcb, false, false);

    // load_bcb() should fix-up the BCB if needed, but well...
    if (gd.booted_slot >= 0)
        gd.have_booted_slot = true;

    return gd.booted_slot;
}

static const char *get_boot_part_hook(int slot) {
    if (get_bootmode() == BOOTMODE_RECOVERY) {
        // Override the name of the boot partition to
        // the recovery one regardless of slot.
        return RECOVERY_PARTITION;
    }

    return get_boot_part(slot);
}

static void cmd_set_active(const char *arg, void *data, unsigned sz) {
    const char *slot = arg;

    if (*slot != 'a' && *slot != 'b') {
        fastboot_fail("Invalid slot. Use 'a' or 'b'");
        return;
    }

    if (!load_bcb()) {
        fastboot_fail("Failed to load BCB");
        return;
    }

    int idx = *slot - 'a';
    gd.bcb.slot[idx] = BCB_SLOT_METADATA_ACTIVE;
    gd.bcb.slot[1 - idx] = BCB_SLOT_METADATA_EMPTY;
    gd.bcb_dirty = true;

    if (!commit_bcb()) {
        fastboot_fail("Failed to write BCB");
        return;
    }

    fastboot_okay("");
}

static void cmd_print_bcb(const char *arg, void *data, unsigned sz) {
    char buf[128];

    if (!load_bcb()) {
        fastboot_fail("Failed to load BCB");
        return;
    }

    int active_slot = bcblib_bcb_get_active_slot(&gd.bcb, false, false);

    npf_snprintf(buf, sizeof(buf), "BCB [Magic: 0x%0x | Ver: %d]",
                 gd.bcb.magic, gd.bcb.version);
    fastboot_info(buf);

    npf_snprintf(buf, sizeof(buf), "Slot A: prio=%-2d tries=%-1d success=%d",
                 gd.bcb.slot[0].priority, gd.bcb.slot[0].tries, gd.bcb.slot[0].success);
    fastboot_info(buf);

    npf_snprintf(buf, sizeof(buf), "Slot B: prio=%-2d tries=%-1d success=%d",
                 gd.bcb.slot[1].priority, gd.bcb.slot[1].tries, gd.bcb.slot[1].success);
    fastboot_info(buf);

    if (active_slot >= 0)
        npf_snprintf(buf, sizeof(buf), "Active slot: %c", 'a' + active_slot);
    else
        npf_snprintf(buf, sizeof(buf), "Active slot: NONE; WILL FAIL TO BOOT!");
    fastboot_info(buf);

    fastboot_okay("");
}

static void cmd_reload_bcb(const char *arg, void *data, unsigned sz) {
    gd.bcb_dirty = true;
    if (!load_bcb()) {
        fastboot_fail("Failed to re-load BCB");
        return;
    }

    fastboot_okay("");
}
#endif // CONFIG_AMAZON_MT8163_AB

#ifdef BOOT_MODE_SELECT_CALL_ADDR
static void real_boot_mode_select(void) {
    // We use this opportunity to grab RTC_PDN1 for use later,
    // as RTC driver init will clear out the recovery bits,
    // which is not what we want when we have to detect
    // RTC recovery mode.
    pwrap_read(RTC_PDN1, &gd.rtc_pdn1);

    // Set bootmode to BOOTMODE_NORMAL for good measure.
    set_bootmode(BOOTMODE_NORMAL);
}
#endif

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
#ifdef CONFIG_AMAZON_MT8163_AB
    bool up = gpio_key_pressed(GPIO_KEY_VOLUME_UP);
    bool down = gpio_key_pressed(GPIO_KEY_VOLUME_DOWN);
#else
    bool up = mtk_detect_key(VOLUME_UP);
    bool down = mtk_detect_key(VOLUME_DOWN);
#endif
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
#ifdef HAVE_DISPLAY
    video_printf(" => HACKED FASTBOOT mode - xyz, k4y0z, R0rt1z2, bengris32\n");
#endif
    fastboot_publish("boot-reason", modereason2str(gd.reason));
    fastboot_publish("brom-cmd-dis", brom_cmd_disabled() ? "1" : "0");

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);
    fastboot_register("oem part-remap", cmd_oem_bypass_remap, 1);

#ifdef CONFIG_AMAZON_MT8163_AB
    // Support slot switching from fastboot.
    int active_slot = get_active_slot();
    fastboot_register("set_active:", cmd_set_active, 1);
    fastboot_register("oem print-bcb", cmd_print_bcb, 1);
    fastboot_register("oem reload-bcb", cmd_reload_bcb, 1);
    fastboot_publish("slot-count", "2");
    if (active_slot >= 0 && active_slot < 2)
        fastboot_publish("current-slot", active_slot ? "b" : "a");
#endif

#ifdef HAVE_DISPLAY
    // Registered last so it shadows the one common_early_init() put in.
    fastboot_register("oem kaeru-version", cmd_kaeru_version, 1);
#endif

#ifdef HAVE_FASTBOOT_INIT
    device_fastboot_init();
#endif
}

void board_early_init(void) {
    printf("Entering early init for %s\n", BOARD_NAME);

    uint32_t addr = 0;

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

#ifdef CONFIG_AMAZON_MT8163_AB
    // Make sure the BCB gets loaded on the first
    // call of load_bcb().
    gd.bcb_dirty = true;
#endif

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

#ifdef HAVE_DISPLAY
    // Silence everything LK draws while serving fastboot commands, it all
    // ends up on top of our banner.
    const uint32_t video_calls[] = { FB_VIDEO_CALL_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(video_calls); i++)
        NOP(video_calls[i], 2);

    // This one is a tail call, so it has to return instead of running into
    // the literals behind it.
    PATCH_MEM(FB_VIDEO_TAIL_CALL_ADDR, 0x4770, 0xBF00);
#endif

    // Redirect the call that prints "fastboot_init()\n" to our hook, so
    // we can register custom fastboot commands and other things.
    PATCH_CALL(FASTBOOT_INIT_PRINTF_CALL_ADDR, &fastboot_init_hook, TARGET_THUMB);

#ifdef BOOTIMG_CMDLINE_PRINT_CALL_ADDR
    // Stock LK always boots a 32-bit kernel. Hook the spot where it reads the
    // boot image command line so we can honor a 'bootopt=64...' request and
    // force the 64-bit kernel flag before the kernel is prepared.
    PATCH_CALL(BOOTIMG_CMDLINE_PRINT_CALL_ADDR, &bootimg_cmdline_hook,
               TARGET_THUMB);
#endif

#ifdef HAVE_DISPLAY
    // Get rid of the stock mode strings LK draws during boot. They are
    // confusing, tell the user nothing useful, and the fastboot one lands
    // right before our banner. Both call sites that print the fastboot one
    // share the string, so blanking it covers both.
    blank_string(SEARCH_STRING(" => FASTBOOT mode...\n"), "fastboot mode");
    blank_string(SEARCH_STRING(" => FACTORYRESET mode...\n"), "factory reset mode");
#endif

#ifdef LOAD_RECOVERY_HDR_CALL_ADDR
    // Load recovery from a dedicated partition instead of the stock one. LK
    // reads the header and the image in two passes, so hook both.
    PATCH_CALL(LOAD_RECOVERY_HDR_CALL_ADDR, &load_recovery_hdr_hook, TARGET_THUMB);
    PATCH_CALL(LOAD_RECOVERY_IMG_CALL_ADDR, &load_recovery_img_hook, TARGET_THUMB);
#endif

#ifdef CONFIG_AMAZON_MT8163_AB
    // Replace Amazon's BCB load function to work around a nasty
    // Preloader "feature" that bricks with the default BCB values.
    PATCH_CALL(GET_ACTIVE_SLOT_FUNC_CALLER_ADDR, &get_active_slot, TARGET_THUMB);

    // Hook get_boot_part() so we can load recovery from a dedicated
    // partition instead of dealing with SAR shenanigans.
    PATCH_CALL(GET_BOOT_PART_FUNC_CALLER_ADDR, &get_boot_part_hook, TARGET_THUMB);
#endif

#ifdef LK_IDME_REGISTER_FUNC_ADDR
    FORCE_RETURN(LK_IDME_REGISTER_FUNC_ADDR, 0);
#endif

#ifdef BOOT_MODE_SELECT_CALL_ADDR
    // Override LK's default boot mode handling.
    PATCH_CALL(BOOT_MODE_SELECT_CALL_ADDR, &real_boot_mode_select, TARGET_THUMB);
#endif

#ifdef FACTORY_RESET_CHECK_ADDR
    // Kill LK's factory reset key combo, it wipes userdata when held.
    FORCE_RETURN(FACTORY_RESET_CHECK_ADDR, 0);
#endif

#ifdef FASTBOOT_KEY_CHECK_ADDR
    // Same for LK's own fastboot key check, it overrides our selection.
    FORCE_RETURN(FASTBOOT_KEY_CHECK_ADDR, 0);
#endif

#ifdef LK_FASTBOOT_KEY_STORE_ADDR
    NOP(LK_FASTBOOT_KEY_STORE_ADDR, 1);
#endif

#ifdef DT_RECOVERY_VERSION_LOAD_ADDR
    // Make sure we always load the DTB with enabled USB.
    PATCH_MEM(DT_RECOVERY_VERSION_LOAD_ADDR, 0x2100);
#endif

#ifdef HAVE_EARLY_INIT
    device_early_init();
#endif
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

#ifdef KERNEL_CMDLINE_ADDR
    // Expose brom_cmd_dis to the OS as ro.boot.brom_cmd_dis.
    cmdline_append(brom_cmd_disabled() ? "androidboot.brom_cmd_dis=1"
                                       : "androidboot.brom_cmd_dis=0");
#endif

#ifdef HAVE_DISPLAY
    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && mode != BOOTMODE_FASTBOOT
        && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal
        // boot. Fastboot is left out since our own banner covers it.
        show_bootmode(mode);
    }
#endif

#ifdef HAVE_LATE_INIT
    device_late_init();
#endif
}
