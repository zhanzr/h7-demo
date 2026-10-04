/**
  * @file    main.c
  * @brief   qspi_map stage-2 app - runs entirely from the QSPI-mapped space.
  *
  * The bootloader (internal flash) has already:
  *   - configured the 480 MHz clock tree (PLL1) and PLL2 for the QUADSPI,
  *   - put the QUADSPI into memory-mapped mode (0xEB 1-4-4),
  *   - set up the MPU so 0x90000000 is cacheable + executable,
  *   - jumped to this app's reset vector at 0x90000000.
  *
  * This app therefore must NOT reconfigure the system clocks / PLL2 or the
  * QUADSPI (its own code lives there). It only re-inits the UART console and
  * LED, prints a banner proving it runs from QSPI, and blinks like blink_hello.
  */

#include <stdio.h>

#include "board.h"
#include "uart_printf.h"

#define APP_BASE  0x90000000UL
#define LED_PORT  GPIOA
#define LED_PIN   GPIO_PIN_8

static void LED_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);

    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

int main(void)
{
    HAL_Init();          /* SysTick only; clocks are owned by the bootloader  */
    __enable_irq();      /* the bootloader jumped with PRIMASK set            */
    UART_Init();         /* re-init the console; does not touch PLL2/QUADSPI  */
    LED_Init();

    printf("\r\n=== qspi_map app: RUNNING FROM QSPI FLASH @ 0x%08lx ===\r\n",
           (unsigned long)APP_BASE);
    printf("    booted at %lu Hz (SystemCoreClock), code at 0x%08lx\r\n",
           (unsigned long)SystemCoreClock,
           (unsigned long)(uintptr_t)&main);

    while (1)
    {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        printf("LED PA8: %s @ %lu Hz (running from QSPI flash)\r\n",
               HAL_GPIO_ReadPin(LED_PORT, LED_PIN) == GPIO_PIN_SET ? "ON" : "OFF",
               (unsigned long)SystemCoreClock);
        HAL_Delay(1000);
    }

    return 0;
}
