#include "mtk_usbdl.h"
#include "../wdt/mtk_wdt.h"

void mtk_set_boot_mode(uint32_t mode, uint32_t timeout_ms) {
    uint32_t usbdl_reg = 0;
    uint32_t timeout;

    timeout = timeout_ms ? timeout_ms / 1000 : USBDL_TIMEOUT_MAX;
    timeout <<= 2;
    timeout &= USBDL_TIMEOUT_MASK;

    usbdl_reg |= timeout;

    if (mode)
        usbdl_reg |= USBDL_BIT_EN;
    else
        usbdl_reg &= ~USBDL_BIT_EN;

    usbdl_reg &= ~USBDL_BROM;
    usbdl_reg |= USBDL_MAGIC;

    writel(MISC_LOCK_KEY_MAGIC, MISC_LOCK_KEY);
    setbits(RST_CON, BIT(0));
    writel(0, MISC_LOCK_KEY);

    writel(usbdl_reg, USBDL_FLAG);
}

void mtk_reboot_emergency(void) {
    mtk_set_boot_mode(1, 0);
    mtk_wdt_reset();
}