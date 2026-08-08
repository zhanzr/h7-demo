/**
  * @file    tim.c
  * @brief   TIM23_CH1 PWM for the LCD backlight (PG12, AF13).
  *
  * TIM23 is on D2 APB1 (kernel clock = 2 x PCLK1 = 275 MHz); ARR = 65535,
  * prescaler 0 -> ~4.2 kHz PWM. Duty is set with lcd_bl_bright_set(duty)
  * (0..65535); the LCD driver starts the PWM during SPI_LCD_Init().
  */

#include "tim.h"
#include "main.h"
#include "board.h"

TIM_HandleTypeDef htim23;

/* ------------------------------------------------------------------------ */
void MX_TIM23_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim23.Instance               = TIM23;
    htim23.Init.Prescaler         = 0;
    htim23.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim23.Init.Period            = 65535;
    htim23.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim23.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim23) != HAL_OK)
    {
        Error_Handler();
    }

    sConfigOC.OCMode     = TIM_OCMODE_PWM1;
    sConfigOC.Pulse      = 32768;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim23, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_TIM_MspPostInit(&htim23);
}

/* ------------------------------------------------------------------------ */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *tim_pwmHandle)
{
    if (tim_pwmHandle->Instance == TIM23)
    {
        __HAL_RCC_TIM23_CLK_ENABLE();
    }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (timHandle->Instance == TIM23)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();

        /* PG12 -> TIM23_CH1 (backlight PWM, AF13). */
        GPIO_InitStruct.Pin       = GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF13_TIM23;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    }
}
