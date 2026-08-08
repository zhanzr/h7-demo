/**
  * @file    board.c
  * @brief   Board init for the h723-mini (STM32H723ZGT6) benchmark builds.
  *
  * Clock tree (HSE = 25 MHz), copied verbatim from the vendor LED example
  * (1.LED闪烁, LX 小周 STM32H723ZGT6 核心板):
  *   PLL1 M=10 -> PLL input 2.5 MHz
  *   PLL1 N=220 -> VCO 550 MHz
  *   PLL1 P=1 -> SYSCLK (core clock) 550 MHz
  *   AHB prescaler 2 -> HCLK 275 MHz
  *   APB1/2/3/4 prescaler 2 -> 137.5 MHz
  *   Flash latency 3, LDO supply, VOS scale 0.
  *
  * Console: USART1 (PA9 TX / PA10 RX, AF7) @ 115200 8-N-1 to COM3. I/D caches
  * enabled. SWV/ITM is enabled in firmware but the ULINK2 cannot capture SWO,
  * so the UART is the working console.
  */

#include "board.h"
#include "uart_printf.h"
#include "swv_printf.h"

/* ------------------------------------------------------------------------ */
/* Same MPU setup the h750-mini builds use: a 4 GB region with subregions
 * 0/1/2/7 disabled, leaving FLASH/DTCM/AXI SRAM on the privileged default
 * memory map (Normal, write-back, cacheable) and denying access to the rest
 * (including the 0x90000000 OCTOSPI memory-map region, which these projects
 * do not use). Kept so the benchmark cache behaviour matches the h750 port. */
static void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress      = 0x0;
    MPU_InitStruct.Size             = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/* ------------------------------------------------------------------------ */
/* Clock tree as in the vendor 550 MHz example (HSE 25 MHz -> PLL1 M=10 N=220
 * P=1 -> 550 MHz SYSCLK, HCLK 275 MHz, APB1/2/3/4 137.5 MHz, VOS scale 0,
 * flash latency 3). */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Supply configuration update enable. */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /* Main internal regulator output voltage: scale 0 (1.35 V, needed for
     * 550 MHz). */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 10;
    RCC_OscInitStruct.PLL.PLLN       = 220;
    RCC_OscInitStruct.PLL.PLLP       = 1;
    RCC_OscInitStruct.PLL.PLLQ       = 2;
    RCC_OscInitStruct.PLL.PLLR       = 2;
    RCC_OscInitStruct.PLL.PLLRGE     = RCC_PLL1VCIRANGE_1;
    RCC_OscInitStruct.PLL.PLLVCOSEL  = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN   = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
void Board_Init(void)
{
    MPU_Config();
    SCB_EnableICache();
    SCB_EnableDCache();

    SystemClock_Config();
    UART_Init();
    SWV_Init();
}

/* ------------------------------------------------------------------------ */
/* HAL tick source. The startup weak-handler default is an infinite loop, so
 * without this the SysTick (enabled by HAL_Init) wedges the core the moment
 * the first tick fires. */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ------------------------------------------------------------------------ */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
