//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <stage1/common.h>
#include <lib/storage.h>

void init_storage(void) {
#ifdef CONFIG_LEGACY_LK
    // Legacy lk needs a int argument to know what device to init (emmc, sdcard..)
    ((void (*)(int))(CONFIG_INIT_STORAGE_ADDRESS | 1))(1);
#else
    ((void (*)(void))(CONFIG_INIT_STORAGE_ADDRESS | 1))();
#endif
}

size_t dprintf(const char* format, ...) {
    return ((size_t (*)(const char* format, ...))(CONFIG_DPRINTF_ADDRESS | 1))(format);
}

void platform_init(void) {
    ((void (*)(void))(CONFIG_PLATFORM_INIT_ADDRESS | 1))();
}

#ifdef CONFIG_LEGACY_LK

struct device_t* mt_part_get_device(void) {
    return ((struct device_t * (*)(void))(CONFIG_MT_PART_GET_DEVICE_ADDRESS | 1))();
}

part_t* mt_part_get_partition(const char* name) {
    return ((part_t * (*)(const char*))(CONFIG_MT_PART_GET_PARTITION_ADDRESS | 1))(name);
}

#endif

ssize_t partition_read(const char* part_name, off_t offset, uint8_t* data, size_t size) {
#ifdef CONFIG_LEGACY_LK
    struct device_t* dev = mt_part_get_device();
    if (!dev || dev->init != 1) {
        return -1;
    }

    part_t* part = mt_part_get_partition(part_name);
    if (!part) {
        return -1;
    }

    uint64_t part_offset = (part->start_sect * BLOCK_SIZE) + offset;
    ssize_t read_bytes = dev->read(dev, part_offset, data, size, part->part_id);
    if (read_bytes < 0) {
        return -1;
    }

    return read_bytes;
#else
    return ((ssize_t(*)(const char*, off_t, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
#endif
}

uint64_t partition_get_size_by_name(const char* part_name) {
#ifdef CONFIG_LEGACY_LK
    part_t* part = mt_part_get_partition(part_name);
    if (!part) {
        return 0;
    }

    uint64_t part_size = part->nr_sects * BLOCK_SIZE;
    return part_size;
#else
    return ((uint64_t (*)(const char*))(CONFIG_PARTITION_GET_SIZE_BY_NAME_ADDRESS | 1))(part_name);
#endif
}
