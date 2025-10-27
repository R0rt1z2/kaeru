#pragma once

#include <stdint.h>

void mtk_set_boot_mode(uint32_t mode, uint32_t timeout_ms);
void mtk_reboot_emergency(void);