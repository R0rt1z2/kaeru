//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <inttypes.h>
#include <stddef.h>

#define CACHE_LINE 32

#define ICACHE 1
#define DCACHE 2
#define UCACHE (ICACHE | DCACHE)

void arch_clean_invalidate_cache_range(uintptr_t start, size_t len);
