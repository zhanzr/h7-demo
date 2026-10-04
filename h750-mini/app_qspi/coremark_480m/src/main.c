#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "utils.h"
#include "custom_def.h"
#include "core_portme.h"

int coremark_main(void);

int main(void)
{
    /* Identical to the embedded-flash twin: Board_Init() is QSPI-aware (the
     * QSPI_APP build makes SystemClock_Config()/MPU_Config() no-ops), so the
     * bootloader's 480 MHz clock tree + QSPI memory map are left untouched. */
    HAL_Init();
    Board_Init();

    const uint32_t cpu_hz = SystemCoreClock;

    while (1)
    {
        printf("\r\n--- CoreMark run on STM32H750VB @ %lu Hz (from QSPI flash) ---\r\n",
               (unsigned long)cpu_hz);
        coremark_main();
        printf("--- CoreMark complete. %lu Hz, %s ---\r\n",
               (unsigned long)cpu_hz, COMPILER_NAME);
        for (int i = 0; i < 10; i++)
        {
            HAL_Delay(1000);
        }
    }

    return 0;
}
