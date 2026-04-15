#include "mtk_uart.h"

void mtk_uart_putc(int ch) {
    while (!(readl(UART_LSR) & UART_LSR_THRE))
        ;
    writel(ch, UART_THR);
}