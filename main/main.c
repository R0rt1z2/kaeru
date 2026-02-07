//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/arm.h>
#include <arch/ops.h>
#include <reloc.h>
#include <board_ops.h>

void kaeru(void) {
#ifdef CONFIG_FRAMEBUFFER_SUPPORT
    fb_init((uint32_t*)CONFIG_FRAMEBUFFER_ADDRESS,
            CONFIG_FRAMEBUFFER_WIDTH,
            CONFIG_FRAMEBUFFER_HEIGHT,
            CONFIG_FRAMEBUFFER_BYTES_PER_PIXEL,
            CONFIG_FRAMEBUFFER_ALIGNMENT);
#endif

#ifdef CONFIG_LIBSEJ_SUPPORT
    set_sej_base(CONFIG_SEJ_BASE);
    init_sej_ctx();
#endif

    board_late_init();

    ((void (*)(const struct app_descriptor*))(CONFIG_APP_ADDRESS | 1))(NULL);
}

__attribute__((section(".text.start"))) void main(void) {
    self_relocate();

    uint32_t search_val = CONFIG_APP_ADDRESS | 1;
    uint32_t start = CONFIG_BOOTLOADER_BASE;
    uint32_t end = CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE;
    uint32_t ptr_addr = 0;

    print_kaeru_info(OUTPUT_CONSOLE);
    common_early_init();
    board_early_init();

    for (uint32_t addr = start; addr < end; addr += 4) {
        if (*(volatile uint32_t*)addr == search_val) {
            ptr_addr = addr;
            break;
        }
    }

    if (ptr_addr != 0) {
        *(volatile uint32_t*)ptr_addr = (uint32_t)kaeru | 1;
        arch_clean_invalidate_cache_range(ptr_addr, 4);
    } else {
#if defined(CONFIG_APP_CALLER) && CONFIG_APP_CALLER != 0
        PATCH_CALL(CONFIG_APP_CALLER, (void*)kaeru, TARGET_THUMB);
        arch_clean_invalidate_cache_range(CONFIG_APP_CALLER, 4);
#endif
    }

    ((void (*)(void))(CONFIG_PLATFORM_INIT_ADDRESS | 1))();
}