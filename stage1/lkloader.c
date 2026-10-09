//
// SPDX-FileCopyrightText: 2025-2026 Shomy <shomy@shomy.is-a.dev>
//                         2025-2026 Roger Ortiz <roger@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/string.h>
#include <lib/image.h>
#include <stage1/lkloader.h>
#include <stage1/memory.h>

// MediaTek firmware uses a standardized image format where a partition
// may contain multiple "sub-partitions", each with its own header.
//
// We package stage 2 as a "kaeru" sub-partition within the bootloader
// partition, so we can locate and load it without hardcoded offsets.
ssize_t load_kaeru_partition(void* buffer, size_t buffer_size) {
    const char* part_name = CONFIG_BOOTLOADER_PARTITION_NAME;

    if (!buffer || buffer_size == 0)
        return -1;

    uint64_t lk_size = partition_get_size_by_name(part_name);
    if (lk_size == 0) {
        LOG("Failed to get partition size for '%s'\n", part_name);
        return -1;
    }

    LOG("Partition '%s' size: 0x%X bytes\n", part_name, (uint32_t)lk_size);

    size_t pos = 0;
    img_hdr_t hdr;

    while (pos + sizeof(img_hdr_t) <= lk_size) {
        ssize_t read = partition_read(part_name, pos, (uint8_t*)&hdr, sizeof(img_hdr_t));
        if (read != (ssize_t)sizeof(img_hdr_t))
            break;

        if (hdr.magic != IMG_MAGIC)
            break;

        uint8_t is_ext = (hdr.ext_magic == IMG_EXT_MAGIC);

        uint32_t hsz = is_ext ? hdr.hdr_sz : IMG_HDR_SZ;
        if (hsz < IMG_HDR_SZ)
            hsz = IMG_HDR_SZ;

        if (pos + hsz > lk_size) {
            LOG("Header size exceeds partition bounds\n");
            break;
        }


        uint64_t data_size = is_ext ? ((uint64_t)hdr.dsz_ext << 32) | hdr.dsz : hdr.dsz;
        uint32_t align = is_ext ? hdr.align_sz : DEFAULT_ALIGNMENT;

        if (!align)
            align = DEFAULT_ALIGNMENT;

        if (strncmp(hdr.name, "kaeru", 5) == 0 && hdr.name[5] == '\0') {
            LOG("Found kaeru partition!\n");

            size_t data_start = pos + hsz;
            if (data_start + data_size > lk_size) {
                LOG("kaeru data exceeds partition bounds\n");
                break;
            }

            if (data_size > buffer_size) {
                LOG("kaeru data exceeds buffer size\n");
                return -1;
            }

            ssize_t kaeru_read = partition_read(part_name, data_start, buffer, (size_t)data_size);
            if (kaeru_read != (ssize_t)data_size)
                return -1;

            return (ssize_t)data_size;
        }

        LOG("Skipping partition: %s\n", hdr.name);

        size_t next = pos + hsz + data_size;
        size_t rem = next % align;
        if (rem)
            next += (align - rem);

        if (next <= pos)
            return -1;

        if (is_ext && hdr.img_list_end)
            break;

        pos = next;
    }

    LOG("kaeru partition not found\n");
    return -1;
}
