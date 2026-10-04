/**
  * @file    w25q64.c
  * @brief   W25Q64 QSPI driver for the h750-mini board (see w25q64.h).
  *
  * QSPI kernel clock: PLL2 (HSE 25 MHz / M25 * N400 / R2 = 200 MHz) with
  * ClockPrescaler = 1 -> 100 MHz QSPI communication clock. 125 MHz works but
  * 1-line reads (0x03) become intermittently corrupt on this board; 100 MHz
  * is reliable. The W25Q64JV is rated 133 MHz.
  */

#include "w25q64.h"

#include "stm32h7xx_hal.h"

#define W25Q64_CMD_RESET_EN      0x66u
#define W25Q64_CMD_RESET         0x99u
#define W25Q64_CMD_JEDEC_ID      0x9Fu
#define W25Q64_CMD_WRITE_ENABLE  0x06u
#define W25Q64_CMD_READ_STATUS1  0x05u
#define W25Q64_STATUS1_BUSY      0x01u
#define W25Q64_STATUS1_WEL       0x02u

#define W25Q64_CMD_WRITE_STATUS1 0x01u  /* Write Status Register 1 (SR1)     */
#define W25Q64_CMD_WRITE_STATUS2 0x31u  /* Write Status Register 2 (SR2)      */
#define W25Q64_CMD_READ_STATUS2  0x35u  /* Read Status Register 2  (SR2)      */
#define W25Q64_STATUS2_QE        0x02u  /* SR2 bit1: Quad Enable              */

#define W25Q64_CMD_SECTOR_ERASE  0x20u   /* 4K  */
#define W25Q64_CMD_BLOCK32_ERASE 0x52u   /* 32K */
#define W25Q64_CMD_BLOCK64_ERASE 0xD8u   /* 64K */

#define W25Q64_CMD_PAGE_PROGRAM      0x02u  /* 1-1-1 */
#define W25Q64_CMD_QUAD_PAGE_PROGRAM 0x32u  /* 1-1-4 */

QSPI_HandleTypeDef hqspi;

/* read-shape table: command, address lines, data lines, dummy cycles, and
 * optional explicit "mode" alternate bytes. For the I/O commands (0xBB, 0xEB)
 * the W25Q64 interprets the first dummy cycles as mode bits; if those read as
 * 0xA0 the device ENTERS continuous-read mode and stops decoding normal
 * commands. We send 0xF0 explicitly via alternate bytes so the device always
 * exits continuous-read mode after each read. */
static const struct {
    uint8_t  instr;
    uint32_t addr_lines;
    uint32_t data_lines;
    uint8_t  dummy;
    uint32_t alt_mode;   /* QSPI_ALTERNATE_BYTES_* or QSPI_ALTERNATE_BYTES_NONE */
    uint8_t  alt_bytes;  /* mode bits value (MSB-first, e.g. 0xF0 = exit cont) */
} read_cfg[W25Q64_READ_MODES] = {
    { 0x03u, QSPI_ADDRESS_1_LINE,  QSPI_DATA_1_LINE,  0, QSPI_ALTERNATE_BYTES_NONE, 0x00u },  /* 1-1-1 */
    { 0x3Bu, QSPI_ADDRESS_1_LINE,  QSPI_DATA_2_LINES, 8, QSPI_ALTERNATE_BYTES_NONE, 0x00u },  /* 1-1-2 */
    { 0xBBu, QSPI_ADDRESS_2_LINES, QSPI_DATA_2_LINES, 0, QSPI_ALTERNATE_BYTES_2_LINES, 0xF0u }, /* 1-2-2 */
    { 0x6Bu, QSPI_ADDRESS_1_LINE,  QSPI_DATA_4_LINES, 8, QSPI_ALTERNATE_BYTES_NONE, 0x00u },  /* 1-1-4 */
    { 0xEBu, QSPI_ADDRESS_4_LINES, QSPI_DATA_4_LINES, 4, QSPI_ALTERNATE_BYTES_4_LINES, 0xF0u }, /* 1-4-4 */
};

/* write-shape table: command, data lines */
static const struct {
    uint8_t  instr;
    uint32_t data_lines;
} write_cfg[W25Q64_WRITE_MODES] = {
    { W25Q64_CMD_PAGE_PROGRAM,      QSPI_DATA_1_LINE  },  /* 1-1-1 */
    { W25Q64_CMD_QUAD_PAGE_PROGRAM, QSPI_DATA_4_LINES },  /* 1-1-4 */
};

/* ------------------------------------------------------------------------ */
/* QSPI kernel clock: PLL2 -> pll2_r_ck = 250 MHz (see header comment).     */
/* ------------------------------------------------------------------------ */
static int w25q64_clock_config(void)
{
    RCC_PeriphCLKInitTypeDef per = {0};

    per.PeriphClockSelection    = RCC_PERIPHCLK_QSPI;
    per.PLL2.PLL2M              = 25;                        /* 25 MHz / 25 = 1 MHz */
    per.PLL2.PLL2N              = 400;                       /* VCO = 400 MHz       */
    per.PLL2.PLL2P              = 2;
    per.PLL2.PLL2Q              = 2;
    per.PLL2.PLL2R              = 2;                         /* 400 / 2 = 200 MHz   */
    per.PLL2.PLL2RGE            = RCC_PLL2VCIRANGE_0;        /* input 1-2 MHz       */
    per.PLL2.PLL2VCOSEL         = RCC_PLL2VCOWIDE;
    per.PLL2.PLL2FRACN          = 0;
    per.QspiClockSelection      = RCC_QSPICLKSOURCE_PLL2;

    if (HAL_RCCEx_PeriphCLKConfig(&per) != HAL_OK)
    {
        return W25Q64_ERR_CLOCK;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
void HAL_QSPI_MspInit(QSPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef gpio = {0};

    if (hspi->Instance == QUADSPI)
    {
        __HAL_RCC_QSPI_CLK_ENABLE();
        __HAL_RCC_QSPI_FORCE_RESET();
        __HAL_RCC_QSPI_RELEASE_RESET();

        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        gpio.Mode  = GPIO_MODE_AF_PP;
        gpio.Pull  = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

        /* PB2  QUADSPI_CLK      AF9  */
        gpio.Pin = GPIO_PIN_2;   gpio.Alternate = GPIO_AF9_QUADSPI;
        HAL_GPIO_Init(GPIOB, &gpio);

        /* PB6  QUADSPI_BK1_NCS  AF10 */
        gpio.Pin = GPIO_PIN_6;   gpio.Alternate = GPIO_AF10_QUADSPI;
        HAL_GPIO_Init(GPIOB, &gpio);

        /* PD11 QUADSPI_BK1_IO0  AF9  */
        gpio.Pin = GPIO_PIN_11;  gpio.Alternate = GPIO_AF9_QUADSPI;
        HAL_GPIO_Init(GPIOD, &gpio);

        /* PD12 QUADSPI_BK1_IO1  AF9  */
        gpio.Pin = GPIO_PIN_12;  gpio.Alternate = GPIO_AF9_QUADSPI;
        HAL_GPIO_Init(GPIOD, &gpio);

        /* PE2  QUADSPI_BK1_IO2  AF9  */
        gpio.Pin = GPIO_PIN_2;   gpio.Alternate = GPIO_AF9_QUADSPI;
        HAL_GPIO_Init(GPIOE, &gpio);

        /* PD13 QUADSPI_BK1_IO3  AF9  */
        gpio.Pin = GPIO_PIN_13;  gpio.Alternate = GPIO_AF9_QUADSPI;
        HAL_GPIO_Init(GPIOD, &gpio);
    }
}

/* ------------------------------------------------------------------------ */
static void w25q64_hw_init(void)
{
    hqspi.Instance = QUADSPI;

    hqspi.Init.ClockPrescaler     = 1;                     /* 200 / 2 = 100 MHz */
    hqspi.Init.FifoThreshold      = 32;
    hqspi.Init.SampleShifting     = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hqspi.Init.FlashSize          = 22;                    /* 2^23 = 8 MB       */
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
    hqspi.Init.ClockMode          = QSPI_CLOCK_MODE_3;
    hqspi.Init.FlashID            = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash          = QSPI_DUALFLASH_DISABLE;

    HAL_QSPI_Init(&hqspi);
}

/* ------------------------------------------------------------------------ */
/* Wait for the W25Q64 BUSY bit (S1 bit 0) to clear via hardware auto-poll. */
/* ------------------------------------------------------------------------ */
static int w25q64_wait_busy(void)
{
    QSPI_CommandTypeDef cmd = {0};
    QSPI_AutoPollingTypeDef pol = {0};

    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_1_LINE;
    cmd.DummyCycles       = 0;
    cmd.Instruction       = W25Q64_CMD_READ_STATUS1;
    cmd.NbData            = 1;

    pol.Match           = 0;
    pol.Mask            = W25Q64_STATUS1_BUSY;
    pol.MatchMode       = QSPI_MATCH_MODE_AND;
    pol.StatusBytesSize = 1;
    pol.Interval        = 0x10;
    pol.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_QSPI_AutoPolling(&hqspi, &cmd, &pol, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_AUTOPOLL;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
static int w25q64_write_enable(void)
{
    QSPI_CommandTypeDef cmd = {0};
    QSPI_AutoPollingTypeDef pol = {0};

    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_NONE;
    cmd.DummyCycles       = 0;
    cmd.Instruction       = W25Q64_CMD_WRITE_ENABLE;

    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_WRITE_ENABLE;
    }

    pol.Match           = W25Q64_STATUS1_WEL;
    pol.Mask            = W25Q64_STATUS1_WEL;
    pol.MatchMode       = QSPI_MATCH_MODE_AND;
    pol.StatusBytesSize = 1;
    pol.Interval        = 0x10;
    pol.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    cmd.Instruction = W25Q64_CMD_READ_STATUS1;
    cmd.DataMode    = QSPI_DATA_1_LINE;
    cmd.NbData      = 1;

    if (HAL_QSPI_AutoPolling(&hqspi, &cmd, &pol, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_AUTOPOLL;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
uint32_t w25q64_read_id(void)
{
    QSPI_CommandTypeDef cmd = {0};
    uint8_t id[3] = {0};

    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_1_LINE;
    cmd.DummyCycles       = 0;
    cmd.NbData            = 3;
    cmd.Instruction       = W25Q64_CMD_JEDEC_ID;

    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return 0;
    }
    if (HAL_QSPI_Receive(&hqspi, id, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return 0;
    }
    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

/* ------------------------------------------------------------------------ */
static int w25q64_read_sr(uint8_t instr, uint8_t *val)
{
    QSPI_CommandTypeDef cmd = {0};
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_1_LINE;
    cmd.DummyCycles       = 0;
    cmd.NbData            = 1;
    cmd.Instruction       = instr;
    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    *val = 0;
    return (HAL_QSPI_Receive(&hqspi, val, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK)
           ? W25Q64_OK : W25Q64_ERR_RECEIVE;
}

uint8_t w25q64_read_sr1(void)
{
    uint8_t v = 0;
    (void)w25q64_read_sr(W25Q64_CMD_READ_STATUS1, &v);
    return v;
}

uint8_t w25q64_read_sr2(void)
{
    uint8_t v = 0;
    (void)w25q64_read_sr(W25Q64_CMD_READ_STATUS2, &v);
    return v;
}

/* ------------------------------------------------------------------------ */
/* Set SR2.QE (Quad Enable) - REQUIRED for all quad commands (0x6B, 0xEB,   */
/* 0xBB, 0x32). The W25Q64JV ships with QE=0; without it the quad pins are   */
/* not connected internally and quad reads/writes silently fail. The vendor  */
/* driver (qspi_w25q64.c) never sets this - a real bug.                      */
/*                                                                           */
/* QE lives in non-volatile SR2, and non-volatile status-register writes are */
/* gated by WP# = IO2 (PE2). In 1-line mode the QUADSPI does not drive IO2,  */
/* so PE2 floats; if it sits low the write is silently rejected. Drive it    */
/* high for the duration of the write, then restore the AF.                  */
/* ------------------------------------------------------------------------ */
static int w25q64_quad_enable(void)
{
    uint8_t sr2;

    /* 1) Read SR2 first (read-modify-write, preserve other bits). */
    QSPI_CommandTypeDef cmd = {0};
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_1_LINE;
    cmd.DummyCycles       = 0;
    cmd.NbData            = 1;
    cmd.Instruction       = W25Q64_CMD_READ_STATUS2;
    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    sr2 = 0;
    if (HAL_QSPI_Receive(&hqspi, &sr2, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_RECEIVE;
    }
    if ((sr2 & W25Q64_STATUS2_QE) != 0)
    {
        return W25Q64_OK;                       /* already enabled */
    }

    /* 2) Drive IO2 (WP#) high so the non-volatile write is accepted. */
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOE_CLK_ENABLE();
    gpio.Pin   = GPIO_PIN_2;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOE, &gpio);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_SET);

    /* 3) Write Enable + write SR2 |= QE. Try the 1-byte 0x31 (SR2) form
     * first; some W25Q64JV revisions only honour QE through the 2-byte
     * 0x01 (SR1,SR2) form, so fall back to that. */
    int st = W25Q64_OK;
    uint8_t data[2] = { 0x00u, (uint8_t)(sr2 | W25Q64_STATUS2_QE) };

    st = w25q64_write_enable();
    if (st == W25Q64_OK)
    {
        cmd.Instruction = W25Q64_CMD_WRITE_STATUS2;   /* 0x31, 1 byte */
        cmd.NbData      = 1;
        cmd.DataMode    = QSPI_DATA_1_LINE;
        if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        {
            st = W25Q64_ERR_COMMAND;
        }
        else if (HAL_QSPI_Transmit(&hqspi, &data[1], HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        {
            st = W25Q64_ERR_TRANSMIT;
        }
        else
        {
            st = w25q64_wait_busy();
        }
        if (st != W25Q64_OK) { return st; }
    }

    if (w25q64_read_sr2() & W25Q64_STATUS2_QE)
    {
        goto out;                                    /* 0x31 worked */
    }

    /* Fall back: 0x01 with (SR1, SR2) two-byte payload. */
    st = w25q64_write_enable();
    if (st != W25Q64_OK)
    {
        return st;
    }
    cmd.Instruction = W25Q64_CMD_WRITE_STATUS1;       /* 0x01, 2 bytes */
    cmd.NbData      = 2;
    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    if (HAL_QSPI_Transmit(&hqspi, data, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_TRANSMIT;
    }
    st = w25q64_wait_busy();
    if (st != W25Q64_OK)
    {
        return st;
    }

out:
    /* 4) Restore IO2 to QUADSPI AF9 regardless of the result. */
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOE, &gpio);

    if (st != W25Q64_OK)
    {
        return st;
    }

    /* 5) Verify QE actually took. */
    cmd.Instruction = W25Q64_CMD_READ_STATUS2;
    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    sr2 = 0;
    if (HAL_QSPI_Receive(&hqspi, &sr2, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_RECEIVE;
    }
    return ((sr2 & W25Q64_STATUS2_QE) != 0) ? W25Q64_OK : W25Q64_ERR_INIT;
}

/* ------------------------------------------------------------------------ */
int w25q64_reset_flash(void)
{
    QSPI_CommandTypeDef cmd = {0};
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_NONE;
    cmd.DummyCycles       = 0;
    cmd.Instruction       = W25Q64_CMD_RESET_EN;   /* 0x66 */
    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    (void)w25q64_wait_busy();
    cmd.Instruction = W25Q64_CMD_RESET;            /* 0x99 */
    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    return w25q64_wait_busy();
}

/* ------------------------------------------------------------------------ */
int w25q64_init(void)
{
    int st = w25q64_clock_config();
    if (st != W25Q64_OK)
    {
        return st;
    }
    w25q64_hw_init();

    /* STM32H7: the I/O compensation cell is required for high-speed QSPI GPIO
     * drive strength (see STM32H750B-DK QSPI_MemoryMappedDual). Without it the
     * 100 MHz QSPI is marginal and can glitch under sustained memory-mapped
     * reads (e.g. a benchmark that thrashes the I-cache). Enable it: CSI clock
     * + SYSCFG clock + SYSCFG_CCCSR bit0. */
    __HAL_RCC_CSI_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_EnableCompensationCell();

    /* Reset the flash so it is in a known (non-QPI, non-memory-mapped) state */
    QSPI_CommandTypeDef cmd = {0};
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_NONE;
    cmd.DummyCycles       = 0;
    cmd.Instruction       = W25Q64_CMD_RESET_EN;
    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    (void)w25q64_wait_busy();
    cmd.Instruction = W25Q64_CMD_RESET;
    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    (void)w25q64_wait_busy();

    if (w25q64_read_id() != W25Q64_JEDEC_ID)
    {
        return W25Q64_ERR_INIT;
    }

    /* Quad Enable must come AFTER reset (reset clears it) and before any
     * quad-mode command. */
    return w25q64_quad_enable();
}

/* ------------------------------------------------------------------------ */
static int w25q64_erase(uint32_t addr, uint8_t instr)
{
    QSPI_CommandTypeDef cmd = {0};

    int st = w25q64_write_enable();
    if (st != W25Q64_OK)
    {
        return st;
    }

    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_1_LINE;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_NONE;
    cmd.DummyCycles       = 0;
    cmd.Address           = addr;
    cmd.Instruction       = instr;

    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_ERASE;
    }
    return w25q64_wait_busy();
}

int w25q64_erase_sector(uint32_t addr)  { return w25q64_erase(addr, W25Q64_CMD_SECTOR_ERASE);  }
int w25q64_erase_block32(uint32_t addr) { return w25q64_erase(addr, W25Q64_CMD_BLOCK32_ERASE); }
int w25q64_erase_block64(uint32_t addr) { return w25q64_erase(addr, W25Q64_CMD_BLOCK64_ERASE); }

/* ------------------------------------------------------------------------ */
static int w25q64_write_page(uint32_t addr, const void *buf, uint16_t len,
                             w25q64_write_mode_t m)
{
    QSPI_CommandTypeDef cmd = {0};

    int st = w25q64_write_enable();
    if (st != W25Q64_OK)
    {
        return st;
    }

    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_1_LINE;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = write_cfg[m].data_lines;
    cmd.DummyCycles       = 0;
    cmd.NbData            = len;
    cmd.Address           = addr;
    cmd.Instruction       = write_cfg[m].instr;

    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    if (HAL_QSPI_Transmit(&hqspi, (uint8_t *)buf, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_TRANSMIT;
    }
    return w25q64_wait_busy();
}

int w25q64_write(uint32_t addr, const void *buf, uint32_t len,
                 w25q64_write_mode_t m)
{
    if (m >= W25Q64_WRITE_MODES)
    {
        return W25Q64_ERR_PARAM;
    }
    if (addr + len > W25Q64_FLASH_SIZE)
    {
        return W25Q64_ERR_PARAM;
    }

    const uint8_t *src = (const uint8_t *)buf;
    while (len > 0)
    {
        /* A page program command must not cross a 256-byte boundary. */
        uint32_t room = W25Q64_PAGE_SIZE - (addr % W25Q64_PAGE_SIZE);
        uint32_t n = (len < room) ? len : room;

        int st = w25q64_write_page(addr, src, (uint16_t)n, m);
        if (st != W25Q64_OK)
        {
            return st;
        }
        addr += n;
        src  += n;
        len  -= n;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
int w25q64_read(uint32_t addr, void *buf, uint32_t len, w25q64_read_mode_t m)
{
    QSPI_CommandTypeDef cmd = {0};

    if (m >= W25Q64_READ_MODES)
    {
        return W25Q64_ERR_PARAM;
    }

    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = read_cfg[m].addr_lines;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.AlternateByteMode = read_cfg[m].alt_mode;
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    cmd.AlternateBytes    = read_cfg[m].alt_bytes;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = read_cfg[m].data_lines;
    cmd.DummyCycles       = read_cfg[m].dummy;
    cmd.NbData            = len;
    cmd.Address           = addr;
    cmd.Instruction       = read_cfg[m].instr;

    if (HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    if (HAL_QSPI_Receive(&hqspi, buf, HAL_QSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_RECEIVE;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
int w25q64_memmap_start(w25q64_read_mode_t m)
{
    QSPI_CommandTypeDef cmd = {0};
    QSPI_MemoryMappedTypeDef mm = {0};

    if (m >= W25Q64_READ_MODES)
    {
        return W25Q64_ERR_PARAM;
    }

    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = read_cfg[m].addr_lines;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.AlternateByteMode = read_cfg[m].alt_mode;
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    cmd.AlternateBytes    = read_cfg[m].alt_bytes;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = read_cfg[m].data_lines;
    cmd.DummyCycles       = read_cfg[m].dummy;
    cmd.Instruction       = read_cfg[m].instr;

    /* Timeout counter DISABLED, matching ST's ExtMem_Boot: with TCEN enabled a
     * gap in memory-mapped reads can clear BUSY / leave memmap mode mid-run,
     * and the next code fetch bus-errors (IBUSERR) - seen as a hard fault when
     * a benchmark thrashes the QSPI. */
    mm.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    mm.TimeOutPeriod     = 0;

    /* Make sure we are in command mode before switching to memory-mapped. */
    hqspi.State = HAL_QSPI_STATE_READY;

    if (HAL_QSPI_MemoryMapped(&hqspi, &cmd, &mm) != HAL_OK)
    {
        return W25Q64_ERR_MEMMAP;
    }
    return W25Q64_OK;
}

void w25q64_memmap_stop(void)
{
    /* Leave memory-mapped mode. The timeout counter is now DISABLED (it caused
     * mid-run mapping drops / IBUSERR faults, see memmap_start), so the BUSY
     * flag must be cleared explicitly with an abort, then FMODE is switched
     * back to indirect-write - this mirrors the colleague guidance and ST's
     * HAL_QSPI_Abort sequence. */
    hqspi.State = HAL_QSPI_STATE_READY;

    QUADSPI->CR |= QUADSPI_CR_ABORT;
    while (QUADSPI->SR & QUADSPI_SR_BUSY) { }
    QUADSPI->CR &= ~QUADSPI_CR_ABORT;
    MODIFY_REG(QUADSPI->CCR, QUADSPI_CCR_FMODE, 0u);   /* indirect-write */

    QSPI_CommandTypeDef cmd = {0};
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
    cmd.DataMode          = QSPI_DATA_1_LINE;
    cmd.DummyCycles       = 0;
    cmd.Instruction       = W25Q64_CMD_READ_STATUS1;
    cmd.NbData            = 1;
    (void)HAL_QSPI_Command(&hqspi, &cmd, HAL_QSPI_TIMEOUT_DEFAULT_VALUE);
    (void)w25q64_wait_busy();
}

/* ------------------------------------------------------------------------ */
const char *w25q64_read_mode_name(w25q64_read_mode_t m)
{
    static const char *const names[W25Q64_READ_MODES] = {
        "1-1-1 (0x03)", "1-1-2 (0x3B)", "1-2-2 (0xBB)",
        "1-1-4 (0x6B)", "1-4-4 (0xEB)"
    };
    return (m < W25Q64_READ_MODES) ? names[m] : "?";
}

const char *w25q64_write_mode_name(w25q64_write_mode_t m)
{
    static const char *const names[W25Q64_WRITE_MODES] = {
        "1-1-1 (0x02)", "1-1-4 (0x32)"
    };
    return (m < W25Q64_WRITE_MODES) ? names[m] : "?";
}
