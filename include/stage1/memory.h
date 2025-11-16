//
// SPDX-FileCopyrightText: 2025 Shomy <shomy@shomy.is-a.dev>
//                         2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stddef.h>

void* memcpy(void* dest, const void* src, size_t n);
void* malloc(size_t size);
void free(void* ptr);
