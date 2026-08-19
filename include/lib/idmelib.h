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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/string.h>

#define IDME_MAGIC          "beefdeed"
#define IDME_MAGIC_SIZE     8
#define IDME_VERSION_LEN    4
#define IDME_MAX_NAME_LEN   16
#define IDME_ALIGN_SIZE     4
#define IDME_MMC_BLOCK_SIZE 512

#define IDME_NUM_BLOCKS     30
#define IDME_SIZE           (IDME_MMC_BLOCK_SIZE * IDME_NUM_BLOCKS)

// Longest name the flag decoder can emit, plus the separators between them.
#define IDME_FLAGS_STR_LEN  256

enum idme_bootmode {
    IDME_BOOTMODE_NORMAL       = 1,
    IDME_BOOTMODE_DIAG         = 2,
    IDME_BOOTMODE_RECOVERY     = 3,
    IDME_BOOTMODE_EMERGENCY    = 4,
    IDME_BOOTMODE_POWERSAVE    = 5,
    IDME_BOOTMODE_FASTBOOT     = 6,
    IDME_BOOTMODE_TRANSITION   = 7,
    IDME_BOOTMODE_WSWDL        = 8,
    IDME_BOOTMODE_STANDBY_LOGO = 9,
    IDME_BOOTMODE_MAX
};

#define FOS_FLAGS_NONE             (0x00000000ull)
#define FOS_FLAGS_ADB_ON           (0x00000001ull)
#define FOS_FLAGS_ADB_ROOT         (0x00000002ull)
#define FOS_FLAGS_CONSOLE_ON       (0x00000004ull)
#define FOS_FLAGS_RAMDUMP_ON       (0x00000008ull)
#define FOS_FLAGS_VERBOSITY_ON     (0x00000010ull)
#define FOS_FLAGS_ADB_AUTH_DISABLE (0x00000020ull)
#define FOS_FLAGS_FORCE_DM_VERITY  (0x00000040ull) /* deprecated */
#define FOS_FLAGS_DM_VERITY_OFF    (0x00000080ull)
#define FOS_FLAGS_BOOT_DEXOPT      (0x00000100ull)

enum idme_flag_type {
    IDME_FLAGS_FOS = 0,
    IDME_FLAGS_DEV,
    IDME_FLAGS_USR,
    IDME_FLAGS_MAX
};

struct idme_board_info {
    char raw[IDME_MAX_NAME_LEN + 1];
    unsigned int board_type;
    unsigned int board_rev;
    bool has_wan;
};

struct idme_desc {
    char name[IDME_MAX_NAME_LEN];
    uint32_t size;
    uint32_t exportable;
    uint32_t permission;
};

struct idme_item {
    struct idme_desc desc;
    uint8_t data[];
};

struct idme {
    char magic[IDME_MAGIC_SIZE];
    char version[IDME_VERSION_LEN];
    uint32_t items_num;
    uint8_t item_data[];
};

// Advance an item pointer to the next aligned item.
static inline struct idme_item *idmelib_item_next(struct idme_item *item) {
    size_t stride = sizeof(struct idme_desc) + item->desc.size;
    stride = (stride + IDME_ALIGN_SIZE - 1) & ~(IDME_ALIGN_SIZE - 1);
    return (struct idme_item *)((char *)item + stride);
}

// Check if the IDME header magic is valid.
static inline bool idmelib_magic_valid(const struct idme *hdr) {
    return memcmp(hdr->magic, IDME_MAGIC, IDME_MAGIC_SIZE) == 0;
}

// Get a pointer to the first item in the IDME blob.
static inline struct idme_item *idmelib_first_item(struct idme *hdr) {
    return (struct idme_item *)hdr->item_data;
}

// Look up an item by name.
struct idme_item *idmelib_get_item(struct idme *hdr, const char *name);

// Get an item's value as a null-terminated string.
int idmelib_get_var(struct idme *hdr, const char *name, char *buf, size_t len);

// Set an item's value from a string.
int idmelib_set_var(struct idme *hdr, const char *name, const char *value);

// Convert a permission bitmask to a rwx string (e.g. "rwxr-xr-x").
void idmelib_permission_to_str(uint32_t perm, char *buf);

// Check if a binary blob item contains any non-zero data.
bool idmelib_item_has_data(const struct idme_item *item);

// Get the IDME version as a null-terminated string. buf must be at least
// IDME_VERSION_LEN + 1 bytes.
int idmelib_get_version(struct idme *hdr, char *buf, size_t len);

// Set the IDME version from a string (max IDME_VERSION_LEN chars).
int idmelib_set_version(struct idme *hdr, const char *version);

// Get the IDME variable name for a given flag type.
const char *idmelib_flags_var_name(enum idme_flag_type type);

// Read a flags variable as a 64-bit integer.
int idmelib_flags_get(struct idme *hdr, enum idme_flag_type type,
                      unsigned long long *out);

// Write a flags variable as a hex string.
int idmelib_flags_set(struct idme *hdr, enum idme_flag_type type,
                      unsigned long long value);

// Check if specific flag bits are set.
bool idmelib_flags_test(struct idme *hdr, enum idme_flag_type type,
                        unsigned long long bits);

// Decode a flags bitmask to a readable string (e.g. "ADB_ON|CONSOLE_ON").
int idmelib_flags_to_str(unsigned long long flags, char *buf, size_t len);

// Resolve a flag name to its bit value (case-insensitive).
int idmelib_flags_parse_name(const char *name, unsigned long long *out);

// Get the bootmode as an integer.
int idmelib_get_bootmode(struct idme *hdr);

// Convert a bootmode value to a human-readable string.
const char *idmelib_bootmode_to_str(int mode);

// Get the bootcount as an unsigned integer.
int idmelib_get_bootcount(struct idme *hdr, unsigned int *out);

// Decode the board_id variable into its component fields.
int idmelib_get_board_info(struct idme *hdr, struct idme_board_info *info);
