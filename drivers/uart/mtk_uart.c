#include <stdint.h>
#include "mtk_uart.h"

#define UART_THR_OFFSET     0x00
#define UART_LSR_OFFSET     0x14
#define UART_LSR_THRE       0x20

void mtk_uart_putc(int ch) {
    while (!(*(volatile uint32_t*)(CONFIG_UART_BASE + UART_LSR_OFFSET) & UART_LSR_THRE))
        ;
    *(volatile uint32_t*)(CONFIG_UART_BASE + UART_THR_OFFSET) = ch;
}