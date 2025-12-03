//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/string.h>
#include <stage1/lkloader.h>
#include <stage1/memory.h>

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
    uint8_t min_hdr[MIN_HEADER_SIZE];

    while (pos + sizeof(min_hdr) <= lk_size) {
        ssize_t read = partition_read(part_name, pos, min_hdr, sizeof(min_hdr));
        if (read != sizeof(min_hdr))
            break;

        uint32_t magic = LE32(min_hdr);
        if (magic != LK_MAGIC)
            break;

        const char* pname = (const char*)(min_hdr + 8);
        uint32_t ext_magic = LE32(min_hdr + 48);
        uint8_t is_ext = (ext_magic == LK_EXT_MAGIC);

        uint32_t hsz = is_ext ? LE32(min_hdr + 52) : DEFAULT_HEADER_SIZE;
        if (hsz < DEFAULT_HEADER_SIZE)
            hsz = DEFAULT_HEADER_SIZE;

        if (pos + hsz > lk_size) {
            LOG("Header size exceeds partition bounds\n");
            break;
        }

        uint8_t* hdr = malloc(hsz);
        if (!hdr)
            return -1;

        read = partition_read(part_name, pos, hdr, hsz);
        if (read != hsz) {
            free(hdr);
            break;
        }

        uint64_t data_size = is_ext ?
            (((uint64_t)LE32(hdr + 72) << 32) | LE32(hdr + 4)) :
            LE32(hdr + 4);

        uint32_t align = is_ext ? LE32(hdr + 68) : DEFAULT_ALIGNMENT;
        if (!align)
            align = DEFAULT_ALIGNMENT;

        if (strncmp(pname, "kaeru", 5) == 0 && pname[5] == '\0') {
            LOG("Found kaeru partition!\n");

            size_t data_start = pos + hsz;
            if (data_start + data_size > lk_size) {
                LOG("kaeru data exceeds partition bounds\n");
                free(hdr);
                break;
            }

            free(hdr);

            ssize_t kaeru_read = partition_read(part_name, data_start, buffer, (size_t)data_size);
            if (kaeru_read <= 0)
                return -1;

            return (ssize_t)data_size;
        }

        LOG("Skipping partition: %s\n", pname);

        size_t next = pos + hsz + data_size;
        size_t rem = next % align;
        if (rem)
            next += (align - rem);

        free(hdr);

        if (next <= pos)
            return -1;

        pos = next;
    }

    LOG("kaeru partition not found\n");
    return -1;
}