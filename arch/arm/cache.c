//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/cache.h>

void arch_clean_invalidate_cache_range(uintptr_t start, uintptr_t size) {
    uintptr_t end = start + size;
    start &= ~(CACHE_LINE - 1);

    while (start < end) {
        __asm__ volatile(
                "mcr p15, 0, %0, c7, c14, 1\n"
                "add %0, %0, %[clsize]\n"
                : "+r"(start)
                : [clsize] "I"(CACHE_LINE)
                : "memory");
    }

    __asm__ volatile("mcr p15, 0, %0, c7, c10, 4\n" ::"r"(0) : "memory");
}