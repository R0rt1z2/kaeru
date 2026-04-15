#pragma once

#include <stdint.h>
#include <arch/mmio.h>

#define MTK_WDT_MODE        (CONFIG_WDT_BASE + 0x00)
#define MTK_WDT_LENGTH      (CONFIG_WDT_BASE + 0x04)
#define MTK_WDT_RESTART     (CONFIG_WDT_BASE + 0x08)
#define MTK_WDT_STATUS      (CONFIG_WDT_BASE + 0x0C)
#define MTK_WDT_INTERVAL    (CONFIG_WDT_BASE + 0x10)
#define MTK_WDT_SWRST       (CONFIG_WDT_BASE + 0x14)

#define MTK_WDT_MODE_KEY          0x22000000
#define MTK_WDT_MODE_ENABLE       BIT(0)
#define MTK_WDT_MODE_EXT_POL      BIT(1)
#define MTK_WDT_MODE_EXTEN        BIT(2)
#define MTK_WDT_MODE_IRQ          BIT(3)
#define MTK_WDT_MODE_AUTO_RESTART BIT(4)
#define MTK_WDT_MODE_DUAL_MODE    BIT(6)

#define MTK_WDT_LENGTH_KEY   0x08
#define MTK_WDT_RESTART_KEY  0x1971
#define MTK_WDT_SWRST_KEY   0x1209

void mtk_wdt_reset(void);
void mtk_wdt_disable(void);