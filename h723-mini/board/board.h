#ifndef __BOARD_H__
#define __BOARD_H__

#include "stm32h7xx_hal.h"

/* Init: caches + 550 MHz clocks + USART1 console (PA9/PA10) + ITM. */
void Board_Init(void);
void SystemClock_Config(void);  /* HSE 25 MHz -> PLL1 -> 550 MHz core / 275 MHz HCLK */
void Error_Handler(void);

#endif /* __BOARD_H__ */
