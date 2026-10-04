#ifndef __TIM_H__
#define __TIM_H__

#include "stm32h7xx_hal.h"

extern TIM_HandleTypeDef htim4;

void MX_TIM4_Init(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timHandle);

#endif /* __TIM_H__ */
