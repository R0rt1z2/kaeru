//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/arm.h>
#include <arch/ops.h>
#include <board_ops.h>
#include <lib/app.h>
#include <lib/common.h>
#include <lib/debug.h>
#include <lib/fastboot.h>

void kaeru(void) {
    common_late_init();
    board_late_init();

    ((void (*)(const struct app_descriptor *))(CONFIG_APP_ADDRESS | 1))(NULL);

    video_printf("Shouldn't have reached here :(\n");
    arch_idle();
}

__attribute__((section(".text.start"))) void main(void) {
    print_kaeru_info(OUTPUT_CONSOLE);
    common_early_init();
    board_early_init();

    PATCH_CALL(CONFIG_APP_CALLER, (void*)kaeru, TARGET_THUMB);
    ((void (*)(void))(CONFIG_PLATFORM_INIT_ADDRESS | 1))();
}