/**
  * @file    gpio.c
  * @brief   Static GPIO: on-board LED (PG7, low active).
  *
  * The LCD control pins (SPI6 PG8/13/14, backlight PG12, DC PG15) are all
  * configured by the LCD driver's HAL_SPI_MspInit().
  */

#include "gpio.h"
#include "main.h"
#include "board.h"

/* ------------------------------------------------------------------------ */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* On-board LED on PG7 (low active). */
    GPIO_InitStruct.Pin   = LED_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}
