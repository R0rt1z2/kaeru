//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include "include/mt8696-common.h"

static const struct {
    char name[12];
    uint32_t base;
    uint32_t size;
} mem_regions[] = {
    { "pstore",     0x59410000, 0xE0000 },
    { "console",    0x59410000, 0x40000 },
    { "pmsg",       0x59450000, 0x10000 },
    { "ramconsole", 0x59400000, 0x1000  },
};

enum mode_reason {
    MODE_REASON_NONE = 0,
    MODE_REASON_MISC = 1,
    MODE_REASON_FACTORY = 2,
    MODE_REASON_Y_CABLE = 3,
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
        case MODE_REASON_Y_CABLE:
            return "Y-cable";
        default:
            return NULL;
    }
}

// This is nasty.. but does the job fine.
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

    cmdline_replace(cl, "buildvariant=", "userdebug", "eng");
    cmdline_replace(cl, "buildvariant=", "user", "eng");

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
#ifdef CMDLINE_END_PTR_ADDR
    WRITE32(CMDLINE_END_PTR_ADDR, (uint32_t)(uintptr_t)w);
#endif

    return cl;
}

static void blank_string(char *s, const char *name) {
    if (!s) {
        printf("Could not find the %s string\n", name);
        return;
    }

    printf("Found the %s string at 0x%08X\n", name, (uint32_t)(uintptr_t)s);
    WRITE8(s, '\0');
}

static void cmd_kaeru_version(const char *arg, void *data, unsigned sz) {
    video_set_cursor(video_get_rows() / 12, 0);
    cmd_version(arg, data, sz);
}

static bool is_partition_protected(const char* partition) {
    // These partitions are critical, flashing them incorrectly can lead to a
    // hard brick. To prevent accidental damage, we mark them as protected and
    // block write access.
    if (strcasecmp(partition, "boot0") == 0 ||
        strcasecmp(partition, "preloader") == 0 ||
        strcasecmp(partition, "singlebootloader") == 0) {
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

bool mem_region_find(const char *name, uint32_t *base, uint32_t *size) {
    for (uint32_t i = 0; i < ARRAY_SIZE(mem_regions); i++) {
        if (strcmp(name, mem_regions[i].name) == 0) {
            *base = mem_regions[i].base;
            *size = mem_regions[i].size;
            return true;
        }
    }

    return false;
}

#ifdef KERNEL_64BIT_FLAG_ADDR
static int kernel_bits(const char *cmdline, uint32_t kernel) {
    int bits = cmdline_kernel_bits(cmdline);

    if (bits < 0)
        bits = READ32(kernel + 36) == 0x016F2818 ? 32 : 64;

    return bits;
}

static void force_kernel_bits(const char *cmdline, uint32_t kernel) {
    int bits = kernel_bits(cmdline, kernel);

    printf("Boot image requests a %d-bit kernel, forcing it\n", bits);
    WRITE32(KERNEL_64BIT_FLAG_ADDR, bits == 64);
}

static void boot_linux_hook(uint32_t kernel, uint32_t tags, const char *cmdline,
                            uint32_t mach, uint32_t ramdisk, uint32_t ramdisk_sz) {
    force_kernel_bits(cmdline, BOOTIMG_STAGE_BASE);

    ((void (*)(uint32_t, uint32_t, const char *, uint32_t, uint32_t, uint32_t))
            (BOOT_LINUX_FUNC_ADDR|1))(kernel, tags, cmdline, mach, ramdisk,
                                      ramdisk_sz);
}
#endif

static void cmd_boot(const char *arg, void *data, unsigned sz) {
    const boot_img_hdr *hdr = data;

    if (!data || sz < sizeof(*hdr)) {
        fastboot_fail("No boot image was downloaded");
        return;
    }

    if (memcmp(hdr->magic, BOOTIMG_MAGIC, BOOTIMG_MAGIC_SZ) != 0) {
        fastboot_fail("Not a boot image");
        return;
    }

    uint32_t page = hdr->page_size;
    if (!page || (page & (page - 1))) {
        fastboot_fail("Boot image has a bogus page size");
        return;
    }

    uint32_t kernel_pages = ((hdr->kernel_size + page - 1) / page) * page;
    uint32_t ramdisk_pages = ((hdr->ramdisk_size + page - 1) / page) * page;

    if ((uint64_t)page + kernel_pages + ramdisk_pages > sz) {
        fastboot_fail("Boot image is shorter than its header claims");
        return;
    }

#ifdef BOOT_HDR_PTR_ADDR
    const uint8_t *kernel = (const uint8_t *)data + page;

#ifdef KERNEL_64BIT_FLAG_ADDR
    if (kernel_bits((const char *)hdr->cmdline, (uint32_t)(uintptr_t)kernel) == 64
        && (hdr->kernel_addr & (KERNEL_64BIT_ALIGN - 1))) {
        fastboot_fail("A 64-bit kernel needs an aligned load address");
        return;
    }
#endif

    static boot_img_hdr boot_hdr;
    memcpy(&boot_hdr, data, BOOT_HDR_READ_SIZE);

    hdr = &boot_hdr;
    WRITE32(BOOT_HDR_PTR_ADDR, (uint32_t)(uintptr_t)&boot_hdr);

    // How many bytes LK copies to the load address, so it has to cover the
    // kernel it leaves packed.
    WRITE32(BOOTIMG_SIZE_ADDR, kernel_pages);

    // Only the storage path appends the image's own command line.
    cmdline_append((const char *)hdr->cmdline);

    printf("boot: %u byte kernel, %u byte ramdisk, staging at 0x%08X\n",
           hdr->kernel_size, hdr->ramdisk_size, BOOTIMG_STAGE_BASE);

    // Ramdisk first, since both moves overlap the download they read from.
    memmove((void *)hdr->ramdisk_addr, kernel + kernel_pages, hdr->ramdisk_size);
    memmove((void *)BOOTIMG_STAGE_BASE, kernel, kernel_pages);

#ifdef KERNEL_64BIT_FLAG_ADDR
    // Our boot_linux hook does not run on this path.
    force_kernel_bits(get_cmdline(), BOOTIMG_STAGE_BASE);
#endif
#else
    memcpy((void *)BOOT_HDR_ADDR, data, BOOT_HDR_READ_SIZE);
    WRITE32(BOOT_HDR_STATE1_ADDR, 1);
    WRITE32(BOOT_HDR_STATE2_ADDR, 0);
    bootimg_parse_bootopt((const char *)hdr->cmdline);

    uint32_t base = READ32(BOOTIMG_RELOC_FLAG_ADDR) ? BOOTIMG_STAGE_BASE
                                                    : hdr->kernel_addr;

    printf("boot: %u byte kernel, %u byte ramdisk, staging at 0x%08X\n",
           hdr->kernel_size, hdr->ramdisk_size, base);

    memmove((void *)base, (const uint8_t *)data + page,
            kernel_pages + ramdisk_pages);
    bootimg_set_base(base);
#endif

    fastboot_okay("");

    video_set_cursor(gd.banner_row, 0);
    video_printf(" => Booting downloaded image (%u bytes)...          \n", sz);

#ifdef BOOT_LINUX_MACH_TYPE
    boot_linux(hdr->kernel_addr, hdr->tags_addr, cmdline_hook(),
               hdr->ramdisk_addr, hdr->ramdisk_size);
#else
    boot_linux();
#endif
}

#ifdef LOG_STORE_HEADER_PTR_ADDR
static void cmd_oem_dmesg(const char *arg, void *data, unsigned sz) {
    char line[FASTBOOT_INFO_MAX + 1];

    uint32_t state = READ32(LOG_STORE_STATE_ADDR);
    if (state != 0x200) {
        npf_snprintf(line, sizeof(line), "Log store is not up, state 0x%X",
                     state);
        fastboot_fail(line);
        return;
    }

    uint32_t hdr = READ32(LOG_STORE_HEADER_PTR_ADDR);
    uint32_t base = hdr ? READ32(hdr + 0x14) + READ32(LOG_STORE_DRAM_BASE_ADDR) : 0;
    uint32_t len = hdr ? READ32(hdr + 0x18) : 0;

    if (!hdr || !base || !len) {
        fastboot_fail("Log store is up but has nothing in it");
        return;
    }

    npf_snprintf(line, sizeof(line), "log at 0x%08X, %u bytes", base, len);
    fastboot_info(line);

    const char *p = (const char *)(uintptr_t)base;
    uint32_t col = 0;

    for (uint32_t i = 0; i < len; i++) {
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

    fastboot_okay("");
}

FASTBOOT_CMD(dmesg, "oem dmesg", cmd_oem_dmesg, 1);
#endif

static void cmd_unlock_critical(const char *arg, void *data, unsigned sz) {
    gd.unlocked_critical = true;
    fastboot_okay("");
}

static void cmd_lock_critical(const char *arg, void *data, unsigned sz) {
    gd.unlocked_critical = false;
    fastboot_okay("");
}

static void cmd_reboot_wrapper(const char *arg, void *data, unsigned sz) {
    cmd_reboot_write_message(arg);

    if (!strcmp(arg, "-bootloader"))
        cmd_reboot_bootloader("", data, sz);
    else
        cmd_reboot("", data, sz);
}

static void fastboot_init_hook(const char *) {
    gd.banner_row = (int)READ32(VIDEO_ROW_ADDR);
    video_printf(" => HACKED FASTBOOT mode - R0rt1z2\n");
    fastboot_publish("boot-reason", modereason2str(gd.reason));

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("boot", cmd_boot, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);

    // Registered last so it shadows the one common_early_init() put in.
    fastboot_register("oem kaeru-version", cmd_kaeru_version, 1);

#ifdef HAVE_FASTBOOT_INIT
    device_fastboot_init();
#endif
}

static bool is_y_cable(void) {
    uint32_t barg = READ32(BOOT_ARG_PTR_ADDR);
    if (!barg)
        return false;

    return READ8(barg + BOOT_ARG_Y_CABLE_OFF) == 1;
}

static bool is_bootmenu_disabled(void) {
#ifdef CONFIG_ENVIRONMENT_SUPPORT
    const char *val = get_env(ENV_DISABLE_BOOTMENU);

    return val && !strcmp(val, "1");
#else
    return false;
#endif
}

static bool is_cold_boot(void) {
    uint32_t barg = READ32(BOOT_ARG_PTR_ADDR);
    if (!barg)
        return false;

    return READ32(barg + BOOT_ARG_BOOT_REASON_OFF) == BOOT_REASON_POWER_KEY;
}

static void seed_bootmenu_default(void) {
#ifdef CONFIG_ENVIRONMENT_SUPPORT
    if (!get_env(ENV_DISABLE_BOOTMENU))
        set_env(ENV_DISABLE_BOOTMENU, "0");
#endif
}

static void boot_mode_select(void) {
    // Act on the preloader's boot mode before anything else, forcing
    // fastboot on a factory boot and recovery on an ATE factory boot.
    bootmode_t bootmode = get_bootmode();
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
        return;
    }

    // Recovery writes this when the user picks the OS. The cable is usually
    // still plugged in by then, so without it we'd bounce straight back.
    if (take_boot_system_request()) {
        printf("misc asked for a system boot, ignoring OTG\n");
        return;
    }

    if (!is_y_cable())
        return;

    if (!is_cold_boot()) {
        printf("OTG detected on a warm boot, ignoring it\n");
        return;
    }

    if (is_bootmenu_disabled()) {
        printf("OTG detected, but the boot menu is disabled\n");
        return;
    }

    printf("OTG detected, booting into recovery\n");
    set_bootmode(BOOTMODE_RECOVERY);
    gd.reason = MODE_REASON_Y_CABLE;
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

#ifdef IMAGE_VERIFY_FUNC_ADDR
    // Skipping it left libtomcrypt uninitialised, so the check LK runs on the
    // scp image on the way to the kernel asserts out of rsa_import. Patch the
    // check itself, not its caller, which reads the anti-rollback version.
    FORCE_RETURN(IMAGE_VERIFY_FUNC_ADDR, 0);
#endif

#ifdef VERITY_CHECK_FUNC_ADDR
    // LK only lets an unlocked device off the AVB hook when IDME carries
    // the right fos_flags, and hands everyone else a red state and a reboot
    // five seconds later. Skip the check so we always land on orange, which
    // is what makes libavb tolerate verification errors.
    FORCE_RETURN(VERITY_CHECK_FUNC_ADDR, 1);
#endif

    // Get rid of the stock mode string LK draws during boot. It is
    // confusing, tells the user nothing useful, and lands right before
    // our banner.
    blank_string(SEARCH_STRING(" => FASTBOOT mode...\n"), "fastboot mode");

    // Disable dm-verity, we won't be needing it anymore :)
    PATCH_CALL(CMDLINE_GET_CALL_ADDR, &cmdline_hook, TARGET_THUMB);

#ifdef CMDLINE_STALE_APPEND_CALL_ADDR
    // While we are here, LK's last append before it jumps re-adds a stale
    // stack buffer. Nothing wants that on the command line.
    NOP(CMDLINE_STALE_APPEND_CALL_ADDR, 2);
#endif

#ifdef KERNEL_64BIT_FLAG_ADDR
    // Pick 32 or 64 bit from the kernel LK is about to start rather than
    // from what the preloader claimed.
    PATCH_CALL(BOOT_LINUX_CALL_ADDR, &boot_linux_hook, TARGET_THUMB);
#endif

    // Redirect the call that prints "fastboot_init()\n" to our hook, so
    // we can register custom fastboot commands and other things.
    PATCH_CALL(FASTBOOT_INIT_PRINTF_CALL_ADDR, &fastboot_init_hook, TARGET_THUMB);

    // Disable the built in flash and erase commands. Ours are registered
    // from the hook below, which runs before LK gets to register its own,
    // and the first match in the command list is the one that serves.
    NOP(FB_REGISTER_FLASH_ADDR, 2);
    NOP(FB_REGISTER_ERASE_ADDR, 2);

    // Same mechanic for reboot and reboot-bootloader, so our own reboot
    // handler is the one that gets called.
    NOP(FB_REGISTER_REBOOT_ADDR, 2);
    NOP(FB_REGISTER_REBOOT_BOOTLOADER_ADDR, 2);
#ifdef FB_REGISTER_REBOOT_FASTBOOT_ADDR
    NOP(FB_REGISTER_REBOOT_FASTBOOT_ADDR, 2);
#endif

    // Set up our getvar handler to make fastboot fetch work.
    NOP(FB_REGISTER_GETVAR_ADDR, 2);

#ifdef FB_REGISTER_DUMP_ADDR
    // LK somehow registers an invalid dump command, and nothing serves it
    // now, so left alone it would branch straight to zero.
    NOP(FB_REGISTER_DUMP_ADDR, 2);
#endif

    // LK's own 'oem idme' takes two tokens at most and answers with a fixed
    // string, so ours replaces it rather than sitting behind it.
    NOP(LK_IDME_REGISTER_CALL_ADDR, 2);

    // Drop 'oem relock', which is useless anyway. Some builds register it
    // twice, hence the list.
    const uint32_t relock_cmds[] = { FB_REGISTER_RELOCK_ADDRS };
    for (uint32_t i = 0; i < ARRAY_SIZE(relock_cmds); i++)
        NOP(relock_cmds[i], 2);

#ifdef HAVE_EARLY_INIT
    device_early_init();
#endif
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

    seed_bootmenu_default();
    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

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
