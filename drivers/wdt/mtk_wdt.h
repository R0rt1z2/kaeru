#pragma once

#define MTK_WDT_MODE        0x00
#define MTK_WDT_LENGTH      0x04
#define MTK_WDT_RESTART     0x08
#define MTK_WDT_STATUS      0x0C
#define MTK_WDT_INTERVAL    0x10
#define MTK_WDT_SWRST       0x14

#define MTK_WDT_MODE_KEY        0x22000000
#define MTK_WDT_MODE_ENABLE     (1 << 0)
#define MTK_WDT_MODE_EXT_POL    (1 << 1)
#define MTK_WDT_MODE_EXTEN      (1 << 2)
#define MTK_WDT_MODE_IRQ        (1 << 3)
#define MTK_WDT_MODE_AUTO_RESTART (1 << 4)
#define MTK_WDT_MODE_DUAL_MODE  (1 << 6)

#define MTK_WDT_LENGTH_KEY      0x08
#define MTK_WDT_RESTART_KEY     0x1971
#define MTK_WDT_SWRST_KEY       0x1209

static inline void wdt_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)(CONFIG_WDT_BASE + offset) = value;
}

static inline uint32_t wdt_read(uint32_t offset) {
    return *(volatile uint32_t *)(CONFIG_WDT_BASE + offset);
}

void mtk_wdt_reset(void);
void mtk_wdt_disable(void);