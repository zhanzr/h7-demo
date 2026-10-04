/**
  * @file    main.c
  * @brief   hse_test - check whether the 25 MHz HSE crystal on the h750-mini
  *          actually locks. Runs entirely from the internal HSI (64 MHz) so it
  *          works even if the HSE is dead. Reports HSE status every 2 s.
  */

#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32h7xx_hal.h"

int main(void)
{
    HAL_Init();
    __enable_irq();
    UART_Init();

    printf("\r\n=== HSE test on STM32H750VB @ %lu Hz (HSI 64 MHz, no PLL) ===\r\n",
           (unsigned long)SystemCoreClock);

    while (1)
    {
        /* Enable the 25 MHz external oscillator and wait (bounded) for HSERDY. */
        __HAL_RCC_HSE_CONFIG(RCC_HSE_ON);
        uint32_t t = 2000000u;          /* ~2 s at 64 MHz */
        while (!__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) && t--) { }

        uint32_t cr = RCC->CR;
        if (__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY))
        {
            printf("HSE: READY - crystal OK (RCC_CR=0x%08lx)\r\n", (unsigned long)cr);
        }
        else
        {
            printf("HSE: FAIL - HSERDY never set after ~2 s (RCC_CR=0x%08lx)\r\n",
                   (unsigned long)cr);
        }
        __HAL_RCC_HSE_CONFIG(RCC_HSE_OFF);
        HAL_Delay(2000);
    }

    return 0;
}
