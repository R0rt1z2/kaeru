//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stddef.h>
#include <stdarg.h>

#include <lib/nanoprintf.h>

typedef enum {
    OUTPUT_CONSOLE,
    OUTPUT_VIDEO,
#ifdef CONFIG_FRAMEBUFFER_SUPPORT
    OUTPUT_FRAMEBUFFER,
#endif
} output_type_t;

int printf(const char* fmt, ...);
int video_printf(const char* fmt, ...);

#ifdef CONFIG_FRAMEBUFFER_SUPPORT
int fb_printf(const char* fmt, ...);
int fb_vprintf(const char *fmt, va_list args);
void fb_hexdump(const void* data, size_t size);
void fb_update_display(void);
#endif

void hexdump(const void* data, size_t size, output_type_t output_type);
void uart_hexdump(const void* data, size_t size);
void video_hexdump(const void* data, size_t size);