//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <arch/ops.h>

void arch_idle(void) {
    while (1) {
        __asm__ volatile("wfi");
    }
}