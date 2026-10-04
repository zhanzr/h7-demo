/**
  * @file    uart_printf.c
  * @brief   USART1 printf implementation for the h750-mini board.
  *
  * USART1 is on PA9 (TX) / PA10 (RX), AF7, and is wired to the on-board CH340
  * USB-serial bridge (COM56 on this PC). Output is 115200 8-N-1, blocking
  * (polled) so nothing is dropped. USART1 clock source: D2PCLK2.
  */

#include "uart_printf.h"
#include "board.h"
#include "stm32h7xx_hal.h"

#include <stdio.h>
#include <stdarg.h>

static UART_HandleTypeDef huart1;

/* ------------------------------------------------------------------------ */
/* printf() replacement for armclang builds (see cmake/printf_rename.h).
 * armclang would otherwise turn printf into ARMCLIB's __2printf ABI, which
 * cannot be linked against newlib. vprintf() is not specialized by armclang,
 * so this thin wrapper keeps the standard printf() working over the UART. */
int bench_printf(const char *fmt, ...)
{
    va_list args;
    int r;

    va_start(args, fmt);
    r = vprintf(fmt, args);
    va_end(args);
    return r;
}

/* ------------------------------------------------------------------------ */
void UART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /* USART1 kernel clock: D2PCLK2 (120 MHz). */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9 = USART1_TX, PA10 = USART1_RX (AF7). */
    GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200U;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/* ------------------------------------------------------------------------ */
int UART_PutChar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart1, &c, 1U, 1000U);
    return ch;
}

/* ------------------------------------------------------------------------ */
int UART_GetChar(void)
{
    uint8_t c;
    /* A streaming host can overrun the RX FIFO while the CPU is busy; the ORE/
     * FE flags then latch and every subsequent HAL_UART_Receive() fails. Clear
     * them on each poll so a latched error cannot wedge the RX path. */
    __HAL_UART_CLEAR_PEFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);
    __HAL_UART_CLEAR_OREFLAG(&huart1);

    if (HAL_UART_Receive(&huart1, &c, 1U, 100U) == HAL_OK)
    {
        return c;
    }
    return -1;
}
