//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
//                         2026 Shomy <git@itssho.my>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stdint.h>

#define IMG_MAGIC               0x58881688
#define IMG_EXT_MAGIC           0x58891689

#define BFBF_MAGIC              0x42464246

#define IMG_HDR_SZ              0x200
#define IMG_NAME_SZ             32

typedef struct img_hdr {
    uint32_t magic;
    uint32_t dsz;
    char name[IMG_NAME_SZ];
    uint32_t addr;
    uint32_t mode;
    uint32_t ext_magic;
    uint32_t hdr_sz;
    uint32_t hdr_ver;
    uint32_t img_type;
    uint32_t img_list_end;
    uint32_t align_sz;
    uint32_t dsz_ext;
    uint32_t addr_ext;
    uint32_t scrambled;
} img_hdr_t;
