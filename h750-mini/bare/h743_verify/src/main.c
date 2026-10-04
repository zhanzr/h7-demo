/**
  * @file    main.c
  * @brief   h743_verify - probe whether this "STM32H750VBT6" actually holds
  *          the STM32H743's 2 Mbyte flash.
  *
  * Some STM32H750VBT6 parts are believed to be *shadow* STM32H743VIT6 dies:
  * the same silicon, but marked/binned as the 128 KB H750 part. If true, the
  * 2 Mbyte flash is physically present and usable.
  *
  * What this firmware does:
  *   1. Prints the real flash size from the factory-programmed FLASH_SIZE
  *      register (0x1FF1E880) - informational.
  *   2. Holds a 15 x 128 KB const array (flash_probe[]) covering the entire
  *      2 Mbyte flash span: sectors 1..15 at 0x08020000..0x081FFFFF (sector 0
  *      holds the running app). Each 128 KB sector is filled with its own
  *      distinct byte pattern (0x51..0x5F) so that address aliasing / a
  *      short flash cannot fake the readback.
  *   3. Reads every sector back and checksums it. If the flash is really
  *      there, the flash tool could program it and each sector's checksum
  *      matches; if the part only has 128 KB, flashing beyond 0x08020000
  *      fails (or the readback returns erased 0xFF / aliased data and the
  *      checksums mismatch).
  *
  * Flash it with the H743 chip definition so probe-rs will attempt the write:
  *   CHIP=STM32H743VI bash tools/bench_capture.sh \
  *       h750-mini/h743_verify/build/h743_verify.hex 8 h743-verify-2m
  */

#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32h7xx_hal.h"

#define PROBE_SECTOR_SIZE (128u * 1024u)      /* one STM32H7 flash sector      */
#define PROBE_SECTORS     15u                 /* sectors 1..15 (0 holds the app) */
#define FLASH_PROBE_SIZE  (PROBE_SECTORS * PROBE_SECTOR_SIZE) /* 15/16 of 2 MB */
#define PROBE_FIRST_BYTE  0x51u               /* pattern byte of the first probe sector */
#define H750_FLASH_LIMIT  (128u * 1024u)      /* official STM32H750VBT6 flash  */

/* Pinned to .flash_probe (0x08020000) by h743_verify.ld. `used` + KEEP in the
 * linker script keep it from being dropped by --gc-sections. Each 128 KB
 * sector carries its own fill byte so aliasing cannot fake a readback. */
__attribute__((section(".flash_probe"), used))
const uint8_t flash_probe[FLASH_PROBE_SIZE] = {
    [0x000000u ... 0x01FFFFu] = 0x51u,
    [0x020000u ... 0x03FFFFu] = 0x52u,
    [0x040000u ... 0x05FFFFu] = 0x53u,
    [0x060000u ... 0x07FFFFu] = 0x54u,
    [0x080000u ... 0x09FFFFu] = 0x55u,
    [0x0A0000u ... 0x0BFFFFu] = 0x56u,
    [0x0C0000u ... 0x0DFFFFu] = 0x57u,
    [0x0E0000u ... 0x0FFFFFu] = 0x58u,
    [0x100000u ... 0x11FFFFu] = 0x59u,
    [0x120000u ... 0x13FFFFu] = 0x5Au,
    [0x140000u ... 0x15FFFFu] = 0x5Bu,
    [0x160000u ... 0x17FFFFu] = 0x5Cu,
    [0x180000u ... 0x19FFFFu] = 0x5Du,
    [0x1A0000u ... 0x1BFFFFu] = 0x5Eu,
    [0x1C0000u ... 0x1DFFFFu] = 0x5Fu,
};

int main(void)
{
    HAL_Init();
    Board_Init();

    /* 1. Real flash size from the flash size register. */
    uint32_t flash_size = FLASH_SIZE;           /* bytes                       */

    while (1)
    {
        /* Re-print the whole result block every second so a capture that opens
         * after boot still sees everything (the flash step itself can take
         * minutes, so booting happens long before the serial monitor opens). */
        printf("\r\n=== h743_verify on STM32H750VBTx @ %lu Hz ===\r\n",
               (unsigned long)SystemCoreClock);
        printf("FLASH_SIZE register: %lu KB (%lu bytes)\r\n",
               (unsigned long)(flash_size >> 10), (unsigned long)flash_size);
        printf("flash_probe @ 0x%08lx, %lu bytes = %u sectors x %lu KB "
               "(probe ends at 0x%08lx, %s 128 KB)\r\n",
               (unsigned long)(uintptr_t)flash_probe,
               (unsigned long)sizeof(flash_probe),
               (unsigned)PROBE_SECTORS,
               (unsigned long)(PROBE_SECTOR_SIZE >> 10),
               (unsigned long)((uintptr_t)flash_probe + sizeof(flash_probe)),
               sizeof(flash_probe) > H750_FLASH_LIMIT ? "image >" : "image <=");

        /* 2. Checksum each 128 KB sector and compare with its expected sum. */
        uint32_t total_ok = 0;
        uint32_t total_bad = 0;
        uint32_t total_sum = 0;
        for (uint32_t s = 0; s < PROBE_SECTORS; s++)
        {
            const uint8_t *sec = &flash_probe[(size_t)s * PROBE_SECTOR_SIZE];
            uint32_t pattern = PROBE_FIRST_BYTE + s;
            uint32_t sum = 0;
            for (uint32_t i = 0; i < PROBE_SECTOR_SIZE; i++)
            {
                sum += sec[i];
            }
            uint32_t expected = (uint32_t)PROBE_SECTOR_SIZE * pattern;
            int ok = (sum == expected);
            total_sum += sum;
            if (ok) total_ok++; else total_bad++;
            printf("sector %2u @ 0x%08lx: pat 0x%02x got 0x%08lx exp 0x%08lx -> %s\r\n",
                   (unsigned)s,
                   (unsigned long)(uintptr_t)sec,
                   (unsigned)pattern,
                   (unsigned long)sum, (unsigned long)expected,
                   ok ? "OK" : "MISMATCH");
        }
        printf("checksum total: 0x%08lx, %u/%u sectors OK -> %s\r\n",
               (unsigned long)total_sum,
               (unsigned)total_ok, (unsigned)PROBE_SECTORS,
               (total_bad == 0) ? "FLASH PRESENT (full 2 MB readback OK)"
                                : "FLASH MISSING (readback mismatch)");
        HAL_Delay(1000);
    }

    return 0;
}
