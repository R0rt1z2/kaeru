//
// SPDX-FileCopyrightText: 2026 R0rt1z2 <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>
#include <lib/mt_part.h>

#define BOARD_NAME "Fire TV Stick 4K / 4K Max / 4K Plus (2nd Gen)"

#define DUMP_CHUNK 0x100000
#define USB_CHUNK  0xFC00

#define MEM_READ_DEFAULT 0x100
#define MEM_READ_MAX     0x400

#define MAX_FETCH_SIZE 0x100000

#define BOOT_ARG_PTR             0x4CEAFFB4
#define BOOT_ARG_USB_EXTCONN_OFF 0x180
#define BOOT_ARG_Y_CABLE_OFF     0x181

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

static inline int usb_write(const void *buf, unsigned len) {
    return ((int (*)(const void *, unsigned))(0x4CE27174 | 1))(buf, len);
}

static inline void cmdline_append(const char *arg) {
    ((void (*)(const char *))(0x4CE28CD8 | 1))(arg);
}

static inline char *get_cmdline(void) {
    return ((char *(*)(void))(0x4CE28CCC | 1))();
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
    WRITE32(0x4CE71380, (uint32_t)(uintptr_t)w);

    return cl;
}

static inline void bootimg_set_base(uint32_t base) {
    ((void (*)(uint32_t))(0x4CE33150 | 1))(base);
}

static inline void bootimg_parse_bootopt(const char *cmdline) {
    ((void (*)(const char *))(0x4CE331A0 | 1))(cmdline);
}

static inline void boot_linux(void) {
    ((void (*)(void))(0x4CE26928 | 1))();
}

static inline void cmd_getvar(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4CE27A2C | 1))(arg, data, sz);
}

static inline void cmd_flash(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4CE2AA84 | 1))(arg, data, sz);
}

static inline void cmd_erase(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4CE2ABD8 | 1))(arg, data, sz);
}

static inline void cmd_reboot(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4CE27AB4 | 1))(arg, data, sz);
}

static inline void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz) {
    ((void (*)(const char *, void *, unsigned))(0x4CE27ACC | 1))(arg, data, sz);
}

static inline void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x4CE2B544 | 1))(row, col);
}

static inline int video_get_rows(void) {
    return ((int (*)(void))(0x4CE2B580 | 1))();
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
    npf_snprintf(hdr, sizeof(hdr), "DATA%08x", len);

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

static void send_mem(uint32_t base, uint32_t len, void *stage) {
    if (!stage) {
        fastboot_fail("No download buffer to stage through");
        return;
    }

    char hdr[16];
    npf_snprintf(hdr, sizeof(hdr), "DATA%08x", len);

    if (usb_write(hdr, 12) < 0) {
        return;
    }

    for (uint32_t off = 0; off < len; ) {
        uint32_t n = (len - off) > DUMP_CHUNK ? DUMP_CHUNK : (len - off);

        memcpy(stage, (const void *)(uintptr_t)(base + off), n);

        if (!usb_write_all(stage, n)) {
            printf("dump: write of 0x%08X failed at 0x%08X\n", base, off);
            return;
        }

        off += n;
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

    uint32_t mem_base;
    uint32_t mem_size;
    bool is_mem = mem_region_find(name, &mem_base, &mem_size);

    const struct part_info *part = NULL;
    if (!is_mem) {
        part = storage_part_find(name);
        if (!part) {
            fastboot_fail("Partition not found");
            return;
        }
    }

    uint64_t total = is_mem ? mem_size
                            : (uint64_t)part->size_blocks * BLOCK_SIZE;
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

    if (is_mem) {
        send_mem(mem_base + (uint32_t)off, (uint32_t)len, data);
        return;
    }

    send_range(part, data, off, (uint32_t)len);
}

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

    // 0x67C, not a whole page: that is exactly what LK reads into this
    // buffer, and the word right past it is a global AVB writes.
    memcpy((void *)0x4CEB3FB8, data, 0x67C);
    WRITE32(0x4CEB3D50, 1);
    WRITE32(0x4CEB3DB0, 0);
    bootimg_parse_bootopt((const char *)hdr->cmdline);

    uint32_t base = READ32(0x4CEAEF3C) ? 0x60000000 : hdr->kernel_addr;

    printf("boot: %u byte kernel, %u byte ramdisk, staging at 0x%08X\n",
           hdr->kernel_size, hdr->ramdisk_size, base);

    memcpy((void *)base, (const uint8_t *)data + page,
           kernel_pages + ramdisk_pages);
    bootimg_set_base(base);

    fastboot_okay("");

    video_set_cursor(gd.banner_row, 0);
    video_printf(" => Booting downloaded image (%u bytes)...          \n", sz);

    boot_linux();
}

static void cmd_getvar_wrapper(const char *arg, void *data, unsigned sz) {
    const char *var = arg;
    advance_partition_name(&var);

    if (strcmp(var, "max-fetch-size") == 0) {
        fastboot_okay("0x100000");
        return;
    }

    if (strncmp(var, "partition-size:", 15) == 0) {
        char value[19];
        uint32_t mem_base;
        uint32_t mem_size;

        if (mem_region_find(var + 15, &mem_base, &mem_size)) {
            hex64(value, mem_size);
            fastboot_okay(value);
            return;
        }

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

static void cmd_oem_partitions(const char *arg, void *data, unsigned sz) {
    int count = storage_part_count();
    if (!count) {
        fastboot_fail("Partition table is not available");
        return;
    }

    char line[FASTBOOT_INFO_MAX + 1];

    fastboot_info("name                    start     blocks");
    for (int i = 0; i < count; i++) {
        const struct part_info *part = storage_part_get(i);
        if (!part) {
            break;
        }

        npf_snprintf(line, sizeof(line), "%-20s %9u %10u", part->name,
                     part->start_block, part->size_blocks);
        fastboot_info(line);
    }

    fastboot_okay("");
}

static void cmd_oem_dmesg(const char *arg, void *data, unsigned sz) {
    char line[FASTBOOT_INFO_MAX + 1];

    uint32_t state = READ32(0x4CE9D114);
    if (state != 0x200) {
        npf_snprintf(line, sizeof(line), "Log store is not up, state 0x%X",
                     state);
        fastboot_fail(line);
        return;
    }

    uint32_t hdr = READ32(0x4CEAEF50);
    uint32_t base = hdr ? READ32(hdr + 0x14) + READ32(0x4CEAEF4C) : 0;
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
    gd.banner_row = (int)READ32(0x4CEAEAF8);
    video_printf(" => HACKED FASTBOOT mode - R0rt1z2\n");
    fastboot_publish("boot-reason", modereason2str(gd.reason));

    // Register our custom command(s).
    fastboot_register("flash:", cmd_flash_wrapper, 1);
    fastboot_register("erase:", cmd_erase_wrapper, 1);
    fastboot_register("reboot", cmd_reboot_wrapper, 1);
    fastboot_register("fetch:", cmd_fetch, 1);
    fastboot_register("boot", cmd_boot, 1);
    fastboot_register("getvar:", cmd_getvar_wrapper, 1);
    fastboot_register("oem partitions", cmd_oem_partitions, 1);
    fastboot_register("oem dmesg", cmd_oem_dmesg, 1);
    fastboot_register("flashing unlock_critical", cmd_unlock_critical, 1);
    fastboot_register("flashing lock_critical", cmd_lock_critical, 1);

    // Registered last so it shadows the one common_early_init() put in.
    fastboot_register("oem kaeru-version", cmd_kaeru_version, 1);
}

static bool is_y_cable(void) {
    uint32_t barg = READ32(BOOT_ARG_PTR);
    if (!barg)
        return false;

    return READ8(barg + BOOT_ARG_Y_CABLE_OFF) == 1;
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

    if (is_y_cable()) {
        printf("OTG detected, booting into recovery\n");
        set_bootmode(BOOTMODE_RECOVERY);
        gd.reason = MODE_REASON_Y_CABLE;
    }
}

void board_early_init(void) {
    printf("Entering early init for %s\n", BOARD_NAME);

    // Initialise global data structure.
    memset(&gd, 0, sizeof(gd));

    // Amazon decides whether the device is unlocked by validating the unlock
    // code stored in IDME. Every gate in LK, from boot to the fastboot command
    // handlers, goes through this one predicate, so forcing it to report an
    // unlocked device is enough to get unrestricted fastboot access.
    FORCE_RETURN(0x4CE01484, 1);

    // LK only lets an unlocked device off the AVB hook when IDME carries
    // the right fos_flags, and hands everyone else a red state and a reboot
    // five seconds later. Skip the check so we always land on orange, which
    // is what makes libavb tolerate verification errors.
    FORCE_RETURN(0x4CE24F44, 1);

    // Get rid of the stock mode string LK draws during boot. It is
    // confusing, tells the user nothing useful, and lands right before
    // our banner.
    blank_string(SEARCH_STRING(" => FASTBOOT mode...\n"), "fastboot mode");

    // Disable dm-verity, we won't be needing it anymore :)
    PATCH_CALL(0x4CE2658E, &cmdline_hook, TARGET_THUMB);

    // While we are here, LK's last append before it jumps re-adds a stale
    // stack buffer. Nothing wants that on the command line.
    NOP(0x4CE26B1E, 2);

    // Redirect the call that prints "fastboot_init()\n" to our hook, so
    // we can register custom fastboot commands and other things.
    PATCH_CALL(0x4CE274D8, &fastboot_init_hook, TARGET_THUMB);

    // Disable the built in flash and erase commands. Ours are registered
    // from the hook below, which runs before LK gets to register its own,
    // and the first match in the command list is the one that serves.
    NOP(0x4CE27566, 2);
    NOP(0x4CE2757A, 2);

    // Same mechanic for reboot, reboot-bootloader and reboot-fastboot, so
    // our own reboot handler is the one that gets called.
    NOP(0x4CE275A0, 2);
    NOP(0x4CE275B4, 2);
    NOP(0x4CE275C6, 2);

    // Set up our getvar handler to make fastboot fetch work.
    NOP(0x4CE27522, 2);

    // LK somehow registers an invalid dump command, and nothing serves it
    // now, so left alone it would branch straight to zero.
    NOP(0x4CE276B4, 2);

    // LK's own 'oem idme' takes two tokens at most and answers with a fixed
    // string, so ours replaces it rather than sitting behind it.
    NOP(0x4CE27694, 2);

    // Drop 'oem relock', which LK registers twice and it's useless anyway.
    NOP(0x4CE27680, 2);
    NOP(0x4CE276A0, 2);
}

void board_late_init(void) {
    printf("Entering late init for %s\n", BOARD_NAME);

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
