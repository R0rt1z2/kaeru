#include <stdint.h>
#include "mtk_wdt.h"

void mtk_wdt_reset(void) {
    /* first kick the watchdog to ensure it's alive */
    wdt_write(MTK_WDT_RESTART, MTK_WDT_RESTART_KEY);
    
    /* configure for immediate reset: enable external signal and IRQ mode
     * but DON'T enable the watchdog timer itself, we want SW reset */
    wdt_write(MTK_WDT_MODE, MTK_WDT_MODE_KEY | MTK_WDT_MODE_EXTEN | MTK_WDT_MODE_IRQ);
    
    /* trigger the actual software reset */
    wdt_write(MTK_WDT_SWRST, MTK_WDT_SWRST_KEY);
}

void mtk_wdt_disable(void) {
    /* read current mode register value */
    uint32_t mode = wdt_read(MTK_WDT_MODE);
    
    /* clear the enable bit to disable watchdog */
    mode &= ~MTK_WDT_MODE_ENABLE;
    
    /* add the magic key (required for any mode register write) */
    mode |= MTK_WDT_MODE_KEY;
    
    /* write back to disable the watchdog */
    wdt_write(MTK_WDT_MODE, mode);
}