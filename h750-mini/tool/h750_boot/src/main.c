/**
  * @file    main.c
  * @brief   h750_boot - minimal bootloader in internal flash for the h750-mini.
  *
  * Boots from internal flash, brings up the 480 MHz clock tree and console,
  * initializes the W25Q64 via the QUADSPI, then performs a *basic* bootability
  * check on a stage-2 firmware at the QSPI memory-mapped base 0x90000000:
  *   - the initial stack pointer (word 0) must land in DTCM/SRAM, and
  *   - the reset vector (word 1) must be a thumb pointer inside the QSPI
  *     window.
  * If the check passes it enters QUADSPI memory-mapped mode and jumps. If it
  * fails it loops: toggles the PA8 LED and prints the check result every
  * second, so a missing/bad firmware is visible on both the LED and the
  * console without a debugger.
  */

#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32h7xx_hal.h"
#include "w25q64.h"

#define APP_BASE  0x90000000UL
#define APP_LIMIT 0x90800000UL     /* 8 MB QSPI window */

#define LED_PORT GPIOA
#define LED_PIN  GPIO_PIN_8

typedef void (*pfnVoid)(void);

/* ------------------------------------------------------------------------ */
static void LED_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

/* MPU: make 0x90000000..0x9FFFFFFF (QUADSPI) cacheable + executable; the rest
 * falls back to the ARM default map. Required for the stage-2 app to fetch and
 * execute its code from the QSPI-mapped space. */
static void mpu_qspi_config(void)
{
    HAL_MPU_Disable();
    SCB_InvalidateDCache();
    SCB_InvalidateICache();

    MPU_Region_InitTypeDef m = {0};
    m.Enable           = MPU_REGION_ENABLE;
    m.Number           = MPU_REGION_NUMBER0;
    m.BaseAddress      = 0x90000000u;
    m.Size             = MPU_REGION_SIZE_256MB;
    m.SubRegionDisable = 0;
    m.TypeExtField     = MPU_TEX_LEVEL1;              /* Normal write-back */
    m.AccessPermission = MPU_REGION_FULL_ACCESS;
    m.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    m.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    m.IsCacheable      = MPU_ACCESS_CACHEABLE;
    m.IsBufferable     = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&m);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    SCB_InvalidateDCache();
    SCB_InvalidateICache();
}

/* ------------------------------------------------------------------------ */
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Basic bootability check on the firmware at the QSPI base. Writes the parsed
 * SP / reset vector out for the caller to print. */
static int app_bootable(uint32_t *out_sp, uint32_t *out_rv)
{
    uint8_t hdr[8];
    *out_sp = 0;
    *out_rv = 0;

    if (w25q64_read(0u, hdr, 8u, W25Q64_READ_1_1_1) != W25Q64_OK)
    {
        return 0;
    }

    uint32_t sp = rd32(&hdr[0]);
    uint32_t rv = rd32(&hdr[4]);
    *out_sp = sp;
    *out_rv = rv;

    /* Initial SP in DTCM/SRAM, reset vector a thumb pointer in the QSPI window. */
    return (sp >= 0x20000000u && sp < 0x24000000u) &&
           (rv & 1u) && (rv & ~1u) >= APP_BASE && (rv & ~1u) < APP_LIMIT;
}

/* ------------------------------------------------------------------------ */
static void jump_to_app(void)
{
    uint32_t sp    = *(volatile uint32_t *)(APP_BASE);
    uint32_t reset = *(volatile uint32_t *)(APP_BASE + 4u);

    __disable_irq();
    SCB->VTOR = APP_BASE;               /* relocate vector table to QSPI */
    __set_MSP(sp);
    ((pfnVoid)reset)();                 /* app reset handler (thumb)     */
    while (1) { }                       /* never reached                 */
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    SCB_EnableICache();
    SCB_EnableDCache();
    SystemClock_Config();
    UART_Init();
    LED_Init();
    mpu_qspi_config();

    printf("\r\n=== h750_boot bootloader @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    int rc = w25q64_init();
    printf("W25Q64 init rc=%d, JEDEC 0x%06lx\r\n",
           rc, (unsigned long)w25q64_read_id());

    uint32_t sp = 0, rv = 0;
    int ok = (rc == W25Q64_OK) && app_bootable(&sp, &rv);

    printf("QSPI firmware check: %s (SP=0x%08lx, Reset=0x%08lx)\r\n",
           ok ? "PASS - booting" : "FAIL", (unsigned long)sp, (unsigned long)rv);

    if (ok)
    {
        rc = w25q64_memmap_start(W25Q64_READ_1_4_4);
        if (rc != W25Q64_OK)
        {
            printf("memmap start FAIL rc=%d\r\n", rc);
            ok = 0;
        }
    }

    if (ok)
    {
        printf("jumping to 0x%08lx ...\r\n", (unsigned long)APP_BASE);
        jump_to_app();
        return 0;
    }

    /* No bootable firmware: loop, toggle the LED, print the result. */
    printf("no bootable firmware on SPI flash - waiting (LED PA8 blinks)\r\n");
    while (1)
    {
        HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
        printf("boot FAIL: SP=0x%08lx Reset=0x%08lx (need SP in DTCM + thumb "
               "reset in 0x%08lx..0x%08lx)\r\n",
               (unsigned long)sp, (unsigned long)rv,
               (unsigned long)APP_BASE, (unsigned long)APP_LIMIT);
        printf("  NOTE: it is only a basic check. The bootloader itself is a "
               "basic demo. If necessary, modify the bootloader or the "
               "application's linker script.\r\n");
        HAL_Delay(3000);
    }
}
