//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <stage1/memory.h>

void* malloc(size_t size) {
    return ((void* (*)(size_t))(CONFIG_MALLOC_ADDRESS | 1))(size);
}

void free(void* ptr) {
    ((void (*)(void*))(CONFIG_FREE_ADDRESS | 1))(ptr);
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}
