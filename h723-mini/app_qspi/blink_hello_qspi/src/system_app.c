/**
  * @file    system_app.c
  * @brief   Non-destructive system init for the h723-mini QSPI app.
  *
  * The stock ST system_stm32h7xx.c SystemInit() resets the whole RCC clock
  * tree (turns off PLL1/PLL2/PLL3, HSE, resets dividers). That would kill the
  * OSPI clock the bootloader configured, and because this app's code and
  * vector table live in the OSPI memory-mapped space (0x90000000), the very
  * next instruction fetch would fault.
  *
  * So the app provides its own SystemInit() that does nothing destructive, and
  * a SystemCoreClock that reflects what the bootloader set (550 MHz core). The
  * bootloader owns the clock tree; the app only uses it. (The H723 startup does
  * not call ExitRun0Mode, so none is provided.)
  */

#include "stm32h7xx.h"

/* Bootloader left the core at 550 MHz (PLL1, HSE 25 MHz, M10 N220 P1). */
uint32_t SystemCoreClock = 550000000U;
uint32_t SystemD2Clock  = 275000000U;

/* Referenced by HAL_RCC_GetHCLKFreq()/GetPCLKx()Freq(). */
const uint8_t D1CorePrescTable[16] = {0, 0, 0, 0, 1, 2, 3, 4, 1, 2, 3, 4, 6, 7, 8, 9};

/* Called by the reset handler before main. Deliberately leaves RCC/OSPI
 * untouched. */
void SystemInit(void)
{
    SystemCoreClock = 550000000U;
    SystemD2Clock  = 275000000U;
}

/* HAL clock helpers may call this; the clock tree is owned by the bootloader,
 * so just report the known values. */
void SystemCoreClockUpdate(void)
{
    SystemCoreClock = 550000000U;
    SystemD2Clock  = 275000000U;
}
