//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <stdarg.h>

#define NANOPRINTF_IMPLEMENTATION

#include <lib/debug.h>
#include <lib/string.h>
#ifdef CONFIG_FRAMEBUFFER_SUPPORT
#include <lib/framebuffer.h>
#endif

#include <uart/mtk_uart.h>

void uart_putc(int c, void* ctx) {
    (void)ctx;
    
    if (c == '\n')
        mtk_uart_putc('\r');
    
    mtk_uart_putc(c);
}

#ifdef CONFIG_LK_LOG_STORE
void lk_log_store(int c, void* ctx) {
    (void)ctx;
    ((void (*)(int))(CONFIG_LK_LOG_STORE_ADDRESS | 1))(c);
}
#endif

int printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = npf_vpprintf(&uart_putc, NULL, fmt, args);

#ifdef CONFIG_LK_LOG_STORE
    va_start(args, fmt);
    npf_vpprintf(&lk_log_store, NULL, fmt, args);
#endif

    va_end(args);
    return ret;
}

int video_printf(const char* fmt, ...) {
    return ((int (*)(const char*))(CONFIG_VIDEO_PRINTF_ADDRESS | 1))(fmt);
}

#ifdef CONFIG_FRAMEBUFFER_SUPPORT
void fb_putc_callback(int c, void *ctx) {
    (void)ctx;
    fb_putc(c);
}

int fb_vprintf(const char *fmt, va_list args) {
    return npf_vpprintf(&fb_putc_callback, NULL, fmt, args);
}

void fb_update_display(void) {
    fb_config_t *config = fb_get_config();
    if (!config->buffer) return;
    uint32_t fb_size = config->width * config->height * config->bppx;
    arch_clean_invalidate_cache_range((uintptr_t)config->buffer, fb_size);
}

int fb_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = fb_vprintf(fmt, args);
    va_end(args);
    fb_update_display();
    return ret;
}
#endif

void hexdump(const void* data, size_t size, output_type_t output_type) {
    size_t i, j;
    const uint8_t* ptr = (const uint8_t*)data;

#define HEXDUMP(print_function)                                           \
    do {                                                                  \
        for (i = 0; i < size; i += 16) {                                  \
            print_function("%08x: ", (uintptr_t)(ptr + i));               \
                                                                          \
            for (j = 0; j < 16; j++) {                                    \
                if (i + j < size) {                                       \
                    print_function("%02x ", ptr[i + j]);                  \
                } else {                                                  \
                    print_function("   ");                                \
                }                                                         \
                                                                          \
                if (j == 7) {                                             \
                    print_function(" ");                                  \
                }                                                         \
            }                                                             \
                                                                          \
            print_function(" |");                                         \
            for (j = 0; j < 16; j++) {                                    \
                if (i + j < size) {                                       \
                    uint8_t c = ptr[i + j];                               \
                    print_function("%c", (c >= 32 && c < 127) ? c : '.'); \
                }                                                         \
            }                                                             \
            print_function("|\n");                                        \
        }                                                                 \
    } while (0)

    switch (output_type) {
        case OUTPUT_CONSOLE:
            HEXDUMP(printf);
            break;
        case OUTPUT_VIDEO:
            HEXDUMP(video_printf);
            break;
#ifdef CONFIG_FRAMEBUFFER_SUPPORT
        case OUTPUT_FRAMEBUFFER:
            HEXDUMP(fb_printf);
            break;
#endif
    }

#undef HEXDUMP
}

void uart_hexdump(const void* data, size_t size) {
    hexdump(data, size, OUTPUT_CONSOLE);
}

void video_hexdump(const void* data, size_t size) {
    hexdump(data, size, OUTPUT_VIDEO);
}

#ifdef CONFIG_FRAMEBUFFER_SUPPORT
void fb_hexdump(const void* data, size_t size) {
    hexdump(data, size, OUTPUT_FRAMEBUFFER);
}
#endif