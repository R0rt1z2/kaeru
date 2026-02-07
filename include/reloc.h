//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <inttypes.h>
#include <arch/cache.h>

extern uint32_t __got_start;
extern uint32_t __got_end;

static inline int32_t get_reloc_offset(void) {
    uint32_t runtime, link;
    asm volatile(
        "1:                 \n"
        "   adr  %0, 1b    \n"
        "   ldr  %1, =1b   \n"
        : "=r"(runtime), "=r"(link)
    );
    return (int32_t)(runtime - link);
}

static inline void self_relocate(void) {
    int32_t delta = get_reloc_offset();
    if (delta == 0)
        return;

    uint32_t *start = (uint32_t *)((uint32_t)&__got_start + delta);
    uint32_t *end   = (uint32_t *)((uint32_t)&__got_end + delta);

    for (uint32_t *entry = start; entry < end; entry++) {
        if (*entry != 0)
            *entry += delta;
    }

    arch_clean_invalidate_cache_range((uint32_t)start, (uint32_t)end - (uint32_t)start);
}