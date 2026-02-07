//
// SPDX-FileCopyrightText: 2025 Roger Ortiz <me@r0rt1z2.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include <arch/cache.h>
#include <lib/string.h>

#define ARM_MODE(lr) ((lr)&1 ? "THUMB" : "ARM")
#define READ_SP(var) asm volatile("mov %0, sp" : "=r"(var))
#define READ_LR(var) asm volatile("mov %0, lr" : "=r"(var))
#define READ_CPSR(var) asm volatile("mrs %0, cpsr" : "=r"(var))
#define READ_VBAR(var) asm volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(var))

typedef enum { TARGET_THUMB, TARGET_ARM } arm_mode_t;

#define PATCH_CALL(addr, func, mode)                                               \
    do {                                                                           \
        uint32_t cur = (addr) + 4;                                                 \
        uint32_t tgt = ((uint32_t)(func)) & ~1;                                    \
        int32_t off = tgt - cur;                                                   \
        uint16_t hi = (off >> 12) & 0x7FF;                                         \
        uint16_t lo = (off >> 1) & 0x7FF;                                          \
        uint16_t hi_inst = 0xF000 | hi;                                            \
        uint16_t lo_inst = ((mode) == TARGET_ARM) ? (0xE800 | lo) : (0xF800 | lo); \
        volatile uint16_t* p = (volatile uint16_t*)(addr);                         \
        *p = hi_inst;                                                              \
        *(p + 1) = lo_inst;                                                        \
    } while (0)

#define PATCH_BRANCH(addr, func)                          \
    do {                                                  \
        uint32_t cur = (addr) + 4;                        \
        uint32_t tgt = ((uint32_t)(func)) & ~1;           \
        int32_t off = tgt - cur;                          \
        uint16_t hi = (off >> 12) & 0x7FF;                \
        uint16_t lo = (off >> 1) & 0x7FF;                 \
        uint16_t hi_inst = 0xF000 | hi;                   \
        uint16_t lo_inst = 0xB800 | lo;                   \
        volatile uint16_t* p = (volatile uint16_t*)(addr);\
        *p = hi_inst;                                     \
        *(p + 1) = lo_inst;                               \
    } while (0)

#define PATCH_MEM(addr, ...)                                                      \
    do {                                                                          \
        const uint16_t patch_data[] = {__VA_ARGS__};                              \
        volatile uint16_t* p = (volatile uint16_t*)(addr);                        \
        for (size_t i = 0; i < sizeof(patch_data) / sizeof(patch_data[0]); i++) { \
            p[i] = patch_data[i];                                                 \
        }                                                                         \
        arch_clean_invalidate_cache_range((uint32_t)(addr), sizeof(patch_data));  \
    } while (0)

#define PATCH_MEM_ARM(addr, ...)                                                  \
    do {                                                                          \
        const uint32_t patch_data[] = {__VA_ARGS__};                              \
        volatile uint32_t* p = (volatile uint32_t*)(addr);                        \
        for (size_t i = 0; i < sizeof(patch_data) / sizeof(patch_data[0]); i++) { \
            p[i] = patch_data[i];                                                 \
        }                                                                         \
        arch_clean_invalidate_cache_range((uint32_t)(addr), sizeof(patch_data));  \
    } while (0)

#define SEARCH_PATTERN(start_addr, end_addr, ...)                            \
    ({                                                                       \
        static uint16_t pattern[] = {__VA_ARGS__};                           \
        const uint32_t pattern_count = sizeof(pattern) / sizeof(pattern[0]); \
        uint32_t result = 0;                                                 \
                                                                             \
        uint32_t max_addr = end_addr - (pattern_count * 2);                  \
        for (uint32_t offset = start_addr; offset < max_addr; offset += 2) { \
            uint16_t first_val = *(volatile uint16_t*)offset;                \
            if (first_val != pattern[0]) continue;                           \
                                                                             \
            uint32_t i;                                                      \
            for (i = 1; i < pattern_count; i++) {                            \
                uint32_t check_addr = offset + (i * 2);                      \
                uint16_t value = *(volatile uint16_t*)check_addr;            \
                                                                             \
                if (value != pattern[i]) break;                              \
            }                                                                \
                                                                             \
            if (i == pattern_count) {                                        \
                result = offset;                                             \
                break;                                                       \
            }                                                                \
        }                                                                    \
                                                                             \
        result;                                                              \
    })

#define SEARCH_PATTERN_ARM(start_addr, end_addr, ...)                        \
    ({                                                                       \
        static uint32_t pattern[] = {__VA_ARGS__};                           \
        const uint32_t pattern_count = sizeof(pattern) / sizeof(pattern[0]); \
        uint32_t result = 0;                                                 \
                                                                             \
        uint32_t max_addr = end_addr - (pattern_count * 4);                  \
        for (uint32_t offset = start_addr; offset < max_addr; offset += 4) { \
            uint32_t first_val = *(volatile uint32_t*)offset;                \
            if (first_val != pattern[0]) continue;                           \
                                                                             \
            uint32_t i;                                                      \
            for (i = 1; i < pattern_count; i++) {                            \
                uint32_t check_addr = offset + (i * 4);                      \
                uint32_t value = *(volatile uint32_t*)check_addr;            \
                                                                             \
                if (value != pattern[i]) break;                              \
            }                                                                \
                                                                             \
            if (i == pattern_count) {                                        \
                result = offset;                                             \
                break;                                                       \
            }                                                                \
        }                                                                    \
                                                                             \
        result;                                                              \
    })

#define FORCE_RETURN(addr, value)                         \
    do {                                                  \
        PATCH_MEM(addr, 0x2000 | ((value)&0xFF), 0x4770); \
    } while (0)

#define FORCE_RETURN_ARM(addr, value)                                                          \
    do {                                                                                       \
        PATCH_MEM_ARM(addr, 0xE3A00000 | ((value)&0xFF) | ((value) << 4 & 0xF00), 0xE12FFF1E); \
    } while (0)

#define NOP(addr, count)                         \
    do {                                         \
        for (int i = 0; i < (count); i++) {      \
            PATCH_MEM((addr) + (i * 2), 0xBF00); \
        }                                        \
    } while (0)

#define NOP_ARM(addr, count)                             \
    do {                                                 \
        for (int i = 0; i < (count); i++) {              \
            PATCH_MEM_ARM((addr) + (i * 4), 0xE320F000); \
        }                                                \
    } while (0)
