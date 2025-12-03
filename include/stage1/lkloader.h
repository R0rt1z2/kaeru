//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stage1/common.h>

#define LK_MAGIC 0x58881688U
#define LK_EXT_MAGIC 0x58891689U
#define BFBF_MAGIC 0x42464246U

#define MIN_HEADER_SIZE 80
#define DEFAULT_HEADER_SIZE 512
#define DEFAULT_ALIGNMENT 8

ssize_t load_kaeru_partition(void* buffer, size_t buffer_size);
