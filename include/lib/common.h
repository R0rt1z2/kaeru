//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <arch/cache.h>
#include <lib/debug.h>
#include <stdbool.h>

#define KAERU_ENV_BLDR_SPOOF "kaeru_bootloader_spoof_status"

#define LK_BASE CONFIG_BOOTLOADER_BASE
#define LK_SIZE CONFIG_BOOTLOADER_SIZE

#define LK_START ((LK_BASE) & ~0xFFF)
#define LK_END ((LK_START) + LK_SIZE)

#define WRITE8(addr, value)                                     \
    do {                                                        \
        *(volatile uint8_t*)(addr) = (uint8_t)(value);          \
        arch_clean_invalidate_cache_range((uint32_t)(addr), 1); \
    } while (0)

#define READ8(addr) (*(volatile uint8_t*)(addr))

#define SET8(addr, mask) WRITE8(addr, READ8(addr) | (mask))

#define CLR8(addr, mask) WRITE8(addr, READ8(addr) & ~(mask))

#define MASK8(addr, mask, value) WRITE8(addr, (READ8(addr) & ~(mask)) | (value))

#define WRITE16(addr, value)                                    \
    do {                                                        \
        *(volatile uint16_t*)(addr) = (uint16_t)(value);        \
        arch_clean_invalidate_cache_range((uint32_t)(addr), 2); \
    } while (0)

#define READ16(addr) (*(volatile uint16_t*)(addr))

#define SET16(addr, mask) WRITE16(addr, READ16(addr) | (mask))

#define CLR16(addr, mask) WRITE16(addr, READ16(addr) & ~(mask))

#define MASK16(addr, mask, value) WRITE16(addr, (READ16(addr) & ~(mask)) | (value))

#define WRITE32(addr, value)                                    \
    do {                                                        \
        *(volatile uint32_t*)(addr) = (uint32_t)(value);        \
        arch_clean_invalidate_cache_range((uint32_t)(addr), 4); \
    } while (0)

#define READ32(addr) (*(volatile uint32_t*)(addr))

#define SET32(addr, mask) WRITE32(addr, READ32(addr) | (mask))

#define CLR32(addr, mask) WRITE32(addr, READ32(addr) & ~(mask))

#define MASK32(addr, mask, value) WRITE32(addr, (READ32(addr) & ~(mask)) | (value))

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define ROUND_TO_PAGE(x, y) (((x) + (y)) & (~(y)))

#define LE32(p) ( \
    ((uint32_t)((const uint8_t *)(p))[0])       | \
    ((uint32_t)((const uint8_t *)(p))[1] << 8)  | \
    ((uint32_t)((const uint8_t *)(p))[2] << 16) | \
    ((uint32_t)((const uint8_t *)(p))[3] << 24) )

#define LE64(hi, lo) ((((uint64_t)(hi)) << 32) | (uint64_t)(lo))

void common_early_init(void);
void common_late_init(void);

void mtk_wdt_reset(void);
bool mtk_detect_key(unsigned short key);
const char* get_mode_string(unsigned int mode);
void print_kaeru_info(output_type_t output_type);
void reboot_emergency(void);
