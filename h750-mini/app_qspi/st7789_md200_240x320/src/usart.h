#ifndef __USART_H__
#define __USART_H__

/* Minimal header so lcd_spi_200.h can be reused unchanged: the driver only
 * needs the UART_HandleTypeDef symbol (it is never referenced, so no
 * definition is required). The real console handle is static inside the
 * board layer (board/uart_printf.c). */

#include "stm32h7xx_hal.h"

extern UART_HandleTypeDef huart1;

#endif /* __USART_H__ */
