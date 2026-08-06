//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
//                         2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#ifdef CONFIG_USE_PMT_PARTITION
#include "pmt.h"
#else
#include "gpt.h"
#endif

struct part_info {
    char     name[PART_NAME_MAX + 1];
    uint32_t start_block;
    uint32_t size_blocks;
};

struct part_context {
    struct part_info parts[PARTS_MAX];
    int              count;
};

// APIs to be implemented by a parser.
typedef int (*part_read_block_fn)(uint32_t blk, void *buf, void *ctx);

int      part_parse(struct part_context *ctx, part_read_block_fn read_block, void *read_ctx);
const struct part_info* part_find(const struct part_context *ctx, const char *name);
uint32_t part_get_start(const struct part_context *ctx, const char *name);
uint32_t part_get_size(const struct part_context *ctx, const char *name);
void     part_dump(const struct part_context *ctx);
