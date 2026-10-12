//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>
#include <lib/mt_part.h>
#include <lib/storage.h>
#include <lib/string.h>

#define MAX_FETCH_SIZE 0x100000
#define USB_CHUNK      0xFC00

struct fetch_src {
    const struct part_info* part;
    uint32_t base;
    uint64_t size;
};

static inline int usb_write(const void* buf, unsigned len) {
    return ((int (*)(const void*, unsigned))(CONFIG_FASTBOOT_USB_WRITE_ADDRESS | 1))(buf, len);
}

static inline void lk_getvar(const char* arg, void* data, unsigned sz) {
    ((void (*)(const char*, void*, unsigned))(CONFIG_FASTBOOT_GETVAR_ADDRESS | 1))(arg, data, sz);
}

static const char* skip_spaces(const char* s) {
    while (*s && ISSPACE(*s)) {
        s++;
    }

    return s;
}

static bool usb_write_all(const uint8_t* buf, uint32_t len) {
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

static bool fetch_src_find(const char* name, struct fetch_src* src) {
    uint32_t base, size;

    if (mem_region_find && mem_region_find(name, &base, &size)) {
        src->part = NULL;
        src->base = base;
        src->size = size;
        return true;
    }

    src->part = storage_part_find(name);
    if (!src->part) {
        return false;
    }

    src->base = 0;
    src->size = (uint64_t)src->part->size_blocks * BLOCK_SIZE;
    return true;
}

static void cmd_fetch(const char* arg, void* data, unsigned sz) {
    const char* p = skip_spaces(arg);
    char name[PART_NAME_MAX + 1];
    struct fetch_src src;
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

    if (!fetch_src_find(name, &src)) {
        fastboot_fail("Partition not found");
        return;
    }

    uint64_t off = 0;
    uint64_t len = src.size;

    if (p[i] == ':') {
        const char* end;
        off = parse_hex64(p + i + 1, &end);
        len = (*end == ':') ? parse_hex64(end + 1, NULL) : src.size - off;
    }

    if (off > src.size || len > src.size - off) {
        fastboot_fail("Requested range is outside the partition");
        return;
    }

    if (len > MAX_FETCH_SIZE) {
        fastboot_fail("Requested more than max-fetch-size");
        return;
    }

    if (src.part) {
        if (len && storage_part_read(src.part, data, off, len) != (ssize_t)len) {
            fastboot_fail("Failed to read the partition");
            return;
        }
    } else {
        memcpy(data, (const void*)(uintptr_t)(src.base + (uint32_t)off), len);
    }

    char hdr[16];
    npf_snprintf(hdr, sizeof(hdr), "DATA%08x", (unsigned)len);

    if (usb_write(hdr, 12) < 0 || !usb_write_all(data, (uint32_t)len)) {
        printf("fetch: sending %s back failed at 0x%08X\n", name, (unsigned)off);
        return;
    }

    fastboot_okay("");
}

static void cmd_getvar(const char* arg, void* data, unsigned sz) {
    const char* var = skip_spaces(arg);
    char value[19];

    if (strcmp(var, "max-fetch-size") == 0) {
        hex64(value, MAX_FETCH_SIZE);
        fastboot_okay(value);
        return;
    }

    if (strncmp(var, "partition-size:", 15) == 0) {
        struct fetch_src src;

        if (fetch_src_find(var + 15, &src)) {
            hex64(value, src.size);
            fastboot_okay(value);
            return;
        }
    }

    lk_getvar(arg, data, sz);
}

FASTBOOT_CMD(fetch, "fetch:", cmd_fetch, 1);
FASTBOOT_CMD(getvar, "getvar:", cmd_getvar, 1);
