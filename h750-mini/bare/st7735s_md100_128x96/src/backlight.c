/*
  backlight.c - ST7735S backlight (BL = PD15, J1B pin 18) PWM via TIM4_CH4.
  PD15 is TIM4_CH4 (AF2). TIM4 is on APB1; with the h750-mini 480 MHz clock
  tree, APB1 = 120 MHz and the timer clock is x2 = 240 MHz. A PSC/ARR of
  239/999 gives a 1 kHz PWM; CCR4 = 0..999 scales 0..100% (default 10%).

  Plain register setup (no HAL TIM module needed for this small demo).
*/

#include "backlight.h"
#include "lcd.h"

void Backlight_Init(void)
{
    GPIO_InitTypeDef g;

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* PD15 = TIM4_CH4, alternate function push-pull. */
    g.Pin   = LCD_BL_Pin;
    g.Mode  = GPIO_MODE_AF_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(LCD_GPIO_PortBL, &g);

    /* 1 kHz PWM: timer clock 240 MHz / (PSC+1) / (ARR+1). */
    TIM4->CR1  = 0;                    /* counter disabled while configuring */
    TIM4->PSC  = 239;
    TIM4->ARR  = 999;
    TIM4->CCR4 = 100;                  /* 10% at init (the project default;
                                          main() may re-set it per pass) */
    TIM4->CCMR2 |= (TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2)  /* PWM mode 1 */
                 | TIM_CCMR2_OC4PE;    /* preload enabled */
    TIM4->CCER  |= TIM_CCER_CC4E;      /* CH4 output enable */
    TIM4->CR1   |= TIM_CR1_CEN;        /* start the counter */
}

void Backlight_SetDuty(uint16_t percent)
{
    if (percent > 100U)
    {
        percent = 100U;
    }
    TIM4->CCR4 = (uint32_t)percent * (TIM4->ARR + 1U) / 100U;
}

uint16_t Backlight_GetDuty(void)
{
    return (uint16_t)((TIM4->CCR4 * 100U) / (TIM4->ARR + 1U));
}
