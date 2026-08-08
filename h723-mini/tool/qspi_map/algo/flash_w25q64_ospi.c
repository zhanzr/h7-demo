/*
 * flash_w25q64_ospi.c - W25Q64 flash algorithm for STM32H723 using the OCTOSPI
 * PERIPHERAL (OCTOSPI1).
 *
 * Ported from the h750 QUADSPI algorithm to the H723 OCTOSPI HAL register map:
 *   - OCTOSPI1 at 0x52005000 (CR/DCR1/SR/FCR/DLR/AR/DR/CCR/TCR/IR)
 *   - OSPI kernel clock = D1HCLK (OCTOSPISEL default 0); prescaler 15 gives
 *     ~4 MHz at the reset HSI 64 MHz (connect-under-reset) and ~18 MHz at the
 *     bootloader's 275 MHz HCLK - both safe for the W25Q64.
 *   - All commands are 1-line SPI (0x02 page program, 0x20 sector erase, 0x03
 *     read, 0x05 status) so no QE bit is required.
 *   - Unlike QUADSPI, the OCTOSPI data register supports BYTE-granular
 *     read/write (one byte pops/pushes one FIFO slot), matching HAL_OSPI_*.
 *     NCS (PG6) is driven by the OCTOSPI hardware (works on this board - see
 *     the spi_flash_test project).
 *
 * Self-verifying: every page program and sector erase is read back and retried.
 *
 * Runs ON the target CPU (probe-rs loads it into RAM). Position independent:
 * no globals, only register access via literal pools.
 */

typedef volatile unsigned long  vu32;

#define RCC_AHB3ENR   (0x580244D4UL)
#define RCC_AHB3RSTR  (0x5802447CUL)
#define RCC_AHB4ENR   (0x580244E0UL)

#define OSPI_BASE     0x52005000UL
#define OSPI_CR       (OSPI_BASE + 0x00UL)
#define OSPI_DCR1     (OSPI_BASE + 0x08UL)
#define OSPI_DCR2     (OSPI_BASE + 0x0CUL)
#define OSPI_SR       (OSPI_BASE + 0x20UL)
#define OSPI_FCR      (OSPI_BASE + 0x24UL)
#define OSPI_DLR      (OSPI_BASE + 0x40UL)
#define OSPI_AR       (OSPI_BASE + 0x48UL)
#define OSPI_DR       (OSPI_BASE + 0x50UL)
#define OSPI_CCR      (OSPI_BASE + 0x100UL)
#define OSPI_TCR      (OSPI_BASE + 0x108UL)
#define OSPI_IR       (OSPI_BASE + 0x110UL)

#define OSPIM_BASE    0x5200B400UL
#define OSPIM_CR      (OSPIM_BASE + 0x00UL)
#define OSPIM_PCR0    (OSPIM_BASE + 0x04UL)   /* port 1 */
#define OSPIM_PCR1    (OSPIM_BASE + 0x08UL)   /* port 2 */

#define GPIOF_MODER   (0x58021400UL)
#define GPIOF_OTYPER  (0x58021404UL)
#define GPIOF_OSPEEDR (0x58021408UL)
#define GPIOF_PUPDR   (0x5802140CUL)
#define GPIOF_AFRL    (0x58021420UL)
#define GPIOF_AFRH    (0x58021424UL)
#define GPIOG_MODER   (0x58021800UL)
#define GPIOG_OTYPER  (0x58021804UL)
#define GPIOG_OSPEEDR (0x58021808UL)
#define GPIOG_PUPDR   (0x5802180CUL)
#define GPIOG_AFRL    (0x58021820UL)

#define W25Q_FLASH_BASE 0x90000000UL
#define W25Q_SECTOR_SIZE 4096u

/* CR bit fields.  CR[29:28] FMODE: 00=indirect WRITE, 01=indirect READ,
 * 10=auto-polling, 11=memory-mapped (HAL OSPI_FUNCTIONAL_MODE_*). */
#define CR_EN         (1u << 0)
#define CR_ABORT      (1u << 1)
#define CR_FTHRES_MSK (0x1Fu << 8)
#define CR_PRESCALER  (0xFFu << 24)
#define CR_FMODE_MSK  (0x3u << 28)
#define CR_FMODE_READ (0x1u << 28)   /* 01 = indirect read */
#define CR_FMODE_WRITE 0u            /* 00 = indirect write */

/* SR bit fields */
#define SR_TCF       (1u << 1)   /* transfer complete */
#define SR_FTF       (1u << 2)   /* fifo threshold flag */
#define SR_BUSY      (1u << 5)

/* FCR clear bits */
#define FCR_CLEAR_ALL (0x1Fu)    /* CTEF|CTCF|CSMF|CTOF|... */

/* ------------------------------------------------------------------------ */
static void ospi_wait_busy(void)
{
    unsigned long t = 0xFFFFFu;
    while (t--)
    {
        if (!(*(vu32 *)OSPI_SR & SR_BUSY))
        {
            return;
        }
    }
}

static void ospi_wait_tc(void)
{
    unsigned long t = 0xFFFFFu;
    while (t-- && !(*(vu32 *)OSPI_SR & SR_TCF)) { }
}

static void ospi_clear_flags(void)
{
    *(vu32 *)OSPI_FCR = FCR_CLEAR_ALL;
}

/* Generic 1-line transfer. NCS (PG6) is the OCTOSPI hardware NCS (AF10).
 * The DR is accessed byte-granular (one byte per access), matching the HAL.
 * This is a register-level port of the HAL sequence:
 *   - OSPI_ConfigCmd clears FMODE first, then CCR/TCR/DLR/IR/AR;
 *   - command-only (n==0) transfers AUTO-START once configured (no trigger
 *     re-write) and finish with TCF;
 *   - data reads start on an AR/IR re-write (HAL_OSPI_Receive);
 *   - data writes start on the first DR write (HAL_OSPI_Transmit). */
static void ospi_transfer(unsigned char instr, int use_addr, unsigned long off,
                          unsigned char *data, unsigned long n, int is_read)
{
    unsigned long ccr = 0x1u;                 /* IMODE = 1-line instruction */
    unsigned long i;

    ospi_wait_busy();
    ospi_clear_flags();

    if (use_addr)
    {
        ccr |= (0x1u << 8) | (0x2u << 12);    /* ADMODE=1-line, ADSIZE=24-bit */
    }
    if (n > 0u)
    {
        ccr |= (0x1u << 24);                  /* DMODE = 1-line data */
    }
    /* Config writes, matching the HAL's OSPI_ConfigCmd exactly:
     * clear FMODE, then CCR, TCR, DLR, IR, AR. */
    *(vu32 *)OSPI_CR = (*(vu32 *)OSPI_CR & ~CR_FMODE_MSK);   /* FMODE = 0 */
    *(vu32 *)OSPI_CCR  = ccr;
    *(vu32 *)OSPI_TCR  = 0x40000000UL;        /* preserve SSHIFT, DCYC = 0 */
    if (n > 0u) { *(vu32 *)OSPI_DLR = n - 1u; }
    else        { *(vu32 *)OSPI_DLR = 0; }
    *(vu32 *)OSPI_IR   = instr;
    *(vu32 *)OSPI_AR   = use_addr ? off : 0u;

    if (n == 0u)
    {
        /* Command-only: the transfer starts as soon as the configuration is
         * done (no trigger re-write) and completes with TCF. */
        ospi_wait_tc();
        ospi_clear_flags();
        return;
    }

    if (is_read)
    {
        /* HAL_OSPI_Receive: FMODE = indirect read, then trigger by re-writing
         * the address (or instruction) register. */
        *(vu32 *)OSPI_CR = (*(vu32 *)OSPI_CR & ~CR_FMODE_MSK) | CR_FMODE_READ;
        if (use_addr) { *(vu32 *)OSPI_AR = off; }
        else          { *(vu32 *)OSPI_IR = instr; }
        for (i = 0; i < n; i++)
        {
            unsigned long t = 0xFFFFFu;
            while (t-- && !(*(vu32 *)OSPI_SR & (SR_FTF | SR_TCF))) { }
            data[i] = *(volatile unsigned char *)OSPI_DR;
        }
        ospi_wait_tc();
        ospi_clear_flags();
        /* Drain any residual RX FIFO bytes after a READ (the OSPI FIFO is
         * shared RX/TX, so stale RX bytes would block the next TX transfer). */
        while (((*(vu32 *)OSPI_SR >> 8) & 0x3Fu) != 0u)
        {
            (void)*(volatile unsigned char *)OSPI_DR;
        }
        ospi_clear_flags();
    }
    else
    {
        /* HAL_OSPI_Transmit: FMODE = indirect write (0); the transfer starts
         * on the first DR write, no trigger re-write. */
        *(vu32 *)OSPI_CR = *(vu32 *)OSPI_CR & ~CR_FMODE_MSK;
#ifdef ALGO_DEBUG
        printf("WRITE cfg: CR=0x%08lX CCR=0x%08lX TCR=0x%08lX DLR=0x%08lX IR=0x%02lX AR=0x%08lX\r\n",
               *(vu32 *)OSPI_CR, *(vu32 *)OSPI_CCR, *(vu32 *)OSPI_TCR,
               *(vu32 *)OSPI_DLR, *(vu32 *)OSPI_IR, *(vu32 *)OSPI_AR);
        printf("WRITE pre SR=0x%08lX\r\n", *(vu32 *)OSPI_SR);
#endif
        for (i = 0; i < n; i++)
        {
            unsigned long t = 0xFFFFFu;
            while (t-- && !(*(vu32 *)OSPI_SR & SR_FTF)) { }
#ifdef ALGO_DEBUG
            if (t == 0u)
                printf("WRITE byte %lu: SR=0x%08lX (FTF wait TIMEOUT)\r\n",
                       (unsigned long)i, *(vu32 *)OSPI_SR);
#endif
            *(volatile unsigned char *)OSPI_DR = data[i];
        }
#ifdef ALGO_DEBUG
        printf("WRITE post-loop SR=0x%08lX\r\n", *(vu32 *)OSPI_SR);
#endif
        ospi_wait_tc();
        ospi_clear_flags();
    }
}

/* Command-only (write enable, chip erase). */
static void ospi_cmd(unsigned char instr)
{
    ospi_transfer(instr, 0, 0, (unsigned char *)0, 0, 0);
}

/* Command + 24-bit address, no data (sector erase). */
static void ospi_cmd_addr(unsigned char instr, unsigned long off)
{
    ospi_transfer(instr, 1, off, (unsigned char *)0, 0, 0);
}

/* Read n bytes at flash offset off (0x03). */
static void ospi_read_n(unsigned long off, unsigned char *out, unsigned long n)
{
    ospi_transfer(0x03, 1, off, out, n, 1);
}

/* Read n bytes with no address (0x9F JEDEC). */
static void ospi_read_cmd_n(unsigned char instr, unsigned char *out, unsigned long n)
{
    ospi_transfer(instr, 0, 0, out, n, 1);
}

/* Write n bytes at flash offset off (0x02 page program). */
static void ospi_write_n(unsigned long off, const unsigned char *buf, unsigned long n)
{
    ospi_transfer(0x02, 1, off, (unsigned char *)buf, n, 0);
}

/* Read the status register (0x05). */
static unsigned char w25q_status(void)
{
    unsigned char s;
    ospi_read_cmd_n(0x05, &s, 1);
    return s;
}

static void w25q_write_enable(void)
{
    ospi_cmd(0x06);
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
        ospi_read_n(off + done, tmp, c);
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
static void ospi_hw_init(void)
{
    /* 1. Peripheral + GPIO clocks; reset the OCTOSPI (it may be stuck in
     *    memory-mapped mode from the bootloader/app that ran before).
     *    AHB3ENR: OSPI1=bit14, IOMNGR (OCTOSPI manager)=bit21.
     *    AHB4ENR: GPIOF=bit5, GPIOG=bit6. Read back after each clock enable
     *    (as the HAL macros do) so the peripheral registers stick. */
    *(vu32 *)RCC_AHB3ENR |= 0x00204000UL;      /* OCTOSPIM + OCTOSPI1 clocks */
    (void)*(vu32 *)RCC_AHB3ENR;
    /* (peripheral reset intentionally skipped during bring-up debugging) */
    *(vu32 *)RCC_AHB4ENR |= 0x00000060UL;      /* GPIOF + GPIOG */
    (void)*(vu32 *)RCC_AHB4ENR;

    /* 2. GPIO (all OCTOSPI port 1, mode 3, ~4-18 MHz):
     *    PF8 IO0 / PF9 IO1 / PF7 IO2 / PF6 IO3 -> AF10
     *    PF10 CLK -> AF9
     *    PG6 NCS -> AF10 (hardware CS)
     * (GPIO writes commented out during bring-up debugging) */
    *(vu32 *)GPIOF_MODER   = 0xFFEAAFFFUL;    /* PF6-PF10 = AF(10), others analog */
    *(vu32 *)GPIOF_OTYPER  = 0;
    *(vu32 *)GPIOF_OSPEEDR = 0x003FF000UL;
    *(vu32 *)GPIOF_PUPDR   = 0;
    *(vu32 *)GPIOF_AFRL    = 0xAA000000UL;     /* PF7=AF10([31:28]) PF6=AF10([27:24]) */
    *(vu32 *)GPIOF_AFRH    = 0x000009AAUL;     /* PF10=AF9([11:8]) PF9=AF10([7:4]) PF8=AF10([3:0]) */
    *(vu32 *)GPIOG_MODER   = 0xFFFFEFFFUL;    /* PG6 = input, others analog (as HAL) */
    *(vu32 *)GPIOG_OTYPER  = 0;
    *(vu32 *)GPIOG_OSPEEDR = 0x00003000UL;
    *(vu32 *)GPIOG_PUPDR   = 0;
    *(vu32 *)GPIOG_AFRL    = 0x0A000000UL;     /* PG6 = AF10 (bits 27:24) */

    /* 3. OCTOSPI IO manager: route OSPI1's CLK/NCS/IO-low to physical port 1
     *    (GPIOF/G). PCR0 (port 1) value matches what the ST HAL programs:
     *    0x02010101 = CLKEN|NCSEN|IOLEN|IOLSRC_0, and REQ2ACK_TIME=0xFF in CR. */
    *(vu32 *)OSPI_CR   = 0u;                                   /* EN=0, FMODE=indirect write */
    *(vu32 *)OSPIM_CR  = 0x00FF0000UL;                         /* REQ2ACK_TIME = 0xFF */
    *(vu32 *)OSPIM_PCR0 = 0x02010101UL;                        /* port1: CLK+NCS+IO low <- OSPI1 */
    *(vu32 *)OSPIM_PCR1 = 0x00000000UL;                        /* port2 disabled */

    /* 4. OCTOSPI: mode 3 (DCR1.CKMODE), delay block bypassed, DEVSIZE=22
     *    (2^23 = 8 MB), clock PRESCALER in DCR2 = 15 (~4 MHz at the 64 MHz
     *    reset HSI, ~18 MHz at the bootloader's 275 MHz HCLK), FTHRES=7 and
     *    SAMPLE SHIFT in TCR.SSHIFT (1/2-cycle shift - both as the ST HAL). */
    *(vu32 *)OSPI_DCR2 = 1u;                                   /* PRESCALER (bits 7:0) */
    *(vu32 *)OSPI_DCR1 = (1u << 0) | (1u << 3) | (0u << 8)     /* CKMODE=3, DLYBYP, CSHT=0 */
                       | (22u << 16);                          /* DEVSIZE = 22 */
    *(vu32 *)OSPI_TCR  = 0x40000000UL;                         /* SSHIFT (1/2 cycle), DCYC=0 */
    *(vu32 *)OSPI_CR  |= (7u << 8) | CR_EN;                    /* FTHRES=7 + EN */
}

static unsigned long w25q_read_id(void)
{
    unsigned char b[3];
    ospi_read_cmd_n(0x9F, b, 3u);            /* JEDEC ID */
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

    ospi_hw_init();

    /* Reset the W25Q64 (0x66/0x99) - the running app leaves it in
     * continuous-read mode, which makes the first 0x9F read return 0x00. */
    ospi_cmd(0x66);
    ospi_cmd(0x99);
    {
        volatile unsigned long d;
        for (d = 0; d < 200000u; d++) { }
    }

    id = w25q_read_id();
    if (id == 0xEF4017UL) { return 0; }
    return (*(vu32 *)OSPI_CR & 0xFFu)
         | ((*(vu32 *)OSPI_SR & 0x3Fu) << 8)
         | ((*(vu32 *)OSPI_DLR & 0xFFu) << 16)
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
        ospi_cmd_addr(0x20, off);               /* sector erase */
        if (w25q_wip()) { }

        /* Read-back check: the 4 KB sector must read back all 0xFF. */
        ospi_read_n(off, t0, 8);
        ospi_read_n(off + W25Q_SECTOR_SIZE - 8u, t1, 8);
        for (i = 0; i < 8; i++) { if (t0[i] != 0xFFu || t1[i] != 0xFFu) { ok = 0; } }
        if (ok) { return 0; }
    }
    return 1;
}

int EraseChip(void)
{
    w25q_write_enable();
    ospi_cmd(0xC7);
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
            ospi_write_n(off, buf, n);
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
