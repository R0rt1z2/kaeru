// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-or-later
/*
 * idmelib - library for manipulating amazon idme data
 * Copyright (c) 2026 Roger Ortiz <me@r0rt1z2.com>
 *
 * This work is dual-licensed under the terms of the 3-Clause BSD License
 * or the GNU General Public License (GPL) version 2.0 or later.
 * You may choose, at your discretion, which of the licenses to follow.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <arch/cache.h>
#include <lib/common.h>
#include <lib/debug.h>
#include <lib/idmelib.h>
#include <lib/string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// The names are spelled out in the structs rather than pointed at. A pointer
// in static data lands as an R_ARM_ABS32 in rodata, and the payload only ever
// relocates its GOT, so it would still hold its link time address when we ran.
#define IDME_FLAG_NAME_LEN 20

static const struct {
    unsigned long long bit;
    char name[IDME_FLAG_NAME_LEN];
} fos_flags_table[] = {
    { FOS_FLAGS_ADB_ON,           "ADB_ON"           },
    { FOS_FLAGS_ADB_ROOT,         "ADB_ROOT"         },
    { FOS_FLAGS_CONSOLE_ON,       "CONSOLE_ON"       },
    { FOS_FLAGS_RAMDUMP_ON,       "RAMDUMP_ON"       },
    { FOS_FLAGS_VERBOSITY_ON,     "VERBOSITY_ON"     },
    { FOS_FLAGS_ADB_AUTH_DISABLE, "ADB_AUTH_DISABLE" },
    { FOS_FLAGS_FORCE_DM_VERITY,  "FORCE_DM_VERITY"  },
    { FOS_FLAGS_DM_VERITY_OFF,    "DM_VERITY_OFF"    },
    { FOS_FLAGS_BOOT_DEXOPT,      "BOOT_DEXOPT"      },
};

static const char flags_var_names[IDME_FLAGS_MAX][IDME_MAX_NAME_LEN] = {
    [IDME_FLAGS_FOS] = "fos_flags",
    [IDME_FLAGS_DEV] = "dev_flags",
    [IDME_FLAGS_USR] = "usr_flags",
};

static const char bootmode_names[IDME_BOOTMODE_MAX][16] = {
    [IDME_BOOTMODE_NORMAL]       = "normal",
    [IDME_BOOTMODE_DIAG]         = "diag",
    [IDME_BOOTMODE_RECOVERY]     = "recovery",
    [IDME_BOOTMODE_EMERGENCY]    = "emergency",
    [IDME_BOOTMODE_POWERSAVE]    = "powersave",
    [IDME_BOOTMODE_FASTBOOT]     = "fastboot",
    [IDME_BOOTMODE_TRANSITION]   = "transition",
    [IDME_BOOTMODE_WSWDL]        = "wswdl",
    [IDME_BOOTMODE_STANDBY_LOGO] = "standby_logo",
};

// Look up an item by name.
struct idme_item *idmelib_get_item(struct idme *hdr, const char *name) {
    struct idme_item *item;
    uint32_t i;

    if (!hdr || !name || !idmelib_magic_valid(hdr))
        return NULL;

    item = idmelib_first_item(hdr);
    for (i = 0; i < hdr->items_num; i++) {
        if (strncmp(name, item->desc.name, IDME_MAX_NAME_LEN) == 0)
            return item;
        item = idmelib_item_next(item);
    }

    return NULL;
}

// Get an item's value as a null-terminated string.
int idmelib_get_var(struct idme *hdr, const char *name, char *buf, size_t len) {
    struct idme_item *item;
    size_t copy_len;
    uint32_t prev;

    if (!buf || len == 0)
        return -1;

    item = idmelib_get_item(hdr, name);
    if (!item)
        return -1;

    copy_len = MIN(item->desc.size, len - 1);

    prev = enable_unaligned();
    memcpy(buf, item->data, copy_len);
    restore_unaligned(prev);

    buf[copy_len] = '\0';

    return 0;
}

// Set an item's value from a string.
int idmelib_set_var(struct idme *hdr, const char *name, const char *value) {
    struct idme_item *item;
    size_t vlen;
    uint32_t prev;

    if (!value)
        return -1;

    item = idmelib_get_item(hdr, name);
    if (!item)
        return -1;

    vlen = strlen(value);

    prev = enable_unaligned();
    memset(item->data, 0, item->desc.size);
    memcpy(item->data, value, MIN(vlen, item->desc.size));
    restore_unaligned(prev);

    return 0;
}

// Get the IDME version as a null-terminated string.
int idmelib_get_version(struct idme *hdr, char *buf, size_t len) {
    size_t copy_len;
    uint32_t prev;

    if (!hdr || !buf || len == 0)
        return -1;

    copy_len = MIN((size_t)IDME_VERSION_LEN, len - 1);

    prev = enable_unaligned();
    memcpy(buf, hdr->version, copy_len);
    restore_unaligned(prev);

    buf[copy_len] = '\0';

    return 0;
}

// Set the IDME version from a string.
int idmelib_set_version(struct idme *hdr, const char *version) {
    size_t vlen;
    uint32_t prev;

    if (!hdr || !version)
        return -1;

    vlen = strlen(version);
    if (vlen > IDME_VERSION_LEN)
        return -1;

    prev = enable_unaligned();
    memset(hdr->version, 0, IDME_VERSION_LEN);
    memcpy(hdr->version, version, vlen);
    restore_unaligned(prev);

    return 0;
}

// Convert a permission bitmask to a rwx string.
void idmelib_permission_to_str(uint32_t perm, char *buf) {
    int i;

    memset(buf, '-', 9);
    buf[9] = '\0';

    for (i = 0; i < 3; i++) {
        uint32_t triplet = (perm >> ((2 - i) * 3)) & 0x7;
        if (triplet & 0x4) buf[i * 3 + 0] = 'r';
        if (triplet & 0x2) buf[i * 3 + 1] = 'w';
        if (triplet & 0x1) buf[i * 3 + 2] = 'x';
    }
}

// Check if a binary blob item contains any non-zero data.
bool idmelib_item_has_data(const struct idme_item *item) {
    uint32_t i;

    if (!item)
        return false;

    for (i = 0; i < item->desc.size; i++) {
        if (item->data[i] != 0)
            return true;
    }

    return false;
}

// Get the IDME variable name for a given flag type.
const char *idmelib_flags_var_name(enum idme_flag_type type) {
    if (type >= IDME_FLAGS_MAX)
        return NULL;
    return flags_var_names[type];
}

// Read a flags variable as a 64-bit integer.
int idmelib_flags_get(struct idme *hdr, enum idme_flag_type type,
                      unsigned long long *out) {
    char buf[IDME_MAX_NAME_LEN + 1] = {0};
    const char *var;

    if (!out)
        return -1;

    var = idmelib_flags_var_name(type);
    if (!var)
        return -1;

    if (idmelib_get_var(hdr, var, buf, sizeof(buf)) != 0)
        return -1;

    *out = parse_hex64(buf, NULL);

    return 0;
}

// Write a flags variable as a hex string.
int idmelib_flags_set(struct idme *hdr, enum idme_flag_type type,
                      unsigned long long value) {
    char buf[19] = {0};
    const char *var;

    var = idmelib_flags_var_name(type);
    if (!var)
        return -1;

    // IDME stores these bare, so skip the 0x hex64() puts on the front.
    hex64(buf, value);

    return idmelib_set_var(hdr, var, buf + 2);
}

// Check if specific flag bits are set.
bool idmelib_flags_test(struct idme *hdr, enum idme_flag_type type,
                        unsigned long long bits) {
    unsigned long long cur = 0;

    if (idmelib_flags_get(hdr, type, &cur) != 0)
        return false;

    return (cur & bits) == bits;
}

// Decode a flags bitmask to a readable string.
int idmelib_flags_to_str(unsigned long long flags, char *buf, size_t len) {
    size_t used = 0;
    size_t i;
    bool first = true;
    unsigned long long remaining;

    if (!buf || len == 0)
        return -1;

    buf[0] = '\0';

    if (flags == FOS_FLAGS_NONE)
        return npf_snprintf(buf, len, "NONE") >= (int)len ? -1 : 0;

    remaining = flags;

    for (i = 0; i < ARRAY_SIZE(fos_flags_table); i++) {
        if (!(flags & fos_flags_table[i].bit))
            continue;

        used += npf_snprintf(buf + used, len - used, "%s%s",
                             first ? "" : "|", fos_flags_table[i].name);
        first = false;
        remaining &= ~fos_flags_table[i].bit;

        if (used >= len)
            return -1;
    }

    if (remaining) {
        char tail[19];

        hex64(tail, remaining);
        used += npf_snprintf(buf + used, len - used, "%s%s",
                             first ? "" : "|", tail);
        if (used >= len)
            return -1;
    }

    return 0;
}

// Resolve a flag name to its bit value (case-insensitive).
int idmelib_flags_parse_name(const char *name, unsigned long long *out) {
    size_t i;

    if (!name || !out)
        return -1;

    for (i = 0; i < ARRAY_SIZE(fos_flags_table); i++) {
        if (strcasecmp(name, fos_flags_table[i].name) == 0) {
            *out = fos_flags_table[i].bit;
            return 0;
        }
    }

    return -1;
}

// Get the bootmode as an integer.
int idmelib_get_bootmode(struct idme *hdr) {
    char buf[8] = {0};

    if (idmelib_get_var(hdr, "bootmode", buf, sizeof(buf)) != 0)
        return -1;

    return (int)strtol(buf, NULL, 10);
}

// Convert a bootmode value to a human-readable string.
const char *idmelib_bootmode_to_str(int mode) {
    if (mode < 1 || mode >= IDME_BOOTMODE_MAX)
        return "unknown";
    return bootmode_names[mode];
}

// Get the bootcount as an unsigned integer.
int idmelib_get_bootcount(struct idme *hdr, unsigned int *out) {
    char buf[12] = {0};

    if (!out)
        return -1;

    if (idmelib_get_var(hdr, "bootcount", buf, sizeof(buf)) != 0)
        return -1;

    *out = (unsigned int)strtoul(buf, NULL, 10);

    return 0;
}

// Parse a substring of length 'n' at offset 'off' as a hex integer.
static unsigned int parse_hex_substr(const char *str, size_t off, size_t n) {
    char tmp[8] = {0};
    size_t slen = strlen(str);
    uint32_t prev;

    if (off + n > slen || n >= sizeof(tmp))
        return 0;

    prev = enable_unaligned();
    memcpy(tmp, str + off, n);
    restore_unaligned(prev);

    return (unsigned int)strtoul(tmp, NULL, 16);
}

// Decode the board_id variable into its component fields.
int idmelib_get_board_info(struct idme *hdr, struct idme_board_info *info) {
    if (!info)
        return -1;

    memset(info, 0, sizeof(*info));

    if (idmelib_get_var(hdr, "board_id", info->raw, sizeof(info->raw)) != 0)
        return -1;

    if (strlen(info->raw) < 9)
        return -1;

    /* [0..3] board type, [7..8] board rev, [5] WAN flag */
    info->board_type = parse_hex_substr(info->raw, 0, 4);
    info->board_rev = parse_hex_substr(info->raw, 7, 2);
    info->has_wan = (info->raw[0] == '0' && info->raw[5] == '1');

    return 0;
}
