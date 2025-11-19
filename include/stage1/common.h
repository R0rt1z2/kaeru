//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stddef.h>
#include <stdint.h>

#if KAERU_DEBUG
    #define LOG(fmt, ...) dprintf(fmt, ##__VA_ARGS__)
#else
    #define LOG(fmt, ...) 
#endif

typedef long long off_t;
typedef long ssize_t;

void init_storage(void);
size_t dprintf(const char* format, ...);
void platform_init(void);
ssize_t partition_read(const char* part_name, off_t offset, uint8_t* data, size_t size);
uint64_t partition_get_size_by_name(const char* part_name);
