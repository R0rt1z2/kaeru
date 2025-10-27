#include <stdint.h>
#include "mtk_wdt.h"

void mtk_wdt_reset(void) {
    volatile uint32_t *wdt_regs = (volatile uint32_t *)CONFIG_WDT_BASE;
    wdt_regs[6] = 0x1971;
    wdt_regs[0] = 0x22000014;
    wdt_regs[5] = 0x1209;
}