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
    int ret = 0;
    void* kaeru_stage2 = NULL;

    dprintf("Hello from kaeru stage 1!\n");
    init_storage();

    // The original platform_init() always attempts to initialize storage,
    // regardless of whether it has already been initialized. Normally, this
    // makes sense because the function is never called twice.
    //
    // However, this is a problem for us: we need storage initialized to load
    // stage 2, but we also still want to execute the original platform_init()
    // because it does much more than just initialize storage.
    //
    // The easiest solution is to NOP out the BL instruction used by
    // platform_init() to call the storage init function, since we already
    // did that manually.
    volatile uint16_t* x = (volatile uint16_t*)CONFIG_INIT_STORAGE_CALLER;
    x[0] = 0xBF00;
    x[1] = 0xBF00;
    arch_sync_cache_range(CONFIG_INIT_STORAGE_CALLER, 4);

    kaeru_stage2 = malloc(MAX_STAGE2_SIZE);

    if (kaeru_stage2 == NULL) {
        dprintf("kaeru stage 1 malloc failed\n");
        goto fail;
    }

    ret = load_kaeru_partition(kaeru_stage2, MAX_STAGE2_SIZE);
    if (ret <= 0 || ret > MAX_STAGE2_SIZE) {
        dprintf("Failed to load kaeru stage 2!\n");
        goto fail;
    }

    dprintf("Jumping to kaeru stage 2\n");
    ((void (*)(void))((uint32_t)kaeru_stage2 | 1))();

    return;

fail:
    if (kaeru_stage2) {
        free(kaeru_stage2);
    }

    // We failed to load stage 2, but that doesn't necessarily mean we
    // have to stop here.
    //
    // Invoke the original platform_init() to resume normal boot. This
    // means kaeru won't be available, but at least the device won't be
    // bricked.
    platform_init();
}

// bootstrap2 called us via a BL instruction. This would have
// executed the original platform_init(), but we take control
// instead.
//
// NOTE: platform_init() is a void function, so there is no
// return value to handle here.
__attribute__ ((section (".text.start"))) void main(void) {
    kaeru_stage1();
}