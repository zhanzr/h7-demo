#include <stdio.h>

#include "board.h"
#include "uart_printf.h"

/* On-board LED: LD3 on PA8 (see cubemx_file/Core/Inc/main.h). */
#define LED_PORT GPIOA
#define LED_PIN  GPIO_PIN_8

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
    HAL_Init();
    Board_Init();
    LED_Init();

    printf("\r\n=== blink_hello on STM32H750VBTx @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    while (1)
    {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        printf("LED PA8: %s @ %lu Hz\r\n",
               HAL_GPIO_ReadPin(LED_PORT, LED_PIN) == GPIO_PIN_SET ? "ON" : "OFF",
               (unsigned long)SystemCoreClock);
        HAL_Delay(1000);
    }

    return 0;
}
