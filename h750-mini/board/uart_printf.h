#ifndef __UART_PRINTF_H__
#define __UART_PRINTF_H__

void UART_Init(void);
int  UART_PutChar(int ch);
int  UART_GetChar(void);   /* blocking RX, 100 ms timeout, -1 on timeout */

/*
 * printf() replacement for armclang builds (see cmake/printf_rename.h).
 * Defined in uart_printf.c; forwards to newlib vprintf() so output reaches
 * the UART through _write() -> UART_PutChar().
 */
int bench_printf(const char *fmt, ...);

#endif /* __UART_PRINTF_H__ */
