#pragma once

#include <stdint.h>
#include <arch/mmio.h>

#define USBDL_FLAG      (CONFIG_SECURITY_AO_BASE + 0x0080)
#define MISC_LOCK_KEY   (CONFIG_SECURITY_AO_BASE + 0x0100)
#define RST_CON         (CONFIG_SECURITY_AO_BASE + 0x0108)

#define MISC_LOCK_KEY_MAGIC     0xAD98
#define USBDL_BIT_EN            BIT(0)
#define USBDL_BROM              BIT(1)
#define USBDL_TIMEOUT_MASK      GENMASK(15, 2)
#define USBDL_TIMEOUT_MAX       (USBDL_TIMEOUT_MASK >> 2)
#define USBDL_MAGIC             0x444C0000

void mtk_set_boot_mode(uint32_t mode, uint32_t timeout_ms);
void mtk_reboot_emergency(void);