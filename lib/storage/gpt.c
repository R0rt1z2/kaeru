//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
//                         2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/debug.h>
#include <lib/storage/part.h>
#include <lib/string.h>

#define GUID_IS_ZERO(g) (memcmp((g), "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0)

static uint8_t gpt_buf[GPT_BLOCK_SIZE] __attribute__((aligned(64)));

int part_parse(struct part_context *ctx, part_read_block_fn read_block, void *read_ctx)
{
    ctx->count = 0;

    if (read_block(GPT_HEADER_LBA, gpt_buf, read_ctx) != 0) {
        printf("GPT header read failed\n");
        return -1;
    }

    struct gpt_header *hdr = (struct gpt_header *)gpt_buf;

    if (memcmp(hdr->signature, "EFI PART", 8) != 0) {
        printf("Bad GPT signature\n");
        return -1;
    }

    uint32_t entry_count = hdr->entry_count;
    uint32_t entry_size  = hdr->entry_size;
    uint64_t entry_start = hdr->entry_start;

    if (entry_count > GPT_MAX_PARTS)
        entry_count = GPT_MAX_PARTS;

    uint32_t entries_per_block = GPT_BLOCK_SIZE / entry_size;
    uint32_t blocks_needed = (entry_count + entries_per_block - 1) / entries_per_block;

    for (uint32_t blk = 0; blk < blocks_needed; blk++) {
        if (read_block((uint32_t)(entry_start + blk), gpt_buf, read_ctx) != 0) {
            printf("GPT entry read failed at block %lu\n", (unsigned long)(entry_start + blk));
            return -1;
        }

        for (uint32_t j = 0; j < entries_per_block && ctx->count < (int)entry_count; j++) {
            struct gpt_entry *e = (struct gpt_entry *)(gpt_buf + j * entry_size);

            if (GUID_IS_ZERO(e->type_guid))
                continue;

            struct part_info *p = &ctx->parts[ctx->count];
            strnarrow(e->name, p->name, GPT_NAME_MAX);
            p->start_block = (uint32_t)e->first_lba;
            p->size_blocks = (uint32_t)(e->last_lba - e->first_lba + 1);
            ctx->count++;
        }
    }

    printf("Found %d GPT partitions:\n", ctx->count);
    part_dump(ctx);
    printf("\n");

    return 0;
}

const struct part_info* part_find(const struct part_context *ctx,
                                  const char *name)
{
    for (int i = 0; i < ctx->count; i++) {
        if (streq(ctx->parts[i].name, name)) {
            return &ctx->parts[i];
        }
    }
    return NULL;
}

uint32_t part_get_start(const struct part_context *ctx, const char *name)
{
    const struct part_info *p = part_find(ctx, name);
    if (!p)
        return 0;

    return p->start_block;
}

uint32_t part_get_size(const struct part_context *ctx, const char *name)
{
    const struct part_info *p = part_find(ctx, name);
    if (!p)
        return 0;

    return p->size_blocks;
}

void part_dump(const struct part_context *ctx)
{
    for (int i = 0; i < ctx->count; i++) {
        printf("  [%2d] %-20s start=%-8lu size=%lu\n",
               i, ctx->parts[i].name,
               ctx->parts[i].start_block,
               ctx->parts[i].size_blocks);
    }
}
