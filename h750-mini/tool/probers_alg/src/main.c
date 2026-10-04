/**
 * qspi_alg_test - run the flash algorithm's exact register-level QUADSPI code
 * as a normal, debuggable firmware (the "host" that simulates probe-rs loading
 * the algorithm). Starts on HSI (replicating the algorithm context) and prints
 * every clock/QSPI register + the JEDEC read result, so the QUADSPI behaviour
 * can finally be seen instead of returning cryptic error codes.
 */

#include <stdio.h>
#include "board.h"
#include "uart_printf.h"
#include "stm32h7xx_hal.h"

/* Same functions as flash_w25q64_qspi.c (register only, but using HAL regs). */

static void qspi_wait_busy(void)
{
    unsigned long t = 0xFFFFFu;
    while (t--) { if (!(QUADSPI->SR & 0x00000020u)) { return; } }
}

/* Force the QUADSPI to end the transaction and deassert NCS. */
static void qspi_abort(void)
{
    unsigned long t;
    if (QUADSPI->CR & 1u)
    {
        QUADSPI->CR |= 2u;                 /* ABORT */
        t = 0xFFFFFu;
        while (t-- && (QUADSPI->CR & 2u)) { }   /* wait ABORT accepted */
        QUADSPI->FCR = 0x1Fu;              /* clear all flags */
        t = 0xFFFFFu;
        while (t-- && (QUADSPI->SR & 0x20u)) { } /* wait idle */
    }
}

static void qspi_clear_flags(void)
{
    QUADSPI->FCR = 0x00000003u;
}

static void qspi_transfer(unsigned char instr, int use_addr, unsigned long off,
                          unsigned char *data, unsigned long n, int is_read)
{
    unsigned long ccr = (unsigned long)instr | (1u << 8);
    unsigned long i;
    qspi_wait_busy();
    qspi_clear_flags();
    GPIOB->ODR &= ~(1UL << 6);       /* manual CS assert (PB6 is GPIO) */
    if (use_addr) ccr |= (1u << 10) | (2u << 12);
    if (n > 0u) { QUADSPI->DLR = n - 1u; ccr |= (1u << 24); }
    else        { QUADSPI->DLR = 0; }
    ccr |= ((unsigned long)(is_read ? 1 : 0)) << 26;
    if (use_addr) { QUADSPI->AR = off; }
    QUADSPI->CCR = ccr;
    if (use_addr) { QUADSPI->AR = off; } else { QUADSPI->AR = 0; }
    if (n > 0u)
    {
        if (is_read)
        {
            /* The H750 QUADSPI DR must be read as 32-bit words (byte reads do
             * NOT pop the FIFO - every byte read returns the same oldest byte).
             * A word read pops 4 bytes: byte0 = DR[7:0], byte1 = DR[15:8], ... */
            unsigned long wi = 0;
            for (i = 0; i < n; i++)
            {
                unsigned long t = 0xFFFFFu;
                while (t-- && !(QUADSPI->SR & (0x04u | 0x02u))) { }
                if ((i & 3u) == 0u) { wi = *(volatile unsigned long *)&QUADSPI->DR; }
                data[i] = (unsigned char)(wi & 0xFFu);
                wi >>= 8;
            }
        }
        else
        {
            /* Word DR writes (byte writes may not push the FIFO either). */
            unsigned long wi = 0;
            for (i = 0; i < n; i++)
            {
                unsigned long t = 0xFFFFFu;
                while (t-- && !(QUADSPI->SR & 0x04u)) { }
                wi |= (unsigned long)data[i] << (8u * (i & 3u));
                if ((i & 3u) == 3u || i == n - 1u)
                {
                    *(volatile unsigned long *)&QUADSPI->DR = wi;
                    wi = 0;
                }
            }
        }
    }
    qspi_wait_busy();
    qspi_clear_flags();
    qspi_abort();          /* force NCS high between transfers */
    GPIOB->ODR |= (1UL << 6);        /* manual CS deassert */
}

static void qspi_cmd(unsigned char instr)
{
    qspi_transfer(instr, 0, 0, (unsigned char *)0, 0, 0);
}

/* Bit-bang JEDEC read (same logic as the working bit-bang driver) - sanity
 * check that the flash + pins are alive before blaming the QUADSPI. */
#define NCS   (1UL << 6)
#define SCK   (1UL << 2)
#define MOSI  (1UL << 11)
#define MISO  (1UL << 12)
static void b_delay(void)
{
    volatile unsigned long i;
    for (i = 0; i < 100; i++) { }
}
static void b_bb_init(void)
{
    RCC->AHB4ENR |= 0x0000001AUL;
    GPIOB->MODER   = 0xFFFFDFDFUL;
    GPIOB->OTYPER  = 0;
    GPIOB->OSPEEDR = 0x00003030UL;
    GPIOB->PUPDR   = 0;
    GPIOB->ODR     = (GPIOB->ODR | NCS) & ~SCK;
    GPIOD->MODER   = 0xF47FFFFFUL;
    GPIOD->OTYPER  = 0;
    GPIOD->OSPEEDR = 0x0000C000UL;
    GPIOD->PUPDR   = 0;
    GPIOD->ODR     = (GPIOD->ODR | (1UL << 13)) & ~MOSI;
    GPIOE->MODER   = 0xFFFFFFDFUL;
    GPIOE->ODR     = GPIOE->ODR | (1UL << 2);
}
static void b_start(void)  { GPIOB->ODR &= ~NCS; b_delay(); }
static void b_stop(void)   { b_delay(); GPIOB->ODR |= NCS; b_delay(); }
static void b_wr(unsigned char b)
{
    unsigned long i;
    for (i = 8; i > 0; i--)
    {
        if (b & 0x80u) { GPIOD->ODR |= MOSI; } else { GPIOD->ODR &= ~MOSI; }
        b_delay();
        GPIOB->ODR |= SCK;
        b_delay();
        GPIOB->ODR &= ~SCK;
        b_delay();
        b <<= 1;
    }
}
static unsigned char b_rd(void)
{
    unsigned char v = 0;
    unsigned long i;
    for (i = 8; i > 0; i--)
    {
        GPIOB->ODR |= SCK;
        b_delay();
        v = (unsigned char)((v << 1) | (((GPIOD->IDR & MISO) != 0) ? 1u : 0u));
        GPIOB->ODR &= ~SCK;
        b_delay();
    }
    return v;
}
static unsigned long bb_read_id(void)
{
    unsigned long r;
    b_start(); b_wr(0xF0); b_stop(); b_delay();
    b_start(); b_wr(0x9F);
    r = ((unsigned long)b_rd() << 16) | ((unsigned long)b_rd() << 8) | b_rd();
    b_stop();
    return r;
}

/* Bit-bang status read (0x05). */
static unsigned char bb_status(void)
{
    unsigned char s;
    b_start(); b_wr(0xF0); b_stop(); b_delay();
    b_start(); b_wr(0x05);
    s = b_rd();
    b_stop();
    return s;
}

static void qspi_read_cmd_n(unsigned char instr, unsigned char *out, unsigned long n)
{
    qspi_transfer(instr, 0, 0, out, n, 1);
}

static void qspi_read_n(unsigned long off, unsigned char *out, unsigned long n)
{
    qspi_transfer(0x03, 1, off, out, n, 1);
}

static unsigned long w25q_read_id(void)
{
    unsigned char b[3];
    qspi_read_cmd_n(0x9F, b, 3u);
    return ((unsigned long)b[0] << 16) | ((unsigned long)b[1] << 8) | b[2];
}

/* The DevEBox-style clock: PLL1 480MHz + D1HCLK QSPI kernel. */
static int qspi_clock_init(void)
{
    unsigned long t;
    printf("  [ck] VOS0+LDO ...\r\n");
    PWR->CR3 |= 0x00000002UL;
    PWR->D3CR = (PWR->D3CR & ~0xC000UL) | 0xC000UL;
    t = 0xFFFFFu; while (t-- && !(PWR->CSR1 & 0x4000UL)) { }
    printf("  [ck] VOSRDY=%ld\r\n", (unsigned long)((PWR->CSR1 >> 14) & 1));
    printf("  [ck] HSE on ...\r\n");
    RCC->CR |= 0x00010000UL;
    t = 0xFFFFFFu; while (t-- && !(RCC->CR & 0x00020000UL)) { }
    printf("  [ck] HSERDY=%ld\r\n", (unsigned long)((RCC->CR >> 17) & 1));
    if (!(RCC->CR & 0x00020000UL)) { return 1; }
    printf("  [ck] CSI on ...\r\n");
    RCC->CR |= 0x00000100UL;
    t = 0xFFFFFu; while (t-- && !(RCC->CR & 0x00000200UL)) { }
    printf("  [ck] CSIRDY=%ld (skipping compensation; not required for QSPI)\r\n",
           (unsigned long)((RCC->CR >> 9) & 1));
    printf("  [ck] PLL1+PLL2 cfg ...\r\n");
    RCC->PLLCKSELR = 0x02UL | (5u << 4) | (25u << 12);
    RCC->PLLCFGR   = (2u << 2) | (1u << 16) | (1u << 17) | (1u << 18)
                   | (1u << 19) | (1u << 20) | (1u << 21);
    RCC->PLL1DIVR  = (2u << 24) | (4u << 16) | (2u << 9) | 192u;
    RCC->PLL2DIVR  = (2u << 24) | (2u << 16) | (2u << 9) | 500u;
    RCC->CR |= 0x05000000UL;
    t = 0xFFFFFu; while (t-- && !(RCC->CR & 0x02000000UL)) { }
    printf("  [ck] PLL1RDY=%ld\r\n", (unsigned long)((RCC->CR >> 25) & 1));
    if (!(RCC->CR & 0x02000000UL)) { return 5; }
    t = 0xFFFFFu; while (t-- && !(RCC->CR & 0x08000000UL)) { }
    printf("  [ck] PLL2RDY=%ld\r\n", (unsigned long)((RCC->CR >> 27) & 1));
    if (!(RCC->CR & 0x08000000UL)) { return 2; }
    printf("  [ck] FLASH_ACR + SYSCLK switch ...\r\n");
    FLASH->ACR = (FLASH->ACR & ~0x0FUL) | 0x04UL;
    __DSB();
    __ISB();
    /* D1CPRE=/1 (SYSCLK_DIV1), HPRE=/2 (HCLK 240MHz), APB3/1/2/4 = /2. */
    RCC->CFGR = 0x00000003UL | (0x4u << 8) | (0x4u << 12) | (0x4u << 16)
              | (0x4u << 20) | (0x4u << 21);
    RCC->D1CCIPR &= ~0x30UL;
    printf("  [ck] SYSCLK switched\r\n");
    return 0;
}

static void qspi_hw_init(void)
{
    RCC->AHB3ENR |= 0x00004000UL;
    RCC->AHB3RSTR |= 0x00004000UL;
    RCC->AHB3RSTR &= ~0x00004000UL;
    RCC->AHB4ENR |= 0x0000001AUL;
    GPIOB->MODER   = 0x00001020UL;   /* PB2 CLK AF(10); PB6 = GPIO output(01), manual CS */
    GPIOB->OSPEEDR = 0x00003030UL;
    GPIOB->PUPDR   = 0;
    GPIOB->AFR[0]  = 0x00000900UL;   /* PB2 AF9 (CLK, bits11:8) only - PB6 is GPIO CS */
    GPIOB->AFR[1]  = 0;
    GPIOB->ODR     = (GPIOB->ODR | (1UL << 6));   /* CS idle high */
    GPIOD->MODER   = 0x2A800000UL;   /* PD11/PD12/PD13 = AF(10), others analog */
    GPIOD->OSPEEDR = 0x0FC00000UL;
    GPIOD->PUPDR   = 0;
    GPIOD->AFR[1]  = 0x00999000UL;   /* PD11/PD12/PD13 AF9 (IO0/IO1/IO3) */
    GPIOE->MODER   = 0x00000020UL;   /* PE2 AF(10), others analog */
    GPIOE->OSPEEDR = 0x00000030UL;
    GPIOE->PUPDR   = 0;
    GPIOE->AFR[0]  = 0x00000900UL;   /* PE2 AF9 (IO2) */
    QUADSPI->CR &= ~1u;                       /* EN = 0 first */
    QUADSPI->CR  = (3u << 8);                 /* FTHRES = 3 (1-byte threshold) */
    QUADSPI->CR  = (31u << 24);               /* PRESCALER = 31 (~2MHz) */
    QUADSPI->DCR = 0x00160000UL;              /* FSIZE=22, CSHT=0, CKMODE=0 (mode 0) */
    QUADSPI->ABR = 0;
    QUADSPI->CR |= 1u;                        /* EN last (as the HAL does) */
}

/* Vendor-style clock for the QUADSPI: HSE + PLL2 only (250MHz kernel), no
 * SYSCLK switch (core stays on HSI) - replicates the working vendor context. */
static int qspi_clock_pll2_only(void)
{
    unsigned long t;
    PWR->CR3 |= 0x00000002UL;
    PWR->D3CR = (PWR->D3CR & ~0xC000UL) | 0xC000UL;
    t = 0xFFFFFu; while (t-- && !(PWR->CSR1 & 0x4000UL)) { }
    printf("  [p2] VOSRDY ok\r\n");
    RCC->CR |= 0x00010000UL;
    t = 0xFFFFFu; while (t-- && !(RCC->CR & 0x00020000UL)) { }
    if (!(RCC->CR & 0x00020000UL)) { return 1; }
    printf("  [p2] HSE ok\r\n");
    RCC->PLLCKSELR = 0x02UL | (25u << 12);   /* HSE, DIVM2=25 */
    RCC->PLLCFGR   = (2u << 6) | (1u << 19) | (1u << 20) | (1u << 21);  /* PLL2RGE=2, P2/Q2/R2 */
    RCC->PLL2DIVR  = (2u << 24) | (2u << 16) | (2u << 9) | 500u;  /* R2=2,Q2=2,P2=2,N2=500 */
    RCC->CR |= 0x04000000UL;                 /* PLL2ON */
    t = 0xFFFFFu; while (t-- && !(RCC->CR & 0x08000000UL)) { }
    if (!(RCC->CR & 0x08000000UL)) { return 2; }
    printf("  [p2] PLL2 ready (pll2_r_ck=250MHz)\r\n");
    RCC->D1CCIPR = (RCC->D1CCIPR & ~0x30UL) | 0x20UL;   /* QSPISEL = PLL2 */
    printf("  [p2] D1CCIPR=0x%08lx QSPISEL=%ld\r\n",
           (unsigned long)RCC->D1CCIPR, (unsigned long)((RCC->D1CCIPR >> 4) & 3));
    return 0;
}

/* Vendor EXACT QUADSPI config: prescaler 1, FTHRES 31, SSHIFT, mode 3. */
static void qspi_hw_init_vendor(void)
{
    RCC->AHB3ENR |= 0x00004000UL;
    RCC->AHB3RSTR |= 0x00004000UL;
    RCC->AHB3RSTR &= ~0x00004000UL;
    RCC->AHB4ENR |= 0x0000001AUL;
    GPIOB->MODER   = 0x00002020UL;
    GPIOB->OSPEEDR = 0x00003030UL;
    GPIOB->PUPDR   = 0;
    GPIOB->AFR[0]  = 0xA0000900UL;
    GPIOB->AFR[1]  = 0;
    GPIOD->MODER   = 0x2A800000UL;
    GPIOD->OSPEEDR = 0x0FC00000UL;
    GPIOD->PUPDR   = 0;
    GPIOD->AFR[1]  = 0x00999000UL;
    GPIOE->MODER   = 0x00000020UL;
    GPIOE->OSPEEDR = 0x00000030UL;
    GPIOE->PUPDR   = 0;
    GPIOE->AFR[0]  = 0x00000900UL;
    QUADSPI->DCR = 0x00160001UL;
    QUADSPI->CR  = (1u << 24) | (31u << 8) | (1u << 4) | 1u;
    QUADSPI->ABR = 0;
}

int main(void)
{
    unsigned long id;
    unsigned char m[16], s;
    unsigned long i;
    int rc;

    HAL_Init();
    __enable_irq();
    UART_Init();

    printf("\r\n=== QSPI algorithm test ===\r\n");

    b_bb_init();
    printf("BITBANG JEDEC=0x%06lX (sanity)\r\n", bb_read_id());

    /* ---- Test 1: full read/write/erase cycle with manual-CS QUADSPI. */
    printf("--- Test 1: manual-CS QUADSPI full cycle ---\r\n");
    qspi_hw_init();
    printf("  JEDEC=0x%06lX\r\n", w25q_read_id());
    qspi_read_n(0, m, 8);
    printf("  0x03[0..7]=%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
           m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7]);

    /* page program at 0x200000 (safe region past the 41KB app) */
    {
        unsigned char wbuf[64], rbuf[64];
        unsigned long off = 0x00200000u;
        int ok = 1;
        for (i = 0; i < 64; i++) { wbuf[i] = (unsigned char)(i + 1); }
        qspi_cmd(0x06);                          /* write enable */
        qspi_transfer(0x02, 1, off, wbuf, 64, 0); /* page program */
        do { qspi_read_cmd_n(0x05, &s, 1u); } while (s & 1u);  /* wait WIP */
        qspi_read_n(off, rbuf, 64);
        for (i = 0; i < 64 && ok; i++) { if (rbuf[i] != wbuf[i]) { ok = 0; } }
        printf("  program+read @0x200000: %s (%02X %02X %02X %02X)\r\n",
               ok ? "MATCH" : "MISMATCH", rbuf[0], rbuf[1], rbuf[2], rbuf[3]);

        /* sector erase at 0x200000 + verify all FF */
        qspi_cmd(0x06);
        qspi_transfer(0x20, 1, off, (unsigned char *)0, 0, 0);  /* sector erase */
        do { qspi_read_cmd_n(0x05, &s, 1u); } while (s & 1u);
        qspi_read_n(off, rbuf, 64);
        ok = 1;
        for (i = 0; i < 64 && ok; i++) { if (rbuf[i] != 0xFFu) { ok = 0; } }
        printf("  erase+read @0x200000: %s\r\n", ok ? "ERASED (FF)" : "NOT ERASED");
    }

    printf("DONE\r\n");
    while (1) { }
}
