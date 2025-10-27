#include <stdint.h>
#include "mtk_usbdl.h"
#include "../wdt/mtk_wdt.h"

#define BOOT_MISC0_OFFSET           0x0080
#define MISC_LOCK_KEY_OFFSET        0x0100
#define RST_CON_OFFSET              0x0108

#define MISC_LOCK_KEY_MAGIC         0xAD98
#define USBDL_BIT_EN                0x00000001
#define USBDL_BROM                  0x00000002
#define USBDL_TIMEOUT_MASK          0x0000FFFC
#define USBDL_TIMEOUT_MAX           (USBDL_TIMEOUT_MASK >> 2)
#define USBDL_MAGIC                 0x444C0000

#define USBDL_FLAG                  (CONFIG_SECURITY_AO_BASE + BOOT_MISC0_OFFSET)
#define MISC_LOCK_KEY               (CONFIG_SECURITY_AO_BASE + MISC_LOCK_KEY_OFFSET)
#define RST_CON                     (CONFIG_SECURITY_AO_BASE + RST_CON_OFFSET)

void mtk_set_boot_mode(uint32_t mode, uint32_t timeout_ms) {
    uint32_t usbdl_reg = 0;
    uint32_t timeout;
    
    timeout = timeout_ms ? timeout_ms / 1000 : USBDL_TIMEOUT_MAX;
    timeout <<= 2;
    timeout &= USBDL_TIMEOUT_MASK;
    
    usbdl_reg |= timeout;
    
    if (mode) {
        usbdl_reg |= USBDL_BIT_EN;
    } else {
        usbdl_reg &= ~USBDL_BIT_EN;
    }
    
    usbdl_reg &= ~USBDL_BROM;
    usbdl_reg |= USBDL_MAGIC;
    
    *(volatile uint32_t*)MISC_LOCK_KEY = MISC_LOCK_KEY_MAGIC;
    *(volatile uint32_t*)RST_CON |= 1;
    *(volatile uint32_t*)MISC_LOCK_KEY = 0;
    
    *(volatile uint32_t*)USBDL_FLAG = usbdl_reg;
}

void mtk_reboot_emergency(void) {
    mtk_set_boot_mode(1, 0);
    mtk_wdt_reset();
}