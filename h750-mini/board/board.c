/**
  * @file    board.c
  * @brief   Board init for the h750-mini (STM32H750VBTx) benchmark builds.
  *
  * Clock tree (HSE = 25 MHz):
  *   PLL1 M=5 -> PLL input 5 MHz
  *   PLL1 N=192 -> VCO 960 MHz
  *   PLL1 P=2 -> SYSCLK (core clock) 480 MHz
  *   AHB prescaler 2 -> HCLK 240 MHz
  *   APB1/2/3/4 prescaler 2 -> 120 MHz
  *   Flash latency 4, LDO supply, VOS scale 0.
  *
  * Console: USART1 (PA9 TX / PA10 RX, AF7) @ 115200 8-N-1 via the on-board
  * CH340 bridge (COM56). I/D caches enabled (data lives in DTCM, no MPU
  * needed). SWV/ITM is enabled in firmware but the ULINK2 cannot capture SWO,
  * so the UART is the working console.
  */

#include "board.h"
#include "uart_printf.h"
#include "swv_printf.h"

/* ------------------------------------------------------------------------ */
/* Same MPU setup the CubeMX/Keil project uses (see its main.c): a 4 GB
 * region with subregions 0/1/2/7 disabled, leaving FLASH/DTCM/AXI SRAM on
 * the privileged default memory map (Normal, write-back, cacheable) and
 * denying access to the rest. Kept so the benchmark cache behaviour matches
 * the working ov5640_to_st7789 build exactly. */
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
/* QSPI apps (QSPI_APP defined): the bootloader owns the clock tree, so an
 * empty SystemClock_Config keeps the (identical) app main() callable without
 * re-configuring RCC/PLL2 and killing the QSPI clock the code runs from. */
#ifdef QSPI_APP
void SystemClock_Config(void)
{
    /* no-op: h750_boot already configured HSE -> PLL1 -> 480 MHz + PLL2/QUADSPI */
}
#else
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Pre-program 4 flash wait states (needed for 480 MHz SYSCLK) with a
     * barrier so the latency set+readback inside HAL_RCC_ClockConfig cannot
     * race on the M7 device bus (same workaround as ov5640_to_st7789). */
    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_4);
    __DSB();
    __ISB();

    /* Supply configuration update enable. */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /* Main internal regulator output voltage: scale 0 (1.35 V). */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 5;
    RCC_OscInitStruct.PLL.PLLN       = 192;
    RCC_OscInitStruct.PLL.PLLP       = 2;
    RCC_OscInitStruct.PLL.PLLQ       = 4;
    RCC_OscInitStruct.PLL.PLLR       = 2;
    RCC_OscInitStruct.PLL.PLLRGE     = RCC_PLL1VCIRANGE_2;
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

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}
#endif /* QSPI_APP */

/* ------------------------------------------------------------------------ */
void Board_Init(void)
{
#ifndef QSPI_APP
    MPU_Config();
#else
    /* h750_boot disables IRQ before jumping to the app; re-enable it here so
     * HAL_GetTick()/HAL_Delay() (SysTick IRQ) work. */
    __enable_irq();
#endif
    SCB_EnableICache();
    SCB_EnableDCache();

    SystemClock_Config();   /* no-op for QSPI apps */
    UART_Init();
    SWV_Init();
}

/* ------------------------------------------------------------------------ */
/* HAL tick source. The startup weak-handler default is an infinite loop, so
 * without this the SysTick (enabled by HAL_Init) wedges the core the moment
 * the first tick fires. Mirrors stm32h7xx_it.c in the ov5640 project. */
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
