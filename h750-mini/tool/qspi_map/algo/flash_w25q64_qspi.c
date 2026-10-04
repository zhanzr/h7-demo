/*
 * flash_w25q64_qspi.c - W25Q64 flash algorithm for STM32H750 using the QUADSPI
 * PERIPHERAL (Step 2 of the faster-driver plan).
 *
 * KEY FINDING (debugged with the qspi_alg_test harness on real hardware):
 *   The QUADSPI's NCS output (PB6) is HIGH-IMPEDANCE when idle, and PB6 has an
 *   external pull-down on this board.  That leaves the flash permanently
 *   selected (CS stuck low), so after the first read the flash enters
 *   continuous-read mode and every later command is ignored.  The fix is to
 *   drive CS (PB6) MANUALLY as a GPIO output around each QUADSPI transfer,
 *   while the QUADSPI handles CLK + IO0/IO1/IO2/IO3.  Verified working:
 *   JEDEC, 0x03 reads (sequential + address), 0x02 page program + read-back,
 *   0x20 sector erase + read-back.
 *
 * Second finding: the QUADSPI DR must be read/written as 32-bit WORDS.  Byte
 * accesses do not pop/push the FIFO (every byte read returns the same oldest
 * byte).  A word read pops 4 bytes (byte0 = DR[7:0], ...).
 *
 * All commands are 1-line SPI (0x02 page program, 0x20 sector erase, 0x03 read,
 * 0x05 status) so no QE bit is required.  Self-verifying like the bit-bang
 * driver: every page program and sector erase is read back and retried.
 *
 * Runs ON the target CPU (probe-rs loads it into RAM). Position independent:
 * no globals, only register access via literal pools.
 */

typedef volatile unsigned long  vu32;

#define RCC_AHB3ENR   (0x580244D4UL)
#define RCC_AHB3RSTR  (0x5802447CUL)
#define RCC_AHB4ENR   (0x580244E0UL)

#define QSPI_BASE     0x52005000UL
#define QSPI_CR       (QSPI_BASE + 0x00UL)
#define QSPI_DCR      (QSPI_BASE + 0x04UL)
#define QSPI_SR       (QSPI_BASE + 0x08UL)
#define QSPI_FCR      (QSPI_BASE + 0x0CUL)
#define QSPI_DLR      (QSPI_BASE + 0x10UL)
#define QSPI_CCR      (QSPI_BASE + 0x14UL)
#define QSPI_AR       (QSPI_BASE + 0x18UL)
#define QSPI_ABR      (QSPI_BASE + 0x1CUL)
#define QSPI_DR       (QSPI_BASE + 0x20UL)

#define GPIOB_MODER   (0x58020400UL)
#define GPIOB_OTYPER  (0x58020404UL)
#define GPIOB_OSPEEDR (0x58020408UL)
#define GPIOB_PUPDR   (0x5802040CUL)
#define GPIOB_IDR     (0x58020410UL)
#define GPIOB_ODR     (0x58020414UL)
#define GPIOB_AFRL    (0x58020420UL)
#define GPIOB_AFRH    (0x58020424UL)
#define GPIOD_MODER   (0x58020C00UL)
#define GPIOD_OSPEEDR (0x58020C08UL)
#define GPIOD_PUPDR   (0x58020C0CUL)
#define GPIOD_AFRH    (0x58020C24UL)
#define GPIOE_MODER   (0x58021000UL)
#define GPIOE_OSPEEDR (0x58021008UL)
#define GPIOE_AFRL    (0x58021020UL)

#define NCS           (1u << 6)        /* PB6 = manual CS (GPIO output) */

#define W25Q_FLASH_BASE 0x90000000UL
#define W25Q_SECTOR_SIZE 4096u

/* ------------------------------------------------------------------------ */
static void qspi_wait_busy(void)
{
    unsigned long t = 0xFFFFFu;
    while (t--)
    {
        if (!(*(vu32 *)QSPI_SR & 0x00000020u))   /* SR.BUSY = bit 5 */
        {
            return;
        }
    }
}

static void qspi_clear_flags(void)
{
    *(vu32 *)QSPI_FCR = 0x00000003u;   /* clear TCF (bit1) + TEF (bit0) */
}

static void qspi_abort(void)
{
    unsigned long t;
    if (*(vu32 *)QSPI_CR & 1u)
    {
        *(vu32 *)QSPI_CR |= 2u;                 /* ABORT */
        t = 0xFFFFFu;
        while (t-- && (*(vu32 *)QSPI_CR & 2u)) { }
        *(vu32 *)QSPI_FCR = 0x1Fu;              /* clear all flags */
        t = 0xFFFFFu;
        while (t-- && (*(vu32 *)QSPI_SR & 0x20u)) { }
    }
}

/* Generic 1-line transfer.  CS (PB6) is toggled manually around the QUADSPI
 * transfer.  The DR FIFO is accessed with 32-bit word read/writes. */
static void qspi_transfer(unsigned char instr, int use_addr, unsigned long off,
                          unsigned char *data, unsigned long n, int is_read)
{
    unsigned long ccr = (unsigned long)instr | (1u << 8);   /* IMODE = 1-line */
    unsigned long i;

    qspi_wait_busy();
    qspi_clear_flags();
    *(vu32 *)GPIOB_ODR &= ~NCS;                 /* manual CS assert */

    if (use_addr)
    {
        ccr |= (1u << 10) | (2u << 12);      /* ADMODE = 1-line, ADSIZE = 24-bit */
    }
    if (n > 0u)
    {
        *(vu32 *)QSPI_DLR = n - 1u;
        ccr |= (1u << 24);                   /* DMODE = 1-line */
    }
    else
    {
        *(vu32 *)QSPI_DLR = 0;
    }
    ccr |= ((unsigned long)(is_read ? 1 : 0)) << 26;   /* FMODE[27:26] */

    if (use_addr) { *(vu32 *)QSPI_AR = off; }
    *(vu32 *)QSPI_CCR = ccr;
    if (use_addr) { *(vu32 *)QSPI_AR = off; } else { *(vu32 *)QSPI_AR = 0; }

    if (n > 0u)
    {
        if (is_read)
        {
            unsigned long wi = 0;
            for (i = 0; i < n; i++)
            {
                unsigned long t = 0xFFFFFu;
                while (t-- && !(*(vu32 *)QSPI_SR & (0x04u | 0x02u))) { }  /* FTF or TCF */
                if ((i & 3u) == 0u) { wi = *(volatile unsigned long *)QSPI_DR; }
                data[i] = (unsigned char)(wi & 0xFFu);
                wi >>= 8;
            }
        }
        else
        {
            unsigned long wi = 0;
            for (i = 0; i < n; i++)
            {
                unsigned long t = 0xFFFFFu;
                while (t-- && !(*(vu32 *)QSPI_SR & 0x04u)) { }  /* FTF */
                wi |= (unsigned long)data[i] << (8u * (i & 3u));
                if ((i & 3u) == 3u || i == n - 1u)
                {
                    *(volatile unsigned long *)QSPI_DR = wi;
                    wi = 0;
                }
            }
        }
    }
    qspi_wait_busy();
    qspi_clear_flags();
    qspi_abort();
    *(vu32 *)GPIOB_ODR |= NCS;                  /* manual CS deassert */
}

/* Command-only (write enable, chip erase). */
static void qspi_cmd(unsigned char instr)
{
    qspi_transfer(instr, 0, 0, (unsigned char *)0, 0, 0);
}

/* Command + 24-bit address, no data (sector erase). */
static void qspi_cmd_addr(unsigned char instr, unsigned long off)
{
    qspi_transfer(instr, 1, off, (unsigned char *)0, 0, 0);
}

/* Read n bytes at flash offset off (0x03). */
static void qspi_read_n(unsigned long off, unsigned char *out, unsigned long n)
{
    qspi_transfer(0x03, 1, off, out, n, 1);
}

/* Read n bytes with no address (0x9F JEDEC). */
static void qspi_read_cmd_n(unsigned char instr, unsigned char *out, unsigned long n)
{
    qspi_transfer(instr, 0, 0, out, n, 1);
}

/* Write n bytes at flash offset off (0x02 page program). */
static void qspi_write_n(unsigned long off, const unsigned char *buf, unsigned long n)
{
    qspi_transfer(0x02, 1, off, (unsigned char *)buf, n, 0);
}

/* Read the status register (0x05). */
static unsigned char w25q_status(void)
{
    unsigned char s;
    qspi_read_cmd_n(0x05, &s, 1);
    return s;
}

static void w25q_write_enable(void)
{
    qspi_cmd(0x06);
}

/* Wait for the W25Q64 WIP (status bit 0) to clear. */
static int w25q_wip(void)
{
    unsigned long t = 0xFFFFFFu;
    volatile unsigned long d;
    for (d = 0; d < 10000u; d++) { }
    while (t--)
    {
        if (!(w25q_status() & 1u)) { return 0; }
    }
    return 1;
}

/* Read-back verify (gap-tolerant) with retry-friendly return. */
static int w25q_verify(unsigned long off, const unsigned char *exp, unsigned long n)
{
    unsigned long done = 0;
    while (done < n)
    {
        unsigned long c = n - done;
        unsigned char tmp[64];
        unsigned long i;
        if (c > 64u) { c = 64u; }
        qspi_read_n(off + done, tmp, c);
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

/* ------------------------------------------------------------------------ */
static void qspi_hw_init(void)
{
    /* 1. Peripheral + GPIO clocks; reset the QUADSPI (it may be stuck in
     *    memory-mapped mode from the bootloader/app that ran before). */
    *(vu32 *)RCC_AHB3ENR |= 0x00004000UL;            /* QUADSPI clock */
    *(vu32 *)RCC_AHB3RSTR |= 0x00004000UL;           /* assert reset */
    *(vu32 *)RCC_AHB3RSTR &= ~0x00004000UL;          /* release reset */
    *(vu32 *)RCC_AHB4ENR |= 0x0000001AUL;            /* GPIOB/GPIOD/GPIOE */

    /* 2. GPIO: PB2 CLK AF9; PB6 = MANUAL CS as GPIO output (idle high) - the
     *    QUADSPI's NCS is hi-Z when idle and PB6 is pulled low on this board,
     *    so the flash would stay permanently selected without manual CS.
     *    PD11/PD12/PD13 = IO0/IO1/IO3 AF9, PE2 = IO2 AF9. */
    *(vu32 *)GPIOB_MODER   = 0x00001020UL;           /* PB2 AF(10), PB6 out(01) */
    *(vu32 *)GPIOB_OTYPER  = 0;
    *(vu32 *)GPIOB_OSPEEDR = 0x00003030UL;
    *(vu32 *)GPIOB_PUPDR   = 0;
    *(vu32 *)GPIOB_AFRL    = 0x00000900UL;           /* PB2 AF9 */
    *(vu32 *)GPIOB_AFRH    = 0;
    *(vu32 *)GPIOB_ODR     = (*(vu32 *)GPIOB_ODR | NCS);   /* CS idle high */
    *(vu32 *)GPIOD_MODER   = 0x2A800000UL;           /* PD11/12/13 AF(10) */
    *(vu32 *)GPIOD_OSPEEDR = 0x0FC00000UL;
    *(vu32 *)GPIOD_PUPDR   = 0;
    *(vu32 *)GPIOD_AFRH    = 0x00999000UL;           /* PD11/12/13 AF9 */
    *(vu32 *)GPIOE_MODER   = 0x00000020UL;           /* PE2 AF(10) */
    *(vu32 *)GPIOE_OSPEEDR = 0x00000030UL;
    *(vu32 *)GPIOE_AFRL    = 0x00000900UL;           /* PE2 AF9 */

    /* 3. QUADSPI: mode 0, 16MHz (64MHz HSI / 4).  FTHRES=3. */
    *(vu32 *)QSPI_CR  &= ~1u;                        /* EN = 0 */
    *(vu32 *)QSPI_CR   = (3u << 8);                  /* FTHRES = 3 */
    *(vu32 *)QSPI_CR   = (3u << 24);                 /* PRESCALER = 3 (~16MHz) */
    *(vu32 *)QSPI_DCR  = 0x00160000UL;               /* FSIZE=22, CKMODE=0 */
    *(vu32 *)QSPI_ABR  = 0;
    *(vu32 *)QSPI_CR  |= 1u;                         /* EN */
}

static unsigned long w25q_read_id(void)
{
    unsigned char b[3];
    qspi_read_cmd_n(0x9F, b, 3u);            /* JEDEC ID */
    return ((unsigned long)b[0] << 16) | ((unsigned long)b[1] << 8) | b[2];
}

int Init(unsigned long adr, unsigned long clk, unsigned long fnc)
{
    unsigned long id;
    (void)adr; (void)clk; (void)fnc;

    /* probe-rs payload buffers live in AXI SRAM; the M7 D-cache must not serve
     * stale lines when the CPU reads them. The algorithm does not need the
     * D-cache, so disable it. */
    *(vu32 *)0xE000ED88u &= ~(1u << 2);              /* SCB->SCTLR.C = 0 */
    __asm volatile ("dsb");
    __asm volatile ("isb");

    qspi_hw_init();

    /* Reset the W25Q64 (0x66/0x99) - the running app leaves it in
     * continuous-read mode, which makes the first 0x9F read return 0x00. */
    qspi_cmd(0x66);
    qspi_cmd(0x99);
    {
        volatile unsigned long d;
        for (d = 0; d < 200000u; d++) { }
    }

    id = w25q_read_id();
    if (id == 0xEF4017UL) { return 0; }
    return (*(vu32 *)QSPI_CR & 0xFFu)
         | ((*(vu32 *)QSPI_SR & 0x3Fu) << 8)
         | ((*(vu32 *)QSPI_DLR & 0xFFu) << 16)
         | ((id & 0xFFu) << 24);
}

int UnInit(unsigned long fnc)
{
    (void)fnc;
    return 0;
}

int EraseSector(unsigned long adr)
{
    unsigned long off = adr - W25Q_FLASH_BASE;
    int attempt;
    for (attempt = 0; attempt < 3; attempt++)
    {
        unsigned char t0[8], t1[8];
        unsigned long i;
        int ok = 1;

        w25q_write_enable();
        qspi_cmd_addr(0x20, off);               /* sector erase */
        if (w25q_wip()) { }

        /* Read-back check: the 4 KB sector must read back all 0xFF. */
        qspi_read_n(off, t0, 8);
        qspi_read_n(off + W25Q_SECTOR_SIZE - 8u, t1, 8);
        for (i = 0; i < 8; i++) { if (t0[i] != 0xFFu || t1[i] != 0xFFu) { ok = 0; } }
        if (ok) { return 0; }
    }
    return 1;
}

int EraseChip(void)
{
    w25q_write_enable();
    qspi_cmd(0xC7);
    return w25q_wip();
}

int ProgramPage(unsigned long adr, unsigned long sz, unsigned char *buf)
{
    unsigned long off = adr - W25Q_FLASH_BASE;
    while (sz > 0)
    {
        unsigned long room = 256u - (off % 256u);
        unsigned long n = (sz < room) ? sz : room;
        int attempt;

        for (attempt = 0; attempt < 4; attempt++)
        {
            w25q_write_enable();
            qspi_write_n(off, buf, n);
            if (w25q_wip()) { }
            if (w25q_verify(off, buf, n) == 0) { break; }
        }
        if (attempt == 4) { return 1; }

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
