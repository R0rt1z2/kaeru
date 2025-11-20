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
    if (!buffer || buffer_size == 0)
        return -1;

    uint64_t lk_size = partition_get_size_by_name("lk");
    if (lk_size == 0) {
        LOG("Failed to get lk partition size\n");
        return -1;
    }

    const char* part_name = CONFIG_BOOTLOADER_PARTITION_NAME;
    size_t pos = 0;

    uint8_t min_hdr[80];

    while (1) {
        if (pos + sizeof(min_hdr) > lk_size) {
            LOG("Reached end of partition without finding kaeru\n");
            goto fail;
        }

        ssize_t read = partition_read(part_name, pos, min_hdr, sizeof(min_hdr));
        if (read != sizeof(min_hdr))
            goto fail;

        uint32_t magic = LE32(min_hdr);
        if (magic != LK_MAGIC)
            goto fail;

        const uint8_t* pname = min_hdr + 8;
        uint32_t ext_magic = LE32(min_hdr + 48);
        uint8_t is_ext = (ext_magic == LK_EXT_MAGIC);

        uint32_t hsz = is_ext ? LE32(min_hdr + 52) : 512;
        if (hsz < 512) hsz = 512;
        if (pos + hsz > lk_size) {
            LOG("Header size would exceed partition bounds\n");
            goto fail;
        }

        uint8_t* hdr = malloc(hsz);
        if (!hdr)
            return -1;

        read = partition_read(part_name, pos, hdr, hsz);
        if (read != hsz) {
            free(hdr);
            goto fail;
        }

        uint64_t data_size = is_ext ?
            (((uint64_t)LE32(hdr + 72) << 32) | LE32(hdr + 4)) :
            LE32(hdr + 4);

        uint32_t align = is_ext ? LE32(hdr + 68) : 8;
        if (!align) align = 8;

        uint8_t is_kaeru = (strncmp((const char*)pname, "kaeru", 5) == 0 && pname[5] == '\0');

        if (is_kaeru) {
            LOG("Found kaeru in lk image!\n");

            size_t data_start = pos + hsz;

            if (data_start + data_size > lk_size) {
                LOG("kaeru data would exceed partition bounds\n");
                free(hdr);
                goto fail;
            }

            free(hdr);

            ssize_t kaeru_data = partition_read(part_name, data_start, buffer, (size_t)data_size);
            if (kaeru_data <= 0)
                return -1;

            return data_size;
        }

        LOG("Found partition %s\n", pname);

        size_t next = pos + hsz + data_size;
        size_t rem = next % align;
        if (rem) next += (align - rem);

        free(hdr);

        if (next <= pos)
            return -1;

        pos = next;
        LOG("Next pos is %d\n", pos);
    }

fail:
    return -1;
}
