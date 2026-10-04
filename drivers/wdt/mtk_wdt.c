#include "mtk_wdt.h"

void mtk_wdt_reset(void) {
    /* first kick the watchdog to ensure it's alive */
    writel(MTK_WDT_RESTART_KEY, MTK_WDT_RESTART);

    /* configure for a hardware reset: external signal plus auto restart so
     * the device comes back without waiting on the power key. IRQ mode must
     * stay clear, otherwise SWRST only raises an interrupt and we just hang */
    writel(MTK_WDT_MODE_KEY | MTK_WDT_MODE_EXTEN | MTK_WDT_MODE_AUTO_RESTART, MTK_WDT_MODE);

    /* trigger the actual software reset */
    writel(MTK_WDT_SWRST_KEY, MTK_WDT_SWRST);
}

void mtk_wdt_disable(void) {
    /* read current mode register value */
    uint32_t mode = readl(MTK_WDT_MODE);

    /* clear the enable bit to disable watchdog */
    mode &= ~MTK_WDT_MODE_ENABLE;

    /* add the magic key (required for any mode register write) */
    mode |= MTK_WDT_MODE_KEY;

    /* write back to disable the watchdog */
    writel(mode, MTK_WDT_MODE);
}