/**
  * @file    system_app.c
  * @brief   Non-destructive system init for the qspi_map QSPI app.
  *
  * The stock ST system_stm32h7xx.c SystemInit() resets the whole RCC clock
  * tree (turns off PLL1/PLL2/PLL3, HSE, resets dividers). That would kill the
  * PLL2 -> QUADSPI clock the bootloader configured, and because this app's code
  * and vector table live in the QSPI memory-mapped space, the very next
  * instruction fetch would fault.
  *
  * So the app provides its own SystemInit()/ExitRun0Mode() that do nothing
  * destructive, and a SystemCoreClock that reflects what the bootloader set
  * (480 MHz core). The bootloader owns the clock tree; the app only uses it.
  */

#include "stm32h7xx.h"

/* Bootloader left the core at 480 MHz (PLL1, HSE 25 MHz, M5 N192 P2). */
uint32_t SystemCoreClock = 480000000U;
uint32_t SystemD2Clock  = 240000000U;

/* Referenced by HAL_RCC_GetHCLKFreq()/GetPCLKx()Freq(). */
const uint8_t D1CorePrescTable[16] = {0, 0, 0, 0, 1, 2, 3, 4, 1, 2, 3, 4, 6, 7, 8, 9};

/* Called by the reset handler before main. Deliberately leaves RCC/PLL2/QSPI
 * untouched. */
void SystemInit(void)
{
    SystemCoreClock = 480000000U;
    SystemD2Clock  = 240000000U;
}

/* Called by the reset handler before SystemInit. The bootloader already
 * configured the power supply (LDO, VOS scale 0) - nothing to do. */
void ExitRun0Mode(void)
{
}

/* HAL clock helpers may call this; the clock tree is owned by the bootloader,
 * so just report the known values. */
void SystemCoreClockUpdate(void)
{
    SystemCoreClock = 480000000U;
    SystemD2Clock  = 240000000U;
}
