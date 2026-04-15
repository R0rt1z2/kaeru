#pragma once

#include <arch/mmio.h>

#define UART_THR    (CONFIG_UART_BASE + 0x00)
#define UART_LSR    (CONFIG_UART_BASE + 0x14)

#define UART_LSR_THRE   BIT(5)

void mtk_uart_putc(int ch);