//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <stdarg.h>

#define NANOPRINTF_IMPLEMENTATION
#include <lib/nanoprintf.h>

#include <lib/debug.h>
#include <lib/string.h>

void low_uart_put(int ch) {
    // The SoC sets 0x20 when UART isn't busy
    while (!(*(volatile uint32_t*)CONFIG_UART_REG0 & 0x20))
        ;
    *(volatile uint32_t*)CONFIG_UART_REG1 = ch;
}

void uart_putc(int c, void* ctx) {
    (void)ctx;
    if (c == '\n') low_uart_put('\r');
    low_uart_put(c);
}

#ifdef CONFIG_LK_LOG_STORE
void lk_log_store(int c, void* ctx) {
    (void)ctx;
#ifdef CONFIG_LK_LOG_STORE_ADDRESS
    ((void (*)(int))(CONFIG_LK_LOG_STORE_ADDRESS | 1))(c);
#endif
}
#endif

int printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = npf_vpprintf(&uart_putc, NULL, fmt, args);

#ifdef CONFIG_LK_LOG_STORE
#ifdef CONFIG_LK_LOG_STORE_ADDRESS
    va_start(args, fmt);
    npf_vpprintf(&lk_log_store, NULL, fmt, args);
#else
#warning "LK Log Store enabled but no address specified"
#endif
#endif

    va_end(args);
    return ret;
}

int video_printf(const char* fmt, ...) {
#if defined(CONFIG_VIDEO_PRINTF_ADDRESS)
    return ((int (*)(const char*))(CONFIG_VIDEO_PRINTF_ADDRESS | 1))(fmt);
#else
#warning "video_printf() is not implemented"
    printf("video_printf() is not implemented\n");
    return 0;
#endif
}

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
    }

#undef HEXDUMP
}

void uart_hexdump(const void* data, size_t size) {
    hexdump(data, size, OUTPUT_CONSOLE);
}

void video_hexdump(const void* data, size_t size) {
    hexdump(data, size, OUTPUT_VIDEO);
}