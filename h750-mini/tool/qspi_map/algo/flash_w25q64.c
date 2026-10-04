/*
 * flash_w25q64.c - W25Q64 flash algorithm for STM32H750, plain bit-banged SPI.
 *
 * The QUADSPI peripheral requires a PLL2 kernel clock that cannot be brought
 * up from the flash-algorithm execution context (chip at reset/HSI). Instead
 * this algorithm bit-bangs the W25Q64's standard SPI commands on the QUADSPI
 * pins as plain GPIOs (SCK=PB2, CS=PB6, MOSI/DI=PD11, MISO/DO=PD12,
 * /WP=PE2 and /HOLD=PD13 driven high). No PLL and no QUADSPI are used.
 *
 * Runs ON the target CPU (probe-rs loads it into RAM). Fully position
 * independent: no globals, only register access via literal pools.
 *
 * Commands (1-line SPI, mode 0): 0x06 write enable, 0x02 page program,
 * 0x20 sector erase, 0xC7 chip erase, 0x05 read status (WIP poll).
 */

typedef volatile unsigned long  vu32;
typedef volatile unsigned char  vu8;

#define RCC_AHB4ENR   (0x580244E0UL)
#define GPIOB_MODER   (0x58020400UL)
#define GPIOB_OTYPER  (0x58020404UL)
#define GPIOB_OSPEEDR (0x58020408UL)
#define GPIOB_PUPDR   (0x5802040CUL)
#define GPIOB_ODR     (0x58020414UL)
#define GPIOB_IDR     (0x58020410UL)
#define GPIOD_MODER   (0x58020C00UL)
#define GPIOD_OTYPER  (0x58020C04UL)
#define GPIOD_OSPEEDR (0x58020C08UL)
#define GPIOD_PUPDR   (0x58020C0CUL)
#define GPIOD_ODR     (0x58020C14UL)
#define GPIOD_IDR     (0x58020C10UL)
#define GPIOE_MODER   (0x58021000UL)
#define GPIOE_ODR     (0x58021014UL)

#define W25Q_FLASH_BASE 0x90000000UL
#define W25Q_SECTOR_SIZE 4096u

/* Pins */
#define SCK   (1UL << 2)    /* PB2  */
#define NCS   (1UL << 6)    /* PB6  */
#define MOSI  (1UL << 11)   /* PD11 = IO0 = DI */
#define MISO  (1UL << 12)   /* PD12 = IO1 = DO */
#define WP    (1UL << 2)    /* PE2  = IO2 = /WP  (drive high) */
#define HOLD  (1UL << 13)   /* PD13 = IO3 = /HOLD (drive high) */

int EraseSector(unsigned long adr);
int ProgramPage(unsigned long adr, unsigned long sz, unsigned char *buf);

static void spi_delay(void)
{
    volatile unsigned long i;
    for (i = 0; i < 100; i++) { }
}

static void spi_start(void);
static void spi_stop(void);
static void spi_write_byte(unsigned char b);

static void w25q_exit_continuous_read(void)
{
    /* The running app leaves the flash in continuous-read mode (memmap reads
     * end with 0xF0 mode bits). That state survives CPU resets and garbles a
     * subsequent 0x03 read, so reset it before every read transaction. */
    spi_start();
    spi_write_byte(0xF0);
    spi_stop();
    spi_delay();
}

static void spi_init(void)
{    /* clocks for GPIOB/D/E */
    *(vu32 *)RCC_AHB4ENR |= 0x0000001AUL;

    /* PB2 SCK + PB6 NCS: output push-pull. H7 GPIO resets to analog
     * (MODER=0xFFFFFFFF); output = 01 -> clear bit5/bit13, keep bit4/bit12. */
    *(vu32 *)GPIOB_MODER   = 0xFFFFDFDFUL;
    *(vu32 *)GPIOB_OTYPER  = 0x00000000UL;
    *(vu32 *)GPIOB_OSPEEDR = 0x00003030UL;
    *(vu32 *)GPIOB_PUPDR   = 0x00000000UL;
    *(vu32 *)GPIOB_ODR     = (*(vu32 *)GPIOB_ODR | NCS) & ~SCK;  /* CS idle high, SCK low */

    /* PD11 MOSI output (01), PD12 MISO input (00), PD13 /HOLD output high (01).
     * Clear bits 23/24/25/27, keep bits 22/26 -> 0xF47FFFFF. */
    *(vu32 *)GPIOD_MODER   = 0xF47FFFFFUL;
    *(vu32 *)GPIOD_OTYPER  = 0x00000000UL;
    *(vu32 *)GPIOD_OSPEEDR = 0x0000C000UL;
    *(vu32 *)GPIOD_PUPDR   = 0x00000000UL;
    *(vu32 *)GPIOD_ODR     = (*(vu32 *)GPIOD_ODR | HOLD) & ~MOSI;

    /* PE2 /WP output high (01): clear bit5, keep bit4 -> 0xFFFFFFDF. */
    *(vu32 *)GPIOE_MODER   = 0xFFFFFFDFUL;
    *(vu32 *)GPIOE_ODR     = (*(vu32 *)GPIOE_ODR | WP);

    w25q_exit_continuous_read();
}

static void spi_start(void)
{
    *(vu32 *)GPIOB_ODR &= ~NCS;
    spi_delay();
}

static void spi_stop(void)
{
    spi_delay();
    *(vu32 *)GPIOB_ODR |= NCS;
    spi_delay();
}

static void spi_write_byte(unsigned char b)
{
    unsigned long i;
    for (i = 8; i > 0; i--)
    {
        if (b & 0x80u) { *(vu32 *)GPIOD_ODR |= MOSI; }
        else           { *(vu32 *)GPIOD_ODR &= ~MOSI; }
        spi_delay();            /* data setup before the rising edge */
        *(vu32 *)GPIOB_ODR |= SCK;
        spi_delay();
        *(vu32 *)GPIOB_ODR &= ~SCK;
        spi_delay();
        b <<= 1;
    }
}

static unsigned char spi_read_byte(void)
{
    unsigned char v = 0;
    unsigned long i;
    for (i = 8; i > 0; i--)
    {
        *(vu32 *)GPIOB_ODR |= SCK;
        spi_delay();
        v = (unsigned char)((v << 1) | (((*(vu32 *)GPIOD_IDR & MISO) != 0) ? 1u : 0u));
        *(vu32 *)GPIOB_ODR &= ~SCK;
        spi_delay();
    }
    return v;
}

static void w25q_write_enable(void)
{
    spi_start();
    spi_write_byte(0x06);
    spi_stop();
}

static unsigned char w25q_status(void)
{
    unsigned char s;
    w25q_exit_continuous_read();
    spi_start();
    spi_write_byte(0x05);
    s = spi_read_byte();
    spi_stop();
    return s;
}

static int w25q_wip(void)
{
    unsigned long t = 0xFFFFFFu;
    volatile unsigned long d;
    /* Let the flash latch its BUSY flag before the first status read, else a
     * poll that starts too early sees WIP=0 and the next command overlaps. */
    for (d = 0; d < 20000u; d++) { }
    while (t--)
    {
        if (!(w25q_status() & 1u)) { return 0; }
    }
    return 1;
}

static void w25q_addr(unsigned long off)
{
    spi_write_byte((unsigned char)(off >> 16));
    spi_write_byte((unsigned char)(off >> 8));
    spi_write_byte((unsigned char)(off));
}

static unsigned long w25q_read_id(void)
{
    unsigned long id = 0;
    unsigned long i;
    w25q_exit_continuous_read();
    spi_start();
    spi_write_byte(0x9F);               /* JEDEC ID */
    for (i = 0; i < 3; i++)
    {
        id = (id << 8) | spi_read_byte();
    }
    spi_stop();
    return id;
}

/* Read n bytes at flash offset off via a fresh 0xF0 + 0x03 transaction (the
 * continuous-read reset makes the read deterministic - the flash's read state
 * is otherwise undefined between commands). */
static void w25q_read_n(unsigned long off, unsigned char *out, unsigned long n)
{
    unsigned long i;
    w25q_exit_continuous_read();
    spi_start();
    spi_write_byte(0x03);               /* read data */
    w25q_addr(off);
    spi_delay(); spi_delay(); spi_delay();   /* settle before the first bit */
    for (i = 0; i < n; i++)
    {
        out[i] = spi_read_byte();
    }
    spi_stop();
}

/* Read back n bytes at off and compare to exp (gap-tolerant: erased 0xFF vs a
 * don't-care 0x00 in the image). Returns 0 if it matches. Small chunks with a
 * fresh 0xF0 + 0x03 each keep the read deterministic. */
static int w25q_verify(unsigned long off, const unsigned char *exp, unsigned long n)
{
    unsigned long done = 0;
    while (done < n)
    {
        unsigned long c = n - done;
        unsigned char tmp[8];
        unsigned long i;
        if (c > 8u) { c = 8u; }
        w25q_read_n(off + done, tmp, c);
        for (i = 0; i < c; i++)
        {
            if (tmp[i] != exp[done + i] && !(tmp[i] == 0xFFu && exp[done + i] == 0x00u))
            {
                return 1;
            }
        }
        done += c;
    }
    return 0;
}

int Init(unsigned long adr, unsigned long clk, unsigned long fnc)
{
    (void)adr; (void)clk; (void)fnc;

    /* probe-rs loads the payload buffers into AXI SRAM, which the M7 D-cache
     * may serve stale lines from if it was left enabled (see STM32Cube / H7
     * flash-driver guidance). The algorithm does not need the D-cache, so turn
     * it off and invalidate before touching any buffer. */
    *(vu32 *)0xE000ED88u &= ~(1u << 2);   /* SCB->SCTLR.C = 0 (D-cache off) */
    __asm volatile ("dsb");
    __asm volatile ("isb");

    spi_init();
    /* The bit-banged 1-line read must return the W25Q64 JEDEC ID. */
    return (w25q_read_id() == 0xEF4017UL) ? 0 : 1;
}

int UnInit(unsigned long fnc)
{
    (void)fnc;
    return 0;
}

int EraseSector(unsigned long adr)
{
    unsigned long off = adr - W25Q_FLASH_BASE;
    unsigned long i;
    unsigned char t0[8], t1[8];
    int attempt;

    for (attempt = 0; attempt < 3; attempt++)
    {
        w25q_write_enable();
        spi_start();
        spi_write_byte(0x20);           /* sector erase */
        w25q_addr(off);
        spi_stop();
        if (w25q_wip()) { }             /* waited for not-busy */

        /* Read-back check: the 4 KB sector must read back all 0xFF. */
        w25q_read_n(off, t0, 8);
        w25q_read_n(off + W25Q_SECTOR_SIZE - 8u, t1, 8);
        {
            int ok = 1;
            for (i = 0; i < 8; i++) { if (t0[i] != 0xFFu || t1[i] != 0xFFu) { ok = 0; } }
            if (ok) { return 0; }
        }
    }
    return 1;
}

int EraseChip(void)
{
    w25q_write_enable();
    spi_start();
    spi_write_byte(0xC7);           /* chip erase */
    spi_stop();
    return w25q_wip();
}

int ProgramPage(unsigned long adr, unsigned long sz, unsigned char *buf)
{
    unsigned long off = adr - W25Q_FLASH_BASE;

    /* The W25Q64 page program cannot cross a 256-byte boundary - split. */
    while (sz > 0)
    {
        unsigned long room = 256u - (off % 256u);
        unsigned long n = (sz < room) ? sz : room;
        unsigned long i;
        int attempt;

        for (attempt = 0; attempt < 4; attempt++)
        {
            w25q_write_enable();
            spi_start();
            spi_write_byte(0x02);           /* page program */
            w25q_addr(off);
            for (i = 0; i < n; i++)
            {
                spi_write_byte(buf[i]);
            }
            spi_stop();
            if (w25q_wip()) { }

            /* Read back the page and compare; retry the whole page on error. */
            if (w25q_verify(off, buf, n) == 0) { break; }
        }
        if (attempt == 4) { return 1; }     /* page failed after retries */

        off += n;
        buf += n;
        sz  -= n;
    }
    return 0;
}

int Verify(unsigned long adr, unsigned long sz, unsigned char *buf)
{
    unsigned long off = adr - W25Q_FLASH_BASE;
    return w25q_verify(off, buf, sz);
}
