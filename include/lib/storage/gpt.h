//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
//                         2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <inttypes.h>

#define GPT_MAX_PARTS    128
#define GPT_NAME_MAX     36
#define GPT_HEADER_LBA   1
#define GPT_ENTRY_LBA    2
#define GPT_BLOCK_SIZE   512

#define PARTS_MAX         GPT_MAX_PARTS
#define PART_NAME_MAX     GPT_NAME_MAX

struct gpt_header {
    uint8_t  signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable;
    uint64_t last_usable;
    uint8_t  disk_guid[16];
    uint64_t entry_start;
    uint32_t entry_count;
    uint32_t entry_size;
    uint32_t entry_crc;
};

struct gpt_entry {
    uint8_t  type_guid[16];
    uint8_t  part_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[GPT_NAME_MAX];
};
