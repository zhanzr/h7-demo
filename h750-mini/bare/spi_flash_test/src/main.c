/**
  * @file    main.c
  * @brief   spi_flash_test - W25Q64 (8 MB) QSPI flash benchmark on the
  *          h750-mini board.
  *
  * Measures, over the USART1 console @ 115200:
  *   - erase: 4K / 32K / 64K block erase time (data-line independent)
  *   - write: 1-1-1 (0x02) vs 1-1-4 (0x32) page program (W25Q64 has NO 2-line
  *     write command)
  *   - read via HAL (FIFO polling): 1-1-1/1-1-2/1-2-2/1-1-4/1-4-4
  *   - read via memory-mapped mode (0x90000000): same 5 shapes, plus an XIP
  *     demo that copies a position-independent function into flash and runs it
  *     from the mapped space.
  *
  * Timed with the DWT cycle counter @ 480 MHz. Results re-printed every second
  * so a serial capture that opens after boot still sees the full report.
  */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "uart_printf.h"
#include "stm32h7xx_hal.h"

#include "w25q64.h"

#define CPU_HZ           480000000UL
#define TEST_ADDR        0x00100000UL            /* 1 MiB test window          */
#define TEST_SIZE        (256u * 1024u)          /* 256 KiB data window         */
#define CHUNK            4096u                   /* HAL read chunk size         */
#define WRITE_SIZE       (64u * 1024u)           /* per-mode write size         */

static uint8_t __attribute__((aligned(8))) buf[W25Q64_BLOCK64_SIZE]; /* 64 KB */

extern const uint8_t __qspi_code_start[];
extern const uint8_t __qspi_code_end[];
extern uint32_t w25q64_xip_fn(uint32_t seed);

/* ------------------------------------------------------------------------ */
/* DWT cycle counter (480 MHz)                                              */
/* ------------------------------------------------------------------------ */
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t cycles_now(void) { return DWT->CYCCNT; }

static inline double cyc_ms(uint32_t c)     { return (double)c / (CPU_HZ / 1000.0); }
static inline double cyc_mbps(uint32_t bytes, uint32_t c)
{
    return (double)bytes / (double)c * CPU_HZ / 1048576.0;
}
static inline double cyc_kbps(uint32_t bytes, uint32_t c)
{
    return (double)bytes / (double)c * CPU_HZ / 1024.0;
}

/* ------------------------------------------------------------------------ */
/* MPU: make the QSPI memory-mapped space (0x90000000) cacheable + executable */
/* ------------------------------------------------------------------------ */
static void mpu_qspi_config(void)
{
    HAL_MPU_Disable();
    SCB_InvalidateDCache();
    SCB_InvalidateICache();

    /* Region 0: QUADSPI BK1 0x90000000..0x9FFFFFFF as Normal write-back,
     * cacheable, executable memory. Everything else uses the default map. */
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
/* helpers                                                                  */
/* ------------------------------------------------------------------------ */
static uint32_t fill_pattern(uint8_t *p, uint32_t n)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint8_t v = (uint8_t)(i & 0xFFu);
        p[i] = v;
        sum += v;
    }
    return sum;
}

static uint32_t buf_sum(const uint8_t *p, uint32_t n)
{
    uint32_t s = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        s += p[i];
    }
    return s;
}

/* Sequential 32-bit read over the mapped QSPI space (cold, cacheline-at-a-time
 * after SCB_InvalidateDCache) - representative of a firmware XIP data read.
 * Sums per-BYTE so the result is comparable to the byte-checksum used for the
 * indirect reads (a raw 32-bit word accumulation wraps in uint32). */
static uint32_t memmap_sum(uint32_t base, uint32_t len)
{
    const volatile uint32_t *p = (const volatile uint32_t *)(uintptr_t)base;
    uint32_t sum = 0;
    len >>= 2;
    while (len--)
    {
        uint32_t v = *p++;
        sum += v & 0xFFu;
        sum += (v >> 8) & 0xFFu;
        sum += (v >> 16) & 0xFFu;
        sum += (v >> 24) & 0xFFu;
    }
    return sum;
}

/* ------------------------------------------------------------------------ */
static void bench_write(void)
{
    printf("\r\n--- write (page program), %u KiB per mode ---\r\n",
           (unsigned)(WRITE_SIZE >> 10));
    uint32_t expect = fill_pattern(buf, WRITE_SIZE);
    uint32_t blk = TEST_ADDR;

    for (w25q64_write_mode_t m = 0; m < W25Q64_WRITE_MODES; m++)
    {
        {
            int rc = w25q64_erase_block64(blk);
            if (rc != W25Q64_OK) { printf("erase FAIL rc=%d\r\n", rc); return; }
        }
        uint32_t t0 = cycles_now();
        int st = w25q64_write(blk, buf, WRITE_SIZE, m);
        uint32_t c = cycles_now() - t0;
        if (st != W25Q64_OK) { printf("write FAIL (%d)\r\n", st); return; }

        memset(buf, 0xEE, WRITE_SIZE);
        if (w25q64_read(blk, buf, WRITE_SIZE, W25Q64_READ_1_4_4) != W25Q64_OK)
        {
            printf("verify read FAIL\r\n"); return;
        }
        int ok = (buf_sum(buf, WRITE_SIZE) == expect);
        printf("%-10s: %7.2f ms, %8.2f KiB/s  verify=%s\r\n",
               w25q64_write_mode_name(m), cyc_ms(c), cyc_kbps(WRITE_SIZE, c),
               ok ? "OK" : "FAIL");
        blk += WRITE_SIZE;   /* keep block content for the read benches */
    }
}

/* ------------------------------------------------------------------------ */
static void bench_read_hal(void)
{
    uint32_t expect = fill_pattern(buf, WRITE_SIZE) * (TEST_SIZE / WRITE_SIZE);

    printf("\r\n--- read via HAL (FIFO polling), %u KiB, 4 KiB chunks ---\r\n",
           (unsigned)(TEST_SIZE >> 10));
    for (w25q64_read_mode_t m = 0; m < W25Q64_READ_MODES; m++)
    {
        uint32_t sum = 0, t0 = cycles_now();
        for (uint32_t off = 0; off < TEST_SIZE; off += CHUNK)
        {
            if (w25q64_read(TEST_ADDR + off, buf, CHUNK, m) != W25Q64_OK)
            {
                printf("read FAIL\r\n"); return;
            }
            sum += buf_sum(buf, CHUNK);
        }
        uint32_t c = cycles_now() - t0;
        printf("%-10s: %7.2f ms, %8.2f MiB/s  checksum=%s\r\n",
               w25q64_read_mode_name(m), cyc_ms(c), cyc_mbps(TEST_SIZE, c),
               (sum == expect) ? "OK" : "FAIL");
    }
}

/* ------------------------------------------------------------------------ */
static void bench_read_memmap(void)
{
    uint32_t expect = fill_pattern(buf, WRITE_SIZE) * (TEST_SIZE / WRITE_SIZE);
    uint32_t base = W25Q64_MEM_BASE + TEST_ADDR;

    printf("\r\n--- read via memory-mapped (0x%08lx), %u KiB ---\r\n",
           (unsigned long)base, (unsigned)(TEST_SIZE >> 10));
    for (w25q64_read_mode_t m = 0; m < W25Q64_READ_MODES; m++)
    {
        if (w25q64_memmap_start(m) != W25Q64_OK)
        {
            printf("memmap start FAIL\r\n"); return;
        }
        SCB_InvalidateDCache();                 /* cold read: pull from flash */
        uint32_t t0 = cycles_now();
        uint32_t sum = memmap_sum(base, TEST_SIZE);
        uint32_t c = cycles_now() - t0;
        w25q64_memmap_stop();
        printf("%-10s: %7.2f ms, %8.2f MiB/s  checksum=%s\r\n",
               w25q64_read_mode_name(m), cyc_ms(c), cyc_mbps(TEST_SIZE, c),
               (sum == expect) ? "OK" : "FAIL");
    }
}

/* ------------------------------------------------------------------------ */
static void bench_erase(void)
{
    printf("\r\n--- erase (data-line independent: always 1-1-1) ---\r\n");
    uint32_t addr = TEST_ADDR;

    {
        int rc = w25q64_erase_sector(addr);
        if (rc != W25Q64_OK) { printf("4K erase FAIL rc=%d\r\n", rc); return; }
    }
    uint32_t t0 = cycles_now();
    (void)w25q64_erase_sector(addr);
    printf("4K  sector erase (0x20): %8.2f ms\r\n", cyc_ms(cycles_now() - t0));

    t0 = cycles_now();
    if (w25q64_erase_block32(addr) != W25Q64_OK) { printf("32K erase FAIL\r\n"); return; }
    printf("32K block  erase (0x52): %8.2f ms\r\n", cyc_ms(cycles_now() - t0));

    t0 = cycles_now();
    if (w25q64_erase_block64(addr) != W25Q64_OK) { printf("64K erase FAIL\r\n"); return; }
    printf("64K block  erase (0xD8): %8.2f ms\r\n", cyc_ms(cycles_now() - t0));
}

/* ------------------------------------------------------------------------ */
static void bench_xip(void)
{
    printf("\r\n--- XIP: execute code from the mapped space (0x90000000) ---\r\n");
    uint32_t seed = 0x12345678u;
    uint32_t expect = (uint32_t)(seed * 0x1234u + 0x5678u);
    uint32_t code_len = (uint32_t)(__qspi_code_end - __qspi_code_start);
    uint32_t off = (uint32_t)(uintptr_t)&w25q64_xip_fn - (uint32_t)(uintptr_t)__qspi_code_start;

    if (w25q64_erase_sector(0u) != W25Q64_OK) { printf("erase FAIL\r\n"); return; }
    if (w25q64_write(0u, __qspi_code_start, code_len, W25Q64_WRITE_1_1_4) != W25Q64_OK)
    {
        printf("program FAIL\r\n"); return;
    }
    if (w25q64_memmap_start(W25Q64_READ_1_4_4) != W25Q64_OK) { printf("memmap FAIL\r\n"); return; }

    SCB_InvalidateICache();
    SCB_InvalidateDCache();
    /* Thumb bit must be set in the entry address (BLX/bx tests bit 0). */
    uint32_t (*fn)(uint32_t) =
        (uint32_t (*)(uint32_t))(uintptr_t)((W25Q64_MEM_BASE + off) | 1u);
    uint32_t got = fn(seed);
    w25q64_memmap_stop();

    printf("code: %lu bytes @ flash offset %lu (linked @ 0x%08lx)\r\n",
           (unsigned long)code_len, (unsigned long)off,
           (unsigned long)(uintptr_t)&w25q64_xip_fn);
    printf("fn(0x%08lx) = 0x%08lx, expected 0x%08lx -> %s\r\n",
           (unsigned long)seed, (unsigned long)got, (unsigned long)expect,
           (got == expect) ? "XIP OK" : "XIP FAIL");
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    Board_Init();
    dwt_init();
    mpu_qspi_config();

    printf("\r\n=== spi_flash_test on STM32H750VBTx @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    printf("init: clock + HW + QE ...\r\n");
    int st = w25q64_init();
    printf("init: rc=%d\r\n", st);
    if (st != W25Q64_OK)
    {
        printf("W25Q64 init FAILED (rc=%d, id=0x%06lx, SR1=0x%02X, SR2=0x%02X)\r\n", st,
               (unsigned long)w25q64_read_id(),
               (unsigned)w25q64_read_sr1(), (unsigned)w25q64_read_sr2());
        while (1)
        {
            printf("init FAILED rc=%d SR1=0x%02X SR2=0x%02X\r\n", st,
                   (unsigned)w25q64_read_sr1(), (unsigned)w25q64_read_sr2());
            HAL_Delay(1000);
        }
    }
    printf("W25Q64 detected: JEDEC 0x%06lx, %lu KiB\r\n",
           (unsigned long)w25q64_read_id(),
           (unsigned long)(W25Q64_FLASH_SIZE >> 10));

    /* Setup: pattern the whole 256 KiB test window (erase 4 x 64K first). */
    printf("erasing %u KiB test window @ 0x%08lx ...\r\n",
           (unsigned)(TEST_SIZE >> 10), (unsigned long)TEST_ADDR);
    for (uint32_t blk = TEST_ADDR; blk < TEST_ADDR + TEST_SIZE; blk += W25Q64_BLOCK64_SIZE)
    {
        if (w25q64_erase_block64(blk) != W25Q64_OK) { printf("erase FAIL\r\n"); while (1) { } }
    }
    fill_pattern(buf, WRITE_SIZE);
    for (uint32_t blk = TEST_ADDR; blk < TEST_ADDR + TEST_SIZE; blk += WRITE_SIZE)
    {
        if (w25q64_write(blk, buf, WRITE_SIZE, W25Q64_WRITE_1_1_4) != W25Q64_OK)
        {
            printf("setup write FAIL\r\n"); while (1) { }
        }
    }

    while (1)
    {
        bench_write();
        bench_read_hal();
        bench_read_memmap();
        bench_erase();
        bench_xip();
        printf("\r\n(done - repeating in 1 s)\r\n");
        HAL_Delay(1000);
    }
}
