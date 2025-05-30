//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/arm.h>
#include <board_ops.h>
#include <lib/common.h>
#include <lib/debug.h>

void board_early_init(void) {
    printf("Entering early init for Redmi Note 4\n");
}

void board_late_init(void) {
    printf("Entering late init for Redmi Note 4\n");
}