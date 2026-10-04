#ifndef __MAIN_H__
#define __MAIN_H__

#include "stm32h7xx_hal.h"

/* SPI4 (ST7789 panel) - AF5 on GPIOE */
#define LCD_CS_Pin        GPIO_PIN_11
#define LCD_CS_GPIO_Port  GPIOE
#define LCD_SCL_Pin       GPIO_PIN_12
#define LCD_SCL_GPIO_Port GPIOE
#define LCD_SDA_Pin       GPIO_PIN_14
#define LCD_SDA_GPIO_Port GPIOE

/* TIM4_CH4 backlight PWM - AF2 on PD15 */
#define LCD_BL_Pin        GPIO_PIN_15
#define LCD_BL_GPIO_Port  GPIOD

/* On-board LED: LD3 on PA8 */
#define LED_Pin           GPIO_PIN_8
#define LED_GPIO_Port     GPIOA

#endif /* __MAIN_H__ */
