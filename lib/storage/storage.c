//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/debug.h>
#include <lib/mt_part.h>
#include <lib/storage.h>
#include <lib/string.h>

static struct {
    struct part_context part;
    uint8_t initialized;
} ctx;

static int storage_part_read_block(uint32_t blk, void *buf, void *ctx) {
    if (!buf || !ctx) {
        return -1;
    }

    struct device_t *dev = ctx;

    uint64_t offset = (uint64_t)blk * BLOCK_SIZE;
    ssize_t read_sz = dev->read(dev, offset, buf, BLOCK_SIZE, USER_PART);
    return (read_sz == BLOCK_SIZE) ? 0 : -1;
}

// Finds a partition in the partition table by its name.
const struct part_info* storage_part_find(const char *name) {
    if (!ctx.initialized) {
        printf("Storage subsystem is not initialized!\n");
        return NULL;
    }

    return part_find(&ctx.part, name);
}

// Reads from a partition.
ssize_t storage_part_read(const struct part_info* part, void *dst,
                          uint64_t off, size_t size) {
    if (!part || !dst || !size)
        return -1;

    if (size > part->size_blocks * BLOCK_SIZE)
        return -1;

    if (!ctx.initialized) {
        printf("Storage subsystem is not initialized!\n");
        return -1;
    }

    struct device_t *dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("Block device not initialized!\n");
        return -1;
    }

    uint64_t offset = ((uint64_t)part->start_block * BLOCK_SIZE) + off;
    ssize_t read_sz = dev->read(dev, offset, dst, size, USER_PART);
    return read_sz;
}

// Writes to a partition.
ssize_t storage_part_write(const struct part_info* part, void *src,
                           uint64_t off, size_t size) {
    if (!part || !src || !size)
        return -1;

    if (size > part->size_blocks * BLOCK_SIZE)
        return -1;

    if (!ctx.initialized) {
        printf("Storage subsystem is not initialized!\n");
        return -1;
    }

    struct device_t *dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("Block device not initialized!\n");
        return -1;
    }

    uint64_t offset = ((uint64_t)part->start_block * BLOCK_SIZE) + off;
    ssize_t read_sz = dev->write(dev, src, offset, size,
                                 USER_PART);
    return read_sz;
}

// Initialise the storage API.
void storage_init(void) {
    memset(&ctx, 0, sizeof(ctx));

    struct device_t *dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        printf("Block device not initialized!\n");
        return;
    }

    if (part_parse(&ctx.part, &storage_part_read_block, dev)) {
        printf("%s: Failed to parse partition table!\n", __func__);
        return;
    }

    ctx.initialized = 1;
}
