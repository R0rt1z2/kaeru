//
// SPDX-FileCopyrightText: 2026 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stdint.h>

#define OPTIONAL_INIT(fn) \
    do { if (fn) fn(); } while (0)

/* these are weak no-ops by default, overridden by the
   real implementation when the subsystem is compiled in */
void __attribute__((weak)) sej_init(void);
void __attribute__((weak)) framebuffer_init(void);