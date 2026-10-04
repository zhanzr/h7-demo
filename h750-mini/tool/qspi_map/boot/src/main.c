/**
  * @file    main.c
  * @brief   qspi_map stage-1 bootloader (internal flash).
  *
  * Boots from internal flash, prints a banner, initializes the W25Q64 via the
  * QUADSPI (100 MHz, QE set), and jumps to the stage-2 app located at the QSPI
  * memory-mapped base 0x90000000 (flash offset 0).
  *
  * If no valid app signature is found at 0x90000000 it enters a raw UART
  * download mode: expect 4-byte little-endian length, then the raw app binary;
  * the bytes are programmed into the W25Q64 with the same driver the benchmark
  * uses, then the bootloader jumps.
  */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32h7xx_hal.h"

#include "w25q64.h"

/* Stage-2 app image embedded in the bootloader (generated from app.bin). */
extern const uint8_t  app_image[];
extern const uint32_t app_image_len;

#define APP_BASE      0x90000000UL
#define APP_LIMIT     0x90800000UL     /* 8 MB window */

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

/* ------------------------------------------------------------------------ */
/* MPU: make 0x90000000..0x9FFFFFFF (QUADSPI) cacheable + executable; the rest
 * of the address space falls back to the ARM default map. The stage-2 app runs
 * under this MPU config, so it must permit code fetch + data access to QSPI. */
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

static int app_valid(void)
{
    uint8_t hdr[8];
    if (w25q64_read(0u, hdr, 8u, W25Q64_READ_1_1_1) != W25Q64_OK)
    {
        return 0;
    }
    uint32_t sp = rd32(&hdr[0]);
    uint32_t rv = rd32(&hdr[4]);
    /* Initial SP in DTCM/SRAM, reset vector (thumb) inside the QSPI window. */
    return (sp >= 0x20000000u && sp < 0x24000000u) &&
           (rv & 1u) && (rv & ~1u) >= APP_BASE && (rv & ~1u) < APP_LIMIT;
}

/* ------------------------------------------------------------------------ */
static uint32_t sum_bytes(const uint8_t *p, uint32_t n)
{
    uint32_t s = 0;
    for (uint32_t i = 0; i < n; i++) { s += p[i]; }
    return s;
}

/* Does the QSPI flash content match the embedded image? (self-update check)
 * Sums only where app_image is non-zero: the image has don't-care gap bytes
 * (0x00 in app.bin, 0xFF erased in flash) that must not trigger an update. */
static uint32_t flash_sum(void)
{
    uint8_t buf[64];
    uint32_t got = 0;
    for (uint32_t off = 0; off < app_image_len; off += sizeof(buf))
    {
        uint32_t n = (app_image_len - off < sizeof(buf)) ? (app_image_len - off) : sizeof(buf);
        w25q64_read(off, buf, n, W25Q64_READ_1_1_1);
        for (uint32_t i = 0; i < n; i++)
        {
            if (app_image[off + i] != 0u) { got += buf[i]; }
        }
    }
    return got;
}

static uint32_t app_sum(void)
{
    uint32_t s = 0;
    for (uint32_t i = 0; i < app_image_len; i++)
    {
        if (app_image[i] != 0u) { s += app_image[i]; }
    }
    return s;
}

static int flash_matches_embedded(void)
{
    return flash_sum() == app_sum();
}

/* ------------------------------------------------------------------------ */
static void jump_to_app(void)
{
    uint32_t sp   = *(volatile uint32_t *)(APP_BASE);
    uint32_t reset = *(volatile uint32_t *)(APP_BASE + 4u);

    __disable_irq();
    SCB->VTOR = APP_BASE;               /* relocate vector table to QSPI   */
    __set_MSP(sp);
    ((pfnVoid)reset)();                 /* app reset handler (thumb)       */
    while (1) { }                       /* never reached                   */
}

/* ------------------------------------------------------------------------ */
/* Program the embedded stage-2 app image into the W25Q64 at offset 0 using
 * the same driver as the benchmark (indirect mode, 1-1-4 page program). */
static int program_embedded_app(void)
{
    printf("programming embedded app (%lu B) into QSPI flash ...\r\n",
           (unsigned long)app_image_len);

    for (uint32_t off = 0; off < app_image_len; off += W25Q64_BLOCK64_SIZE)
    {
        int rc = w25q64_erase_block64(off);
        if (rc != W25Q64_OK) { printf("erase@%04lx FAIL rc=%d\r\n", (unsigned long)off, rc); return -1; }
    }

    for (uint32_t off = 0; off < app_image_len;)
    {
        uint32_t n = app_image_len - off;
        if (n > 1024u) { n = 1024u; }
        if (w25q64_write(off, &app_image[off], n, W25Q64_WRITE_1_1_4) != W25Q64_OK)
        {
            printf("write FAIL @ %lu\r\n", (unsigned long)off);
            return -1;
        }
        off += n;
        if ((off & 0x3FFF) == 0) { printf("  %lu / %lu\r\n", (unsigned long)off, (unsigned long)app_image_len); }
    }
    printf("programmed - verifying\r\n");
    return app_valid() ? 0 : -1;
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

    printf("\r\n=== qspi_map bootloader on STM32H750VBTx @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    int rc = w25q64_init();
    printf("W25Q64 init rc=%d, JEDEC 0x%06lx, SR2=0x%02X\r\n",
           rc, (unsigned long)w25q64_read_id(), (unsigned)w25q64_read_sr2());
    if (rc != W25Q64_OK)
    {
        printf("QSPI init failed, cannot map the app\r\n");
        while (1) { }
    }

    while (!app_valid() || !flash_matches_embedded())
    {
        printf("app missing or outdated - self-programming embedded image (%lu B)\r\n",
               (unsigned long)app_image_len);
        if (program_embedded_app() != 0)
        {
            printf("self-program failed; retrying\r\n");
            HAL_Delay(500);
        }
    }

    printf("app found at 0x%08lx - entering memory-mapped mode + jumping\r\n",
           (unsigned long)APP_BASE);
    rc = w25q64_memmap_start(W25Q64_READ_1_4_4);
    if (rc != W25Q64_OK)
    {
        printf("memmap start FAIL rc=%d\r\n", rc);
        while (1) { }
    }
    printf("jumping to 0x%08lx ...\r\n", (unsigned long)APP_BASE);
    jump_to_app();
    return 0;
}
