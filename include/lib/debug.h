//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stddef.h>

#include <lib/nanoprintf.h>
#include <lib/string.h>

typedef enum { OUTPUT_CONSOLE, OUTPUT_VIDEO } output_type_t;

int printf(const char* fmt, ...);
int video_printf(const char* fmt, ...);
void uart_hexdump(const void* data, size_t size);
void video_hexdump(const void* data, size_t size);