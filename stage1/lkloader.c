//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/common.h>
#include <lib/string.h>
#include <stage1/lkloader.h>
#include <stage1/memory.h>

static inline int find_kaeru_partition(const void* buf, size_t len, LkPartitionRegion* out) {
    if (!buf || !out) return -1;

    size_t pos = 0;
    if (len < pos + 80) return -1;

    while (pos + 80 <= len) {
        const uint8_t* hdr = (const uint8_t*)buf + pos;
        uint32_t magic = LE32(hdr);

        if (magic != LK_MAGIC) {
            break;
        }

        const uint8_t* pname = hdr + 8;
        char name_str[33] = {0};
        memcpy(name_str, pname, 32);

        if (strncmp((const char*)pname, "kaeru", 5) == 0 && pname[5] == '\0') {
            uint32_t hsz = 512;
            uint64_t data_size = ((uint64_t)LE32(hdr + 72) << 32) | LE32(hdr + 4);
            size_t data_start = pos + hsz;
            if (data_start > len) return -1;
            out->data = (const uint8_t*)buf + data_start;
            out->data_size =
                    (data_size > (len - data_start)) ? (len - data_start) : (size_t)data_size;
            return 0;
        }

        uint32_t ext_magic = LE32(hdr + 48);
        uint8_t is_ext = (ext_magic == LK_EXT_MAGIC);
        uint32_t hsz = is_ext ? LE32(hdr + 52) : 512;
        if (hsz == 0) hsz = 512;

        uint64_t data_size =
                is_ext ? ((uint64_t)LE32(hdr + 72) << 32) | LE32(hdr + 4) : LE32(hdr + 4);

        uint32_t align = is_ext ? LE32(hdr + 68) : 8;
        if (!align) align = 8;

        size_t next = pos + hsz + (size_t)data_size;
        if (align) {
            size_t rem = next % align;
            if (rem) next += align - rem;
        }

        if (next <= pos || next > len) {
            break;
        }

        pos = next;
    }

    return -1;
}

ssize_t load_kaeru_partition(void* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }

    // For legacy LK: fixed 1 MiB size to avoid broken part_t handling.
    uint64_t lk_size = 1024 * 1024;

#ifndef CONFIG_LEGACY_LK
    lk_size = partition_get_size_by_name("lk");

    if (lk_size == 0 || lk_size > (128ULL * 1024 * 1024)) {
        return -1;
    }
#endif

    void* lk_buf = malloc((size_t)lk_size);
    if (!lk_buf) {
        return -1;
    }

    ssize_t read_size = partition_read("lk", 0, lk_buf, (size_t)lk_size);

    if (read_size <= 0) {
        goto fail;
    }

    LkPartitionRegion kaeru;
    if (find_kaeru_partition(lk_buf, (size_t)read_size, &kaeru) != 0) {
        goto fail;
    }

    if (kaeru.data_size == 0 || kaeru.data_size > buffer_size) {
        goto fail;
    }

    memcpy(buffer, kaeru.data, kaeru.data_size);

    free(lk_buf);
    return kaeru.data_size;

fail:
    free(lk_buf);
    return -1;
}
