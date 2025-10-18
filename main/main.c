//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/arm.h>
#include <arch/ops.h>
#include <board_ops.h>

void kaeru(void) {
    common_late_init();

#ifdef CONFIG_FRAMEBUFFER_SUPPORT
    fb_init((uint32_t*)CONFIG_FRAMEBUFFER_ADDRESS, 
            CONFIG_FRAMEBUFFER_WIDTH, 
            CONFIG_FRAMEBUFFER_HEIGHT, 
            CONFIG_FRAMEBUFFER_BYTES_PER_PIXEL,
            CONFIG_FRAMEBUFFER_ALIGNMENT);
#endif

    board_late_init();

    ((void (*)(const struct app_descriptor *))(CONFIG_APP_ADDRESS | 1))(NULL);

    arch_idle();
}

__attribute__((section(".text.start"))) void main(void) {
    print_kaeru_info(OUTPUT_CONSOLE);
    common_early_init();
    board_early_init();

    PATCH_CALL(CONFIG_APP_CALLER, (void*)kaeru, TARGET_THUMB);
    ((void (*)(void))(CONFIG_PLATFORM_INIT_ADDRESS | 1))();
}