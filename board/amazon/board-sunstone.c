//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define DUMP_CHUNK     0x100000
#define USB_CHUNK      0xFC00
#define MAX_FETCH_SIZE 0x100000

#define KEY_VOLUME_UP 0

#define BOOT_ARG          0x562AC990
#define BOOT_ARG_MAGIC    0x504C504C
#define BOOT_ARG_MODE_OFF 0x4

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

static inline void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x56035114 | 1))(row, col);
}

static inline void mdelay(unsigned ms) {
    ((void (*)(unsigned))(0x5604AD30 | 1))(ms);
}

static inline void mt_power_off(void) {
    ((void (*)(void))(0x56022868 | 1))();
}

static inline int video_get_rows(void) {
    return ((int (*)(void))(0x5603514C | 1))();
}

static inline int usb_write(const void *buf, unsigned len) {
    return ((int (*)(const void *, unsigned))(0x5602A000 | 1))(buf, len);
}

static inline void cmdline_append(const char *arg) {
    ((void (*)(const char *))(0x5602C038 | 1))(arg);
}

static inline char *get_cmdline(void) {
    return ((char *(*)(void))(0x5602C02C | 1))();
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
    WRITE32(0x560DEFB8, (uint32_t)(uintptr_t)w);

    return cl;
}

static inline void cmd_flash(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x56031F3C | 1))(arg, data, sz);
}

static inline void cmd_erase(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x56031DB8 | 1))(arg, data, sz);
}

static inline void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x5602B858 | 1))(arg, data, sz);
}

static inline void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x5602B87C | 1))(arg, data, sz);
}

static inline void cmd_getvar(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x5602B7A0 | 1))(arg, data, sz);
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
    const char *part = arg;
    advance_partition_name(&part);

    if (is_partition_protected(part)) {
        critical_op_fail("You are attempting to flash to a critical partition.");
        return;
    }

    cmd_flash(arg, data, sz);
}

static void cmd_erase_wrapper(const char *arg, void *data, unsigned sz) {
    const char *part = arg;
    advance_partition_name(&part);

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

static bool usb_write_all(const uint8_t *buf, uint32_t len) {
    while (len) {
        uint32_t n = len > USB_CHUNK ? USB_CHUNK : len;

        if (usb_write(buf, n) < 0) {
            return false;
        }

        buf += n;
        len -= n;
    }

    return true;
}

static void send_range(const struct part_info *part, void *buf, uint64_t off,
                       uint32_t len) {
    char hdr[16];
    npf_snprintf(hdr, sizeof(hdr), "DATA%08x", (unsigned)len);

    if (usb_write(hdr, 12) < 0) {
        return;
    }

    while (len) {
        uint32_t n = len > DUMP_CHUNK ? DUMP_CHUNK : len;

        if (storage_part_read(part, buf, off, n) != (ssize_t)n) {
            printf("dump: read of %s failed at 0x%08X\n", part->name,
                   (unsigned)off);
            return;
        }

        if (!usb_write_all(buf, n)) {
            printf("dump: write of %s failed at 0x%08X\n", part->name,
                   (unsigned)off);
            return;
        }

        off += n;
        len -= n;
    }

    fastboot_okay("");
}

static void cmd_fetch(const char *arg, void *data, unsigned sz) {
    const char *p = arg;
    advance_partition_name(&p);

    char name[PART_NAME_MAX + 1];
    size_t i = 0;
    while (p[i] && p[i] != ':' && i < PART_NAME_MAX) {
        name[i] = p[i];
        i++;
    }
    name[i] = '\0';

    if (!data) {
        fastboot_fail("No download buffer to stage through");
        return;
    }

    const struct part_info *part = storage_part_find(name);
    if (!part) {
        fastboot_fail("Partition not found");
        return;
    }

    uint64_t total = (uint64_t)part->size_blocks * BLOCK_SIZE;
    uint64_t off = 0;
    uint64_t len = total;

    if (p[i] == ':') {
        const char *end;
        off = parse_hex64(p + i + 1, &end);
        len = (*end == ':') ? parse_hex64(end + 1, NULL) : total - off;
    }

    if (off > total || len > total - off) {
        fastboot_fail("Requested range is outside the partition");
        return;
    }

    if (len > MAX_FETCH_SIZE) {
        fastboot_fail("Requested more than max-fetch-size");
        return;
    }

    send_range(part, data, off, (uint32_t)len);
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
    if (READ32(0x7D2C0000) != 0x4842444D) {
        fastboot_fail("Log buffers are not set up");
        return;
    }

    const char *what = arg;
    advance_partition_name(&what);

    bool lk = strcmp(what, "pl") != 0;
    bool pl = strcmp(what, "pl") == 0 || strcmp(what, "all") == 0;

    if (pl) {
        dump_log(0x7D2C0008, 0x1BFF4);
    }

    if (lk) {
        dump_log(0x7D2DC000, 0x1BFFC);
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

static void cmd_getvar_wrapper(const char *arg, void *data, unsigned sz) {
    const char *var = arg;
    advance_partition_name(&var);

    if (strcmp(var, "max-fetch-size") == 0) {
        fastboot_okay("0x100000");
        return;
    }

    if (strncmp(var, "partition-size:", 15) == 0) {
        char value[19];

        const struct part_info *part = storage_part_find(var + 15);
        if (!part) {
            fastboot_fail("Partition not found");
            return;
        }

        hex64(value, (uint64_t)part->size_blocks * BLOCK_SIZE);
        fastboot_okay(value);
        return;
    }

    cmd_getvar(arg, data, sz);
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
    int rows = video_get_rows();
    int row = rows > 24 ? (rows - 24) / 2 : 0;

    video_set_cursor(row, 0);
    cmd_version(arg, data, sz);
}

static void memory_layout_hook(void *info) {
    // LK's ramdisk window is 16 MiB, right below LK itself, so a larger
    // ramdisk lands on the running bootloader... LK takes any window
    // asked for at "0x80000000 - size", so ask for 64 MiB at 0x7C000000 :)
    WRITE32(0x561D06E4, 0x4000000);
    WRITE32((uint32_t)(uintptr_t)info + 0x14, 0x7C000000);

    ((void (*)(void *))(0x56027894 | 1))(info);

    printf("ramdisk window: 0x%08X + 0x%X\n", (unsigned)READ32(0x561D06E0),
           (unsigned)READ32(0x561D06E4));
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
    gd.banner_row = (int)READ32(0x561DCE34);

    // The console is drawn rotated here, so no column pulls this flush
    // left. Leave it where LK had it.
    video_set_cursor(gd.banner_row, 0);
    video_printf(" => HACKED FASTBOOT mode - R0rt1z2\n");
    fastboot_publish("boot-reason", modereason2str(gd.reason));

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("fetch:", cmd_fetch, 1);
    fastboot_register("getvar:", cmd_getvar_wrapper, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);

    // Registered last so it shadows the one common_early_init() put in.
    fastboot_register("oem kaeru-version", cmd_kaeru_version, 1);
}

void board_early_init(void) {
    printf("Entering early init for Fire Max 11 (2023)\n");

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Amazon decides whether the device is unlocked by validating the unlock
    // code stored in IDME. Every gate in LK, from boot to the fastboot command
    // handlers, goes through this one predicate, so forcing it to report an
    // unlocked device is enough to get unrestricted fastboot access.
    FORCE_RETURN(0x56000F60, 1);

    // LK only lets an unlocked device off the AVB hook when IDME carries
    // the right fos_flags, and hands everyone else a red state and a reboot
    // five seconds later. Skip the check so we always land on orange, which
    // is what makes libavb tolerate verification errors.
    FORCE_RETURN(0x5600D324, 1);

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
    NOP(0x56031228, 2);
    NOP(0x560321BA, 2);

    // A crash leaves the watchdog to reset the board, and the boot after
    // that runs LK's crash dumper, which spends the better part of a second
    // pushing everything into expdb and then resets us a second time...
    FORCE_RETURN(0x56042D8C, 0);

    // LK appends androidboot.selinux=enforcing and enforcing=1 unless IDME
    // dev_flags asks otherwise. Drop the call and leave the cmdline alone.
    NOP(0x5602980C, 2);

    // Disable dm-verity, we won't be needing it anymore :)
    PATCH_CALL(0x56028FF2, &cmdline_hook, TARGET_THUMB);

    // Drop the store of LK's own window size, and the two checks that only
    // pass for the stock base and size, then hand the layout function ours.
    NOP(0x5602791E, 1);
    PATCH_MEM(0x56027A30, 0x4280);
    PATCH_MEM(0x56027AA6, 0x42B6, 0xBF00);
    PATCH_CALL(0x560417AC, &memory_layout_hook, TARGET_THUMB);

    // Redirect the call that prints "fastboot_init()\n" to our hook, so
    // we can register custom fastboot commands and other things.
    PATCH_CALL(0x5602A7F6, &fastboot_init_hook, TARGET_THUMB);

    // Disable the built in flash and erase commands. Ours are registered
    // from the hook below, which runs before LK gets to register its own,
    // and the first match in the command list is the one that serves.
    NOP(0x5602A8EE, 2);
    NOP(0x5602A906, 2);

    // Same mechanic for reboot, reboot-bootloader, reboot-recovery and
    // reboot-fastboot, so our own reboot handler is the one that serves
    // the whole prefix and every target goes through the BCB.
    NOP(0x5602A94C, 2);
    NOP(0x5602A964, 2);
    NOP(0x5602A97E, 2);
    NOP(0x5602A998, 2);

    // Set up our getvar handler to make fastboot fetch work.
    NOP(0x5602A854, 2);

    // Get rid of the stock fastboot commands used to read logs. We have
    // our own that are more useful anyway.
    NOP(0x5602AAD2, 2);
    NOP(0x5602ABA2, 2);

    // LK registers its own 'oem idme' through a small wrapper that tail
    // branches into fastboot_register. Kill the call to that wrapper so
    // our idmelib backed handler is the one that serves.
    NOP(0x5602AB46, 2);
}

static bootmode_t get_preloader_bootmode(void) {
    if (READ32(BOOT_ARG) != BOOT_ARG_MAGIC) {
        printf("Preloader boot args are not set up, asking LK instead\n");
        return get_bootmode();
    }

    return (bootmode_t)READ32(BOOT_ARG + BOOT_ARG_MODE_OFF);
}

static void boot_mode_select(void) {
    // A held key wins over everything else, so there is always a way into
    // recovery whatever the preloader or the BCB asked for.
    if (mtk_detect_key(KEY_VOLUME_UP)) {
        set_bootmode(BOOTMODE_RECOVERY);
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
    printf("Entering late init for Fire Max 11 (2023)\n");

    boot_mode_select();
    printf("Boot mode reason: %s\n", modereason2str(gd.reason));

    bootmode_t mode = get_bootmode();
    if (mode != BOOTMODE_NORMAL && mode != BOOTMODE_FASTBOOT
        && !is_unknown_mode(mode)) {
        // Show the current boot mode on screen when not performing a normal
        // boot.
        show_bootmode(mode);
    }
}