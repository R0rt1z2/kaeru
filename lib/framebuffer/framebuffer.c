//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <lib/framebuffer.h>

static fb_config_t fb_config = {0};

static uint32_t text_scale = 1;
static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static uint32_t text_color = FB_WHITE;

void fb_init(uint32_t *fb_addr, uint32_t width, uint32_t height, uint32_t bppx) {
    fb_config.buffer = fb_addr;
    fb_config.width = width;
    fb_config.height = height;
    fb_config.bppx = bppx;
    
    cursor_x = 0;
    cursor_y = 0;
    text_scale = 1;
    text_color = FB_WHITE;
}

fb_config_t *fb_get_config(void) {
    return &fb_config;
}

bool fb_valid(uint32_t x, uint32_t y) {
    return (x < fb_config.width && y < fb_config.height);
}

void fb_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_valid(x, y) || !fb_config.buffer) return;
    fb_config.buffer[y * fb_config.width + x] = color;
}

void fb_clear(uint32_t color) {
    if (!fb_config.buffer) return;
    
    uint32_t total_pixels = fb_config.width * fb_config.height;
    for (uint32_t i = 0; i < total_pixels; i++) {
        fb_config.buffer[i] = color;
    }
    
    cursor_x = 0;
    cursor_y = 0;
}

void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < w; i++) {
        fb_pixel(x + i, y, color);
        fb_pixel(x + i, y + h - 1, color);
    }
    
    for (uint32_t i = 0; i < h; i++) {
        fb_pixel(x, y + i, color);
        fb_pixel(x + w - 1, y + i, color);
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            fb_pixel(x + col, y + row, color);
        }
    }
}

void fb_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, 
                    uint32_t radius, uint32_t color) {
    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            bool skip_corner = false;
            
            if ((row < radius && col < radius) ||
                (row < radius && col >= w - radius) ||
                (row >= h - radius && col < radius) ||
                (row >= h - radius && col >= w - radius)) {
                
                if ((row < radius - 1 && (col < radius - 1 || col >= w - radius + 1)) ||
                    (row >= h - radius + 1 && (col < radius - 1 || col >= w - radius + 1))) {
                    skip_corner = true;
                }
            }
            
            if (!skip_corner) {
                fb_pixel(x + col, y + row, color);
            }
        }
    }
}

void fb_arrow_right(uint32_t x, uint32_t y, uint32_t size, uint32_t color) {
    for (uint32_t i = 0; i < size; i++) {
        if (i < size / 2) {
            fb_pixel(x + i, y + i, color);
            fb_pixel(x + i + 1, y + i, color);
        } else {
            fb_pixel(x + (size - 1 - i), y + i, color);
            fb_pixel(x + (size - i), y + i, color);
        }
    }
}

void fb_text(uint32_t x, uint32_t y, const char *str, uint32_t color) {
    uint32_t pos_x = x;
    uint32_t char_width = fb_get_char_width();
    
    while (*str) {
        fb_char(pos_x, y, *str, color);
        pos_x += char_width;
        str++;
    }
}

uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void fb_set_cursor(uint32_t x, uint32_t y) {
    cursor_x = x;
    cursor_y = y;
}

void fb_get_cursor(uint32_t *x, uint32_t *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void fb_set_text_color(uint32_t color) {
    text_color = color;
}

uint32_t fb_get_text_color(void) {
    return text_color;
}

void fb_set_text_scale(uint32_t scale) {
    if (scale >= 1 && scale <= 8) {
        text_scale = scale;
    }
}

uint32_t fb_get_text_scale(void) {
    return text_scale;
}

void fb_cursor_advance(void) {
    cursor_x += fb_get_char_width();
    
    if (cursor_x + fb_get_char_width() > fb_config.width) {
        fb_cursor_newline();
    }
}

void fb_cursor_newline(void) {
    cursor_x = 0;
    cursor_y += fb_get_char_height() + 4;
    
    if (cursor_y + fb_get_char_height() > fb_config.height) {
        cursor_y = 0;
    }
}

void fb_cursor_home(void) {
    cursor_x = 0;
    cursor_y = 0;
}

void fb_putc(char c) {
    if (c == '\n') {
        fb_cursor_newline();
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c >= 32) {
        fb_char(cursor_x, cursor_y, c, text_color);
        fb_cursor_advance();
    }
}