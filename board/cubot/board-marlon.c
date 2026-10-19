//
// Author: kelexine <https://github.com/kelexine>
// Date: 2026-10-19
// Purpose: Board support, partition dump (fetch), and LK runtime hooks for CUBOT Marlon (P50 EEA / MT6765)
// SPDX-FileCopyrightText: 2026 kelexine <https://github.com/kelexine>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>

#define DUMP_CHUNK        0x100000 // 1 MB staging buffer chunk
#define USB_CHUNK         0xFC00   // 63 KB USB endpoint transfer limit
#define MAX_FETCH_SIZE    0x100000 // 1 MB per fetch transaction
#define OEM_READ_MAX_DUMP 0x400    // 1024 bytes max for interactive terminal hexdump
#define USB_TIMEOUT_MS    5000     // 5 seconds USB transfer timeout
#define PART_NAME_MAX     36       // Maximum GPT / partition name length

typedef long long off_t;
typedef long ssize_t;

// Stock LK USB write function: int usb_write(const void *buf, unsigned len, unsigned timeout)
static inline int usb_write(const void *buf, unsigned len) {
    return ((int (*)(const void *, unsigned, unsigned))(0x48022C20 | 1))(buf, len, USB_TIMEOUT_MS);
}

// Stock LK partition index resolver
static inline int partition_get_index(const char *name) {
    return ((int (*)(const char *))(0x480456CC | 1))(name);
}

// Stock LK partition size resolver
static inline uint64_t partition_get_size(int index) {
    return ((uint64_t (*)(int))(0x480452E4 | 1))(index);
}

// Stock LK partition read function
static inline ssize_t marlon_partition_read(const char *name, uint64_t offset, void *data, size_t size) {
    return ((ssize_t (*)(const char *, off_t, uint8_t *, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
        name, (off_t)offset, (uint8_t *)data, size);
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

static bool advance_partition_name(const char **partition) {
    if (!partition || !*partition) {
        return false;
    }
    while (**partition != '\0' && ISSPACE(**partition)) {
        (*partition)++;
    }
    return (**partition != '\0');
}

static void send_range(const char *part_name, void *buf, uint64_t off, uint32_t len) {
    char hdr[16];
    npf_snprintf(hdr, sizeof(hdr), "DATA%08x", len);

    if (usb_write(hdr, 12) < 0) {
        fastboot_fail("Failed to write DATA header");
        return;
    }

    uint32_t remaining = len;
    uint64_t current_off = off;

    while (remaining) {
        uint32_t n = remaining > DUMP_CHUNK ? DUMP_CHUNK : remaining;

        ssize_t rd = marlon_partition_read(part_name, current_off, buf, n);
        if (rd != (ssize_t)n) {
            printf("fetch: read of %s failed at 0x%llX (got %d)\n",
                   part_name, current_off, (int)rd);
            return;
        }

        if (!usb_write_all(buf, n)) {
            printf("fetch: write of %s failed at 0x%llX\n", part_name, current_off);
            return;
        }

        current_off += n;
        remaining -= n;
    }

    fastboot_okay("");
}

static void cmd_fetch(const char *arg, void *data, unsigned sz) {
    (void)sz;
    const char *p = arg;
    advance_partition_name(&p);

    char name[PART_NAME_MAX + 1];
    size_t i = 0;
    while (p[i] && p[i] != ':' && !ISSPACE(p[i]) && i < PART_NAME_MAX) {
        name[i] = p[i];
        i++;
    }
    name[i] = '\0';

    if (!data) {
        fastboot_fail("No download buffer to stage through");
        return;
    }

    int idx = partition_get_index(name);
    if (idx < 0) {
        fastboot_fail("Partition not found");
        return;
    }

    uint64_t total = partition_get_size(idx);
    if (!total) {
        fastboot_fail("Partition size is zero");
        return;
    }

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

    send_range(name, data, off, (uint32_t)len);
}

static void (*const stock_cmd_getvar)(const char *arg, void *data, unsigned sz) =
    (void (*)(const char *, void *, unsigned))(0x480241D9);

static void cmd_getvar_wrapper(const char *arg, void *data, unsigned sz) {
    const char *var = arg;
    advance_partition_name(&var);

    if (strcmp(var, "max-fetch-size") == 0) {
        fastboot_okay("0x100000");
        return;
    }

    if (strncmp(var, "partition-size:", 15) == 0) {
        const char *part_name = var + 15;
        int idx = partition_get_index(part_name);
        if (idx < 0) {
            fastboot_fail("Partition not found");
            return;
        }

        uint64_t total = partition_get_size(idx);
        char value[24];
        npf_snprintf(value, sizeof(value), "0x%llX", total);
        fastboot_okay(value);
        return;
    }

    if (strncmp(var, "partition-type:", 15) == 0) {
        const char *part_name = var + 15;
        int idx = partition_get_index(part_name);
        if (idx < 0) {
            fastboot_fail("Partition not found");
            return;
        }
        fastboot_okay("raw");
        return;
    }

    if (strncmp(var, "has-slot:", 9) == 0) {
        fastboot_okay("no");
        return;
    }

    stock_cmd_getvar(arg, data, sz);
}

static void render_hexdump(const char *title, uint64_t off, const uint8_t *data, uint32_t len) {
    char line[FASTBOOT_INFO_MAX + 1];
    npf_snprintf(line, sizeof(line), "%s [0x%llX - 0x%llX]:", title, off, off + len);
    fastboot_info(line);

    for (uint32_t o = 0; o < len; o += 16) {
        uint32_t n = (len - o) > 16 ? 16 : (len - o);
        size_t used = 0;
        used += npf_snprintf(line + used, sizeof(line) - used, "%08X: ", (unsigned)(off + o));
        for (uint32_t i = 0; i < n; i++) {
            used += npf_snprintf(line + used, sizeof(line) - used, "%02x ", data[o + i]);
        }
        for (uint32_t i = n; i < 16; i++) {
            used += npf_snprintf(line + used, sizeof(line) - used, "   ");
        }
        used += npf_snprintf(line + used, sizeof(line) - used, " |");
        for (uint32_t i = 0; i < n; i++) {
            uint8_t c = data[o + i];
            used += npf_snprintf(line + used, sizeof(line) - used, "%c",
                                 (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        npf_snprintf(line + used, sizeof(line) - used, "|");
        fastboot_info(line);
    }
}

static void cmd_oem_read(const char *arg, void *data, unsigned sz) {
    (void)data;
    (void)sz;

    char part_name[32];
    char tok_off[32];
    char tok_len[32];
    char line[FASTBOOT_INFO_MAX + 1];

    while (*arg == ' ') arg++;

    const char *p = next_token(arg, part_name, sizeof(part_name));
    if (!part_name[0]) {
        fastboot_fail("Usage: fastboot oem read <partition> [offset] [len]");
        return;
    }

    int idx = partition_get_index(part_name);
    if (idx < 0) {
        npf_snprintf(line, sizeof(line), "Partition '%s' not found", part_name);
        fastboot_fail(line);
        return;
    }

    uint64_t total = partition_get_size(idx);
    uint32_t mb = (uint32_t)(total / (1024 * 1024));

    p = next_token(p, tok_off, sizeof(tok_off));
    p = next_token(p, tok_len, sizeof(tok_len));

    if (!tok_off[0]) {
        npf_snprintf(line, sizeof(line), "Partition: %s (index %d)", part_name, idx);
        fastboot_info(line);
        npf_snprintf(line, sizeof(line), "Size: 0x%llX bytes (%u MB)", total, mb);
        fastboot_info(line);
        fastboot_info("To dump this partition to your PC, run:");
        npf_snprintf(line, sizeof(line), "  fastboot fetch %s %s.img", part_name, part_name);
        fastboot_info(line);
        fastboot_okay("");
        return;
    }

    uint64_t off = parse_hex64(tok_off, NULL);
    uint32_t len = 0x100;

    if (tok_len[0]) {
        len = (uint32_t)parse_hex64(tok_len, NULL);
    }

    if (len > OEM_READ_MAX_DUMP) {
        len = OEM_READ_MAX_DUMP;
    }

    if (off >= total) {
        fastboot_fail("Offset is beyond partition boundary");
        return;
    }

    if (off + len > total) {
        len = (uint32_t)(total - off);
    }

    uint8_t dump_buf[OEM_READ_MAX_DUMP];
    ssize_t rd = marlon_partition_read(part_name, off, dump_buf, len);
    if (rd != (ssize_t)len) {
        fastboot_fail("Failed to read from partition");
        return;
    }

    render_hexdump(part_name, off, dump_buf, len);
    fastboot_okay("");
}

// Set the LK boot mode. Arg: normal|meta|recovery|factory|fastboot.
static void cmd_bootmode_set(const char* arg, void* data, unsigned sz) {
    (void)data;
    (void)sz;

    while (*arg == ' ') arg++;

    bootmode_t mode;
    if (strncmp(arg, "normal", 6) == 0) {
        mode = BOOTMODE_NORMAL;
    } else if (strncmp(arg, "meta", 4) == 0) {
        mode = BOOTMODE_META;
    } else if (strncmp(arg, "recovery", 8) == 0) {
        mode = BOOTMODE_RECOVERY;
    } else if (strncmp(arg, "factory", 7) == 0) {
        mode = BOOTMODE_FACTORY;
    } else if (strncmp(arg, "fastboot", 8) == 0) {
        mode = BOOTMODE_FASTBOOT;
    } else {
        fastboot_fail("unknown mode; use: normal|meta|recovery|factory|fastboot");
        return;
    }

    set_bootmode(mode);
    show_bootmode(get_bootmode());

    fastboot_info("boot mode overridden");
    fastboot_okay("");
}

// Print device, SoC, and boot mode information.
static void cmd_marlon_info(const char* arg, void* data, unsigned sz) {
    (void)arg;
    (void)data;
    (void)sz;

    print_kaeru_info(printf);

    fastboot_info("device : CUBOT Marlon (P50 EEA / v956)");
    fastboot_info("soc    : MediaTek MT6765 (Helio P22/P35)");
    fastboot_info(bootmode2str(get_bootmode()));

    fastboot_okay("");
}

// List kaeru OEM fastboot commands.
static void cmd_list(const char* arg, void* data, unsigned sz) {
    (void)arg;
    (void)data;
    (void)sz;

    fastboot_info("kaeru OEM commands (CUBOT Marlon):");
    fastboot_info("  oem kaeru-version          -- print kaeru version");
    fastboot_info("  oem kaeru-info             -- device/SoC/bootmode summary");
    fastboot_info("  oem kaeru-list             -- this help text");
    fastboot_info("  oem help                   -- list stock LK fastboot commands");
    fastboot_info("  oem mem read <addr> [len]  -- hex dump memory (core)");
    fastboot_info("  oem mem write <addr> <val> -- write 32-bit word (core)");
    fastboot_info("  oem kaeru-bootmode <mode>  -- set boot mode");
    fastboot_info("       modes: normal | meta | recovery | factory | fastboot");
    fastboot_info("  oem bldr_spoof on|off|status -- spoof bootloader lock state");
    fastboot_info("  oem read <part> [off] [len]  -- query partition / hexdump");
    fastboot_info("  fastboot fetch <part> <file> -- binary partition dump to PC");
    fastboot_okay("");
}

// LK fetches the kernel cmdline buffer through a tiny getter immediately
// before committing it into the FDT /chosen "bootargs" property. The getter
// is resolved at runtime from the BL inside the commit-site pattern below.
static char* (*cmdline_buf_getter)(void);

static char* recovery_cmdline_hook(void) {
    handle_recovery_boot();
    return cmdline_buf_getter();
}

// Apply lock-state spoofing patches.
static void spoof_lock_state(void) {
    int spoofing = is_spoofing_enabled();
    fastboot_publish("is-spoofing", spoofing ? "1" : "0");

    if (!spoofing) {
        printf("kaeru: bootloader lock spoofing disabled\n");
        return;
    }

    printf("kaeru: bootloader lock spoofing enabled, applying patches\n");

    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xB1D0, 0xB510, 0x4604, 0xF7FF, 0xFFDD);
    if (addr) {
        printf("kaeru: patching seccfg_get_lock_state @ 0x%08X\n", addr);
        PATCH_MEM(addr + 6,
                  0x2304,  // movs r3, #4 (LKS_LOCK)
                  0x6023,  // str r3, [r4]
                  0x2000,  // movs r0, #0
                  0xBD10); // pop {r4, pc}
    } else {
        printf("kaeru: seccfg_get_lock_state pattern not found\n");
    }

    addr = SEARCH_PATTERN(LK_START, LK_END, 0xF003, 0xFD30, 0xF003, 0xFD28, 0x4605);
    if (addr) {
        uint32_t getter = DECODE_BL_TARGET(addr + 4);
        printf("kaeru: hooking cmdline commit @ 0x%08X (getter @ 0x%08X)\n",
               addr + 4, getter);
        cmdline_buf_getter = (char* (*)(void))(getter | 1);
        PATCH_CALL(addr + 4, (void*)recovery_cmdline_hook, TARGET_THUMB);
    } else {
        printf("kaeru: cmdline commit hook point not found\n");
    }
}

// Pattern-patch LK in RAM: bypass unlock/verify/download policy checks,
// silence bootloader warnings, and unrestrict the fastboot command processor.
void board_early_init(void) {
    printf("kaeru: early init - CUBOT Marlon (MT6765)\n");

    uint32_t addr = 0;

    // fastboot_unlock_verify: return 0 to bypass OEM unlock verification.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4FF0, 0xF5AD, 0x5DAC);
    if (addr) {
        printf("kaeru: patching fastboot_unlock_verify @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: fastboot_unlock_verify pattern not found\n");
    }

    // get_vfy_policy: return 0 to accept unsigned boot/recovery images.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF75, 0xF3C0);
    if (!addr) {
        addr = SEARCH_PATTERN(LK_START, LK_END, 0xB510, 0x4C00, 0x447C, 0x6824);
    }
    if (addr) {
        printf("kaeru: patching get_vfy_policy @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: get_vfy_policy pattern not found\n");
    }

    // get_dl_policy: return 0 to allow flashing regardless of lock state.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB508, 0xF7FF, 0xFF6F, 0xF000);
    if (addr) {
        printf("kaeru: patching get_dl_policy @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: get_dl_policy pattern not found (may be inlined)\n");
    }

    // orange_state_warning: skip the unlocked bootloader warning screen.
    addr = SEARCH_PATTERN(LK_START, LK_END,
                           0xB508, 0x4B0E, 0x447B, 0x681B, 0x681B, 0x2B02);
    if (addr) {
        printf("kaeru: patching orange_state_warning @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: orange_state_warning pattern not found\n");
    }

    // dm_verity_corruption_warning: skip the dm-verity warning screen.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x2802, 0xD000, 0x4770, 0xB538, 0x2432);
    if (addr) {
        printf("kaeru: patching dm_verity_corruption_warning @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: dm_verity_corruption_warning pattern not found\n");
    }

    // red_state_warning: skip the red-state warning and halt loop.
    addr = SEARCH_PATTERN(LK_START, LK_END,
                           0xB508, 0x4B0C, 0x447B, 0x681B, 0x681B, 0x2B03);
    if (!addr) {
        addr = SEARCH_PATTERN(LK_START, LK_END, 0x2803, 0xD001, 0x4770);
    }
    if (addr) {
        printf("kaeru: patching red_state_warning @ 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("kaeru: red_state_warning pattern not found\n");
    }

    // Fastboot command processor: NOP security gates and jump to command dispatch.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x4880, 0xB087, 0x4D5A);
    if (addr) {
        printf("kaeru: patching fastboot command processor @ 0x%08X\n", addr);
        NOP(addr + 0x15A, 2);
        NOP(addr + 0x166, 2);
        PATCH_MEM(addr + 0xF0, 0xE006);
    } else {
        printf("kaeru: fastboot command processor pattern not found\n");
    }

    // Env init done: printf call in platform_init right after env_init().
    addr = SEARCH_PATTERN(LK_START, LK_END, 0x4946, 0x3046, 0x4FF0, 0x000B, 0x7A44);
    if (addr) {
        printf("kaeru: hooking env init done @ 0x%08X\n", addr);
        PATCH_CALL(addr + 0xA, (void*)spoof_lock_state, TARGET_THUMB);
    } else {
        printf("kaeru: env init hook point not found\n");
    }

    // Patch USB QMU driver to disable ZLP on bulk IN endpoints.
    // MTK QMU sends a 0-length packet on transfers that are multiples of 512 bytes,
    // which causes the host Linux usbdevfs / fastboot client to read 0 bytes and
    // fail with "Device sent unknown status code: ".
    // At 0x48044C66: orr r3, r3, #0x80 (sets ZLP bit). We NOP it.
    addr = SEARCH_PATTERN(LK_START, LK_END, 0xB365, 0xF043, 0x0380, 0x4D20, 0x4631, 0x7023);
    if (addr) {
        printf("kaeru: patching QMU ZLP bit @ 0x%08X\n", addr + 2);
        NOP(addr + 2, 2);
    } else {
        printf("kaeru: QMU ZLP pattern not found\n");
    }

    printf("kaeru: early init complete\n");
}

// Register kaeru OEM fastboot commands and handlers.
void board_late_init(void) {
    printf("kaeru: late init - CUBOT Marlon (MT6762/MT6765)\n");

    fastboot_publish("max-fetch-size", "0x100000");

    fastboot_register("getvar:", cmd_getvar_wrapper, 1);
    fastboot_register("fetch:", cmd_fetch, 1);
    fastboot_register("oem read", cmd_oem_read, 1);
    fastboot_register("oem kaeru-info", cmd_marlon_info, 1);
    fastboot_register("oem kaeru-list", cmd_list, 1);
    fastboot_register("oem kaeru-bootmode", cmd_bootmode_set, 1);
    fastboot_register("oem bldr_spoof", cmd_spoof_bootloader_lock, 1);

    if (get_bootmode() != BOOTMODE_NORMAL) {
        show_bootmode(get_bootmode());
    }

    printf("kaeru: late init complete\n");
}
