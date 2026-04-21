//
// SPDX-FileCopyrightText: 2025-2026 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/arm.h>
#include <board_ops.h>
#include <main/main.h>

void kaeru_late_init(void) {
    OPTIONAL_INIT(sej_init);
    OPTIONAL_INIT(framebuffer_init);

    board_late_init();

    ((void (*)(const struct app_descriptor*))(CONFIG_APP_ADDRESS | 1))(NULL);
}

// If support for stage1 is enabled, we are now running in the heap
// and we were called by the previous stage.
//
// If support for stage1 is disabled, then we are essentially being
// called from the original bl platform_init() call.
//
// Regardless of the caller, we want to patch the '.apps' entry in
// the rodata section of the bootloader to point to our late init
// function, so that we can take control before mt_boot_init() runs.
void kaeru_early_init(void) {
    uint32_t search_val = CONFIG_APP_ADDRESS | 1;
    uint32_t start = CONFIG_BOOTLOADER_BASE;
    uint32_t end = CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE;
    uint32_t ptr_addr = 0;

    print_kaeru_info(printf);
    common_early_init();
    board_early_init();

    for (uint32_t addr = start; addr < end; addr += 4) {
        if (*(volatile uint32_t*)addr == search_val) {
            ptr_addr = addr;
            break;
        }
    }

    if (ptr_addr != 0) {
        *(volatile uint32_t*)ptr_addr = (uint32_t)kaeru_late_init | 1;
        arch_clean_cache_range(ptr_addr, 4);
    } else {
        // Not a critical failure, since the device should still boot
        // but I'm sure users will appreciate some feedback about it.
        printf("Failed to patch mt_init_boot() pointer\n");
        printf("kaeru won't be able to run its late init!\n");
    }

    ((void (*)(void))(CONFIG_PLATFORM_INIT_ADDRESS | 1))();
}