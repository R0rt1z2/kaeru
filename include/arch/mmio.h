/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Shomy, R0rt1z2
 */

#pragma once

#include <stdint.h>

#define dsb(option) __asm__ __volatile__ ("dsb " #option : : : "memory")
#define dmb(option) __asm__ __volatile__ ("dmb " #option : : : "memory")
#define isb()       __asm__ volatile("isb" ::: "memory")

#define mb()        dsb(sy)
#define rmb()       dsb(sy)
#define wmb()       dsb(sy)

#define smp_mb()    dmb(ish)
#define smp_rmb()   dmb(ish)
#define smp_wmb()   dmb(ish)

#define __raw_readb(addr) \
    ({ uint8_t __v = (*(volatile uint8_t *)(uintptr_t)(addr)); __v; })

#define __raw_readw(addr) \
    ({ uint16_t __v = (*(volatile uint16_t *)(uintptr_t)(addr)); __v; })

#define __raw_readl(addr) \
    ({ uint32_t __v = (*(volatile uint32_t *)(uintptr_t)(addr)); __v; })

#define __raw_writeb(val, addr) \
    ({ (*(volatile uint8_t *)(uintptr_t)(addr)) = (val); })

#define __raw_writew(val, addr) \
    ({ (*(volatile uint16_t *)(uintptr_t)(addr)) = (val); })

#define __raw_writel(val, addr) \
    ({ (*(volatile uint32_t *)(uintptr_t)(addr)) = (val); })

#define readb_relaxed(addr) \
    ({ uint8_t __v = __raw_readb(addr); __asm__ volatile("" ::: "memory"); __v; })

#define readw_relaxed(addr) \
    ({ uint16_t __v = __raw_readw(addr); __asm__ volatile("" ::: "memory"); __v; })

#define readl_relaxed(addr) \
    ({ uint32_t __v = __raw_readl(addr); __asm__ volatile("" ::: "memory"); __v; })

#define writeb_relaxed(val, addr) \
    ({ __asm__ volatile("" ::: "memory"); __raw_writeb(val, addr); })

#define writew_relaxed(val, addr) \
    ({ __asm__ volatile("" ::: "memory"); __raw_writew(val, addr); })

#define writel_relaxed(val, addr) \
    ({ __asm__ volatile("" ::: "memory"); __raw_writel(val, addr); })

#define readb(addr) \
    ({ uint8_t __v = __raw_readb(addr); rmb(); __v; })

#define readw(addr) \
    ({ uint16_t __v = __raw_readw(addr); rmb(); __v; })

#define readl(addr) \
    ({ uint32_t __v = __raw_readl(addr); rmb(); __v; })

#define writeb(val, addr) \
    ({ wmb(); __raw_writeb(val, addr); })

#define writew(val, addr) \
    ({ wmb(); __raw_writew(val, addr); })

#define writel(val, addr) \
    ({ wmb(); __raw_writel(val, addr); })

#define setbits_8(addr, set) \
    writeb_relaxed(readb_relaxed(addr) | (set), addr)

#define setbits_16(addr, set) \
    writew_relaxed(readw_relaxed(addr) | (set), addr)

#define setbits_32(addr, set) \
    writel_relaxed(readl_relaxed(addr) | (set), addr)

#define clrbits_8(addr, clr) \
    writeb_relaxed(readb_relaxed(addr) & ~(clr), addr)

#define clrbits_16(addr, clr) \
    writew_relaxed(readw_relaxed(addr) & ~(clr), addr)

#define clrbits_32(addr, clr) \
    writel_relaxed(readl_relaxed(addr) & ~(clr), addr)

#define clrsetbits_8(addr, clr, set) \
    writeb_relaxed((readb_relaxed(addr) & ~(clr)) | (set), addr)

#define clrsetbits_16(addr, clr, set) \
    writew_relaxed((readw_relaxed(addr) & ~(clr)) | (set), addr)

#define clrsetbits_32(addr, clr, set) \
    writel_relaxed((readl_relaxed(addr) & ~(clr)) | (set), addr)

#define setbits(addr, set)          setbits_32(addr, set)
#define clrbits(addr, clr)          clrbits_32(addr, clr)
#define clrsetbits(addr, clr, set)  clrsetbits_32(addr, clr, set)

#define readl_poll_timeout(addr, val, cond, timeout_us) \
({ \
    uint32_t __timeout = (timeout_us); \
    int __ret = -1; \
    for (uint32_t __i = 0; __i < __timeout; __i++) { \
        (val) = readl(addr); \
        if (cond) { \
            __ret = 0; \
            break; \
        } \
        for (volatile int __d = 0; __d < 100; __d++); \
    } \
    __ret; \
})

#define GENMASK(h, l) \
    (((~0UL) << (l)) & (~0UL >> (31 - (h))))

#define FIELD_PREP(mask, val) \
    (((val) << (__builtin_ctzl(mask))) & (mask))

#define FIELD_GET(mask, reg) \
    (((reg) & (mask)) >> (__builtin_ctzl(mask)))

#define BIT(n)              (1UL << (n))

#define swab16(x) \
    ((uint16_t)( \
        (((uint16_t)(x) & 0x00ffU) << 8) | \
        (((uint16_t)(x) & 0xff00U) >> 8)))

#define swab32(x) \
    ((uint32_t)( \
        (((uint32_t)(x) & 0x000000ffUL) << 24) | \
        (((uint32_t)(x) & 0x0000ff00UL) <<  8) | \
        (((uint32_t)(x) & 0x00ff0000UL) >>  8) | \
        (((uint32_t)(x) & 0xff000000UL) >> 24)))

#define cpu_to_le16(x)  (x)
#define cpu_to_le32(x)  (x)
#define le16_to_cpu(x)  (x)
#define le32_to_cpu(x)  (x)

#define cpu_to_be16(x)  swab16(x)
#define cpu_to_be32(x)  swab32(x)
#define be16_to_cpu(x)  swab16(x)
#define be32_to_cpu(x)  swab32(x)