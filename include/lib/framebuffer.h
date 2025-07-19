//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include <arch/cache.h>

#define FB_WHITE         0xFFFFFFFF
#define FB_BLACK         0xFF000000
#define FB_RED           0xFFFF0000
#define FB_GREEN         0xFF00FF00
#define FB_BLUE          0xFF0000FF
#define FB_YELLOW        0xFFFFFF00
#define FB_ORANGE        0xFFFF8000
#define FB_CYAN          0xFF00FFFF
#define FB_MAGENTA       0xFFFF00FF
#define FB_GRAY          0xFF808080
#define FB_DARK_RED      0xFF8B0000
#define FB_DARK_GREEN    0xFF006400
#define FB_DARK_BLUE     0xFF00008B
#define FB_DARK_GRAY     0xFF404040
#define FB_LIGHT_GRAY    0xFFD3D3D3
#define FB_SILVER        0xFFC0C0C0
#define FB_MAROON        0xFF800000
#define FB_NAVY          0xFF000080
#define FB_PURPLE        0xFF800080
#define FB_LIME          0xFF00FF00
#define FB_AQUA          0xFF00FFFF
#define FB_FUCHSIA       0xFFFF00FF
#define FB_OLIVE         0xFF808000
#define FB_TEAL          0xFF008080

typedef struct {
    uint32_t *buffer;
    uint32_t width;
    uint32_t height;
    uint32_t bppx;
} fb_config_t;

void fb_init(uint32_t *fb_addr, uint32_t width, uint32_t height, uint32_t bppx);
void fb_clear(uint32_t color);
void fb_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t radius, uint32_t color);
void fb_arrow_right(uint32_t x, uint32_t y, uint32_t size, uint32_t color);

void fb_char(uint32_t x, uint32_t y, char c, uint32_t color);
void fb_text(uint32_t x, uint32_t y, const char *str, uint32_t color);
uint32_t fb_get_char_width(void);
uint32_t fb_get_char_height(void);

void fb_set_cursor(uint32_t x, uint32_t y);
void fb_get_cursor(uint32_t *x, uint32_t *y);
void fb_set_text_color(uint32_t color);
void fb_set_text_scale(uint32_t scale);
uint32_t fb_get_text_scale(void);

uint32_t fb_get_text_color(void);
void fb_putc(char c);


void fb_cursor_newline(void);
void fb_cursor_advance(void);
void fb_cursor_home(void);

bool fb_valid(uint32_t x, uint32_t y);
fb_config_t *fb_get_config(void);
uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b);