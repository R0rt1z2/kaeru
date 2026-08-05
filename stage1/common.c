//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <stage1/common.h>

#ifdef CONFIG_LEGACY_LK
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

ssize_t partition_read(const char* part_name, off_t offset, uint8_t* data, size_t size) {
#ifdef CONFIG_LEGACY_LK
    struct device_t* dev = mt_part_get_device();
    if (!dev || dev->init != 1)
        return -1;

    part_t* part = mt_part_get_partition(part_name);
    if (!part)
        return -1;

    ssize_t read_bytes = dev->read(dev, mt_part_offset(part) + offset, data, size, part->part_id);
    return (read_bytes < 0) ? -1 : read_bytes;
#else
    return ((ssize_t (*)(const char*, off_t, uint8_t*, size_t))(CONFIG_PARTITION_READ_ADDRESS | 1))(
            part_name, offset, data, size);
#endif
}

uint64_t partition_get_size_by_name(const char* part_name) {
#ifdef CONFIG_LEGACY_LK
    part_t* part = mt_part_get_partition(part_name);
    if (!part)
        return 0;

    return mt_part_size(part);
#else
    return ((uint64_t (*)(const char*))(CONFIG_PARTITION_GET_SIZE_BY_NAME_ADDRESS | 1))(part_name);
#endif
}
