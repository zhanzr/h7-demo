/*
  backlight.h - ST7735S backlight (BL = PD15) PWM driver.
  PD15 is TIM4_CH4 (AF2) on the J1B header (pin 18). TIM4 is clocked from
  APB1; at the h750-mini 480 MHz clock tree (APB1 = 120 MHz, timer x2 =
  240 MHz) a 1 kHz PWM is PSC=239, ARR=999, and CCR4 scales 0..100 %
  (default 10%).
*/

#ifndef __BACKLIGHT_H
#define __BACKLIGHT_H

#include <stdint.h>

void Backlight_Init(void);
void Backlight_SetDuty(uint16_t percent);
uint16_t Backlight_GetDuty(void);      /* current duty in percent */

#endif /* __BACKLIGHT_H */
