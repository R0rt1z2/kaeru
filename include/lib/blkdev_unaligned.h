//
// SPDX-FileCopyrightText: 2026 Ben Grisdale <bengris32@protonmail.ch>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <lib/mt_part.h>
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_BLKDEV_HAS_BROKEN_UNALIGNED_ACCESS
static inline int blkdev_read_unaligned(struct device_t* dev, uint64_t offset, void* buf, size_t size) {
    uint8_t* dst = buf;
    uint8_t block[BLOCK_SIZE];

    if (!dev || dev->init != 1)
        return -1;

    while (size) {
        uint64_t aligned = offset & ~(uint64_t)(BLOCK_SIZE - 1);
        size_t skip = (size_t)(offset - aligned);
        size_t chunk = 0;

        if (skip == 0 && size >= BLOCK_SIZE) {
            chunk = size & ~(size_t)(BLOCK_SIZE - 1);
            if (dev->read(dev, offset, dst, chunk, USER_PART) != chunk)
                return -1;
        } else {
            chunk = BLOCK_SIZE - skip;
            if (chunk > size)
                chunk = size;

            if (dev->read(dev, aligned, block, BLOCK_SIZE, USER_PART) != BLOCK_SIZE)
                return -1;

            for (size_t i = 0; i < chunk; i++)
                dst[i] = block[skip + i];
        }

        offset += chunk;
        dst += chunk;
        size -= chunk;
    }

    return 0;
}

// CAUTION: A partial block has to be read back, modified and written to flash again,
// so a failure part way through can leave the tail of a block updated and the rest
// not. Callers that care should write whole blocks.
static inline int blkdev_write_unaligned(struct device_t* dev, uint64_t offset, const void* buf, size_t size) {
    const uint8_t* src = buf;
    uint8_t block[BLOCK_SIZE];

    if (!dev || dev->init != 1)
        return -1;

    while (size) {
        uint64_t aligned = offset & ~(uint64_t)(BLOCK_SIZE - 1);
        size_t skip = (size_t)(offset - aligned);
        size_t chunk = 0;

        if (skip == 0 && size >= BLOCK_SIZE) {
            chunk = size & ~(size_t)(BLOCK_SIZE - 1);
            if (dev->write(dev, (void*)src, offset, chunk, USER_PART) != chunk)
                return -1;
        } else {
            chunk = BLOCK_SIZE - skip;
            if (chunk > size)
                chunk = size;

            if (dev->read(dev, aligned, block, BLOCK_SIZE, USER_PART) != BLOCK_SIZE)
                return -1;

            for (size_t i = 0; i < chunk; i++)
                block[skip + i] = src[i];

            if (dev->write(dev, block, aligned, BLOCK_SIZE, USER_PART) != BLOCK_SIZE)
                return -1;
        }

        offset += chunk;
        src += chunk;
        size -= chunk;
    }

    return 0;
}

#endif /* CONFIG_BLKDEV_HAS_BROKEN_UNALIGNED_ACCESS */
