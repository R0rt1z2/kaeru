//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <stage1/common.h>

#if defined(CONFIG_USE_MT_PART_API) || defined(CONFIG_USE_LEGACY_PARTITION_API)
#ifdef CONFIG_BLKDEV_HAS_BROKEN_UNALIGNED_ACCESS
#include <lib/blkdev_unaligned.h>
#endif
#include <lib/mt_part.h>
#endif

void init_storage(void) {
    // AAPCS: r0-r3 are caller-saved scratch registers regardless of
    // callee's actual parameter count. Passing an unused arg is harmless.
    ((void (*)(int))(CONFIG_INIT_STORAGE_ADDRESS | 1))(1);
}

size_t dprintf(const char* format, ...) {
    return ((size_t (*)(const char*, ...))(CONFIG_DPRINTF_ADDRESS | 1))(format);
}

void platform_init(void) {
    ((void (*)(void))(CONFIG_PLATFORM_INIT_ADDRESS | 1))();
}

#ifdef CONFIG_USE_LEGACY_PARTITION_API
static inline int partition_get_index(const char* name) {
    return ((int (*)(const char*))(CONFIG_PARTITION_GET_INDEX_ADDRESS | 1))(name);
}

static inline uint64_t partition_get_offset(int index) {
    return ((uint64_t (*)(int))(CONFIG_PARTITION_GET_OFFSET_ADDRESS | 1))(index);
}

static inline uint64_t partition_get_size(int index) {
    return ((uint64_t (*)(int))(CONFIG_PARTITION_GET_SIZE_ADDRESS | 1))(index);
}
#endif

ssize_t partition_read(const char* part_name, off_t offset, uint8_t* data, size_t size) {
#if defined(CONFIG_USE_LEGACY_PARTITION_API)
    struct device_t* dev = mt_part_get_device();
    if (!dev || dev->init != 1)
        return -1;

    int index = partition_get_index(part_name);
    if (index < 0)
        return -1;

#ifdef CONFIG_BLKDEV_HAS_BROKEN_UNALIGNED_ACCESS
    if (blkdev_read_unaligned(dev, partition_get_offset(index) + offset, data, size))
        return -1;

    return (ssize_t)size;
#else
    ssize_t read_bytes = dev->read(dev, partition_get_offset(index) + offset, data, size, USER_PART);
    return (read_bytes < 0) ? -1 : read_bytes;
#endif
#elif defined(CONFIG_USE_MT_PART_API)
    struct device_t* dev = mt_part_get_device();
    if (!dev || dev->init != 1)
        return -1;

    part_t* part = mt_part_get_partition(part_name);
    if (!part)
        return -1;

#ifdef CONFIG_BLKDEV_HAS_BROKEN_UNALIGNED_ACCESS
    if (blkdev_read_unaligned(dev, mt_part_offset(part) + offset, data, size))
        return -1;

    return (ssize_t)size;
#else
    ssize_t read_bytes = dev->read(dev, mt_part_offset(part) + offset, data, size, part->part_id);
    return (read_bytes < 0) ? -1 : read_bytes;
#endif
#else
    return ((ssize_t (*)(const char*, off_t, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
#endif
}

uint64_t partition_get_size_by_name(const char* part_name) {
#if defined(CONFIG_USE_LEGACY_PARTITION_API)
    int index = partition_get_index(part_name);
    if (index < 0)
        return 0;

    uint64_t size = partition_get_size(index);
    return (size == (uint64_t)-1) ? 0 : size;
#elif defined(CONFIG_USE_MT_PART_API)
    part_t* part = mt_part_get_partition(part_name);
    if (!part)
        return 0;

    return mt_part_size(part);
#else
    return ((uint64_t (*)(const char*))(CONFIG_PARTITION_GET_SIZE_BY_NAME_ADDRESS | 1))(part_name);
#endif
}
