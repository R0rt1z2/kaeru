//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/cache.h>
#include <stage1/common.h>
#include <stage1/lkloader.h>
#include <stage1/memory.h>

#define MAX_STAGE2_SIZE 128 * 1024

static inline void kaeru_stage1(void) {
    void* kaeru_stage2 = NULL;
    int ret = 0;

    // It is necessary to nop the call for init_storage in platform_init
    // or the device will panic when stage2 calls platform_init
    volatile uint16_t* x = (volatile uint16_t*)CONFIG_INIT_STORAGE_CALLER;
    x[0] = 0xBF00;  // nop
    x[1] = 0xBF00;  // nop
    arch_clean_invalidate_cache_range((uintptr_t)CONFIG_INIT_STORAGE_CALLER, 2);

    kaeru_stage2 = malloc(MAX_STAGE2_SIZE);

    if (kaeru_stage2 == NULL) {
        dprintf("Kaeru Stage 1 malloc failed\n");
        goto fail;
    }

    ret = load_kaeru_partition(kaeru_stage2, MAX_STAGE2_SIZE);
    if (ret <= 0 || ret > MAX_STAGE2_SIZE) {
        dprintf("Failed to load Kaeru Stage 2!\n");
        goto fail;
    }

    dprintf("Jumping to Kaeru stage 2\n");
    ((void (*)(void))((uint32_t)kaeru_stage2 | 1))();

    return;

fail:
    if (kaeru_stage2) {
        free(kaeru_stage2);
    }
    platform_init();
}

__attribute__((section(".text.start"))) void main(void) {
    dprintf("Welcome from Kaeru Stage 1!\n");
    init_storage();
    kaeru_stage1();
}
