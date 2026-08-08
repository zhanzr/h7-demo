/**
  * @file    w25q64.c
  * @brief   W25Q64 OSPI driver for the h723-mini board (see w25q64.h).
  *
  * OCTOSPI1 on port 1: PF8 IO0 / PF9 IO1 / PF7 IO2 / PF6 IO3 / PF10 CLK /
  * PG6 NCS. OSPI kernel clock = D1HCLK (275 MHz), ClockPrescaler = 2 ->
  * 137.5 MHz communication clock (vendor 6.OSPI example default).
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

OSPI_HandleTypeDef hospi1;

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
    uint32_t alt_mode;   /* HAL_OSPI_ALTERNATE_BYTES_* or _NONE */
    uint8_t  alt_bytes;  /* mode bits value (MSB-first, e.g. 0xF0 = exit cont) */
} read_cfg[W25Q64_READ_MODES] = {
    { 0x03u, HAL_OSPI_ADDRESS_1_LINE,  HAL_OSPI_DATA_1_LINE,  0, HAL_OSPI_ALTERNATE_BYTES_NONE, 0x00u },  /* 1-1-1 */
    { 0x3Bu, HAL_OSPI_ADDRESS_1_LINE,  HAL_OSPI_DATA_2_LINES, 8, HAL_OSPI_ALTERNATE_BYTES_NONE, 0x00u },  /* 1-1-2 */
    { 0xBBu, HAL_OSPI_ADDRESS_2_LINES, HAL_OSPI_DATA_2_LINES, 0, HAL_OSPI_ALTERNATE_BYTES_2_LINES, 0xF0u }, /* 1-2-2 */
    { 0x6Bu, HAL_OSPI_ADDRESS_1_LINE,  HAL_OSPI_DATA_4_LINES, 8, HAL_OSPI_ALTERNATE_BYTES_NONE, 0x00u },  /* 1-1-4 */
    { 0xEBu, HAL_OSPI_ADDRESS_4_LINES, HAL_OSPI_DATA_4_LINES, 4, HAL_OSPI_ALTERNATE_BYTES_4_LINES, 0xF0u }, /* 1-4-4 */
};

/* write-shape table: command, data lines */
static const struct {
    uint8_t  instr;
    uint32_t data_lines;
} write_cfg[W25Q64_WRITE_MODES] = {
    { W25Q64_CMD_PAGE_PROGRAM,      HAL_OSPI_DATA_1_LINE  },  /* 1-1-1 */
    { W25Q64_CMD_QUAD_PAGE_PROGRAM, HAL_OSPI_DATA_4_LINES },  /* 1-1-4 */
};

/* ------------------------------------------------------------------------ */
/* Fill the OSPI command struct with the common/default fields.              */
static void ospi_cmd_defaults(OSPI_RegularCmdTypeDef *cmd)
{
    cmd->OperationType           = HAL_OSPI_OPTYPE_COMMON_CFG;
    cmd->FlashId                 = HAL_OSPI_FLASH_ID_1;
    cmd->Instruction             = 0;
    cmd->InstructionMode         = HAL_OSPI_INSTRUCTION_1_LINE;
    cmd->InstructionSize         = HAL_OSPI_INSTRUCTION_8_BITS;
    cmd->InstructionDtrMode      = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    cmd->Address                 = 0;
    cmd->AddressMode             = HAL_OSPI_ADDRESS_NONE;
    cmd->AddressSize             = HAL_OSPI_ADDRESS_24_BITS;
    cmd->AddressDtrMode          = HAL_OSPI_ADDRESS_DTR_DISABLE;
    cmd->AlternateBytes          = 0;
    cmd->AlternateBytesMode      = HAL_OSPI_ALTERNATE_BYTES_NONE;
    cmd->AlternateBytesSize      = HAL_OSPI_ALTERNATE_BYTES_8_BITS;
    cmd->AlternateBytesDtrMode   = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
    cmd->DataMode                = HAL_OSPI_DATA_NONE;
    cmd->NbData                  = 0;
    cmd->DataDtrMode             = HAL_OSPI_DATA_DTR_DISABLE;
    cmd->DummyCycles             = 0;
    cmd->DQSMode                 = HAL_OSPI_DQS_DISABLE;
    cmd->SIOOMode                = HAL_OSPI_SIOO_INST_EVERY_CMD;
}

/* ------------------------------------------------------------------------ */
/* OSPI kernel clock: D1HCLK (275 MHz) / ClockPrescaler 2 -> 137.5 MHz.      */
static int w25q64_clock_config(void)
{
    RCC_PeriphCLKInitTypeDef per = {0};

    per.PeriphClockSelection = RCC_PERIPHCLK_OSPI;
    per.OspiClockSelection   = RCC_OSPICLKSOURCE_D1HCLK;

    if (HAL_RCCEx_PeriphCLKConfig(&per) != HAL_OK)
    {
        return W25Q64_ERR_CLOCK;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
void HAL_OSPI_MspInit(OSPI_HandleTypeDef *hospi)
{
    GPIO_InitTypeDef gpio = {0};

    if (hospi->Instance == OCTOSPI1)
    {
        __HAL_RCC_OCTOSPIM_CLK_ENABLE();
        __HAL_RCC_OSPI1_CLK_ENABLE();
        __HAL_RCC_OSPI1_FORCE_RESET();
        __HAL_RCC_OSPI1_RELEASE_RESET();

        __HAL_RCC_GPIOF_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();

        gpio.Mode  = GPIO_MODE_AF_PP;
        gpio.Pull  = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

        /* PF6 OCTOSPIM_P1_IO3 / PF7 IO2 / PF8 IO0 / PF9 IO1  -> AF10 */
        gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
        gpio.Alternate = GPIO_AF10_OCTOSPIM_P1;
        HAL_GPIO_Init(GPIOF, &gpio);

        /* PF10 OCTOSPIM_P1_CLK -> AF9 */
        gpio.Pin = GPIO_PIN_10;
        gpio.Alternate = GPIO_AF9_OCTOSPIM_P1;
        HAL_GPIO_Init(GPIOF, &gpio);

        /* PG6 OCTOSPIM_P1_NCS -> AF10 */
        gpio.Pin = GPIO_PIN_6;
        gpio.Alternate = GPIO_AF10_OCTOSPIM_P1;
        HAL_GPIO_Init(GPIOG, &gpio);
    }
}

/* ------------------------------------------------------------------------ */
static void w25q64_hw_init(void)
{
    OSPIM_CfgTypeDef sOspiManagerCfg = {0};

    hospi1.Instance = OCTOSPI1;

    hospi1.Init.ClockPrescaler       = 2;                    /* 275 / 2 = 137.5 MHz */
    hospi1.Init.FifoThreshold        = 8;
    hospi1.Init.DualQuad             = HAL_OSPI_DUALQUAD_DISABLE;
    hospi1.Init.MemoryType           = HAL_OSPI_MEMTYPE_MICRON;
    hospi1.Init.DeviceSize           = 23;                   /* 2^23 = 8 MB       */
    hospi1.Init.ChipSelectHighTime   = 1;
    hospi1.Init.FreeRunningClock     = HAL_OSPI_FREERUNCLK_DISABLE;
    hospi1.Init.ClockMode            = HAL_OSPI_CLOCK_MODE_3;
    hospi1.Init.WrapSize             = HAL_OSPI_WRAP_NOT_SUPPORTED;
    hospi1.Init.SampleShifting       = HAL_OSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hospi1.Init.DelayHoldQuarterCycle = HAL_OSPI_DHQC_DISABLE;
    hospi1.Init.ChipSelectBoundary   = 0;
    hospi1.Init.DelayBlockBypass     = HAL_OSPI_DELAY_BLOCK_BYPASSED;
    hospi1.Init.MaxTran              = 0;
    hospi1.Init.Refresh              = 0;

    HAL_OSPI_Init(&hospi1);

    /* OSPI manager: use port 1 for CLK/NCS and the low 4 IO lines. */
    sOspiManagerCfg.ClkPort    = 1;
    sOspiManagerCfg.NCSPort    = 1;
    sOspiManagerCfg.IOLowPort  = HAL_OSPIM_IOPORT_1_LOW;
    HAL_OSPIM_Config(&hospi1, &sOspiManagerCfg, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);
}

/* ------------------------------------------------------------------------ */
/* Wait for the W25Q64 BUSY bit (S1 bit 0) to clear via hardware auto-poll. */
static int w25q64_wait_busy(void)
{
    OSPI_RegularCmdTypeDef cmd = {0};
    OSPI_AutoPollingTypeDef pol = {0};

    ospi_cmd_defaults(&cmd);
    cmd.DataMode    = HAL_OSPI_DATA_1_LINE;
    cmd.NbData      = 1;
    cmd.Instruction = W25Q64_CMD_READ_STATUS1;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_AUTOPOLL;
    }

    pol.Match         = 0;
    pol.Mask          = W25Q64_STATUS1_BUSY;
    pol.MatchMode     = HAL_OSPI_MATCH_MODE_AND;
    pol.Interval      = 0x10;
    pol.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_OSPI_AutoPolling(&hospi1, &pol, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_AUTOPOLL;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
static int w25q64_write_enable(void)
{
    OSPI_RegularCmdTypeDef cmd = {0};
    OSPI_AutoPollingTypeDef pol = {0};

    ospi_cmd_defaults(&cmd);
    cmd.Instruction = W25Q64_CMD_WRITE_ENABLE;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_WRITE_ENABLE;
    }

    ospi_cmd_defaults(&cmd);
    cmd.DataMode    = HAL_OSPI_DATA_1_LINE;
    cmd.NbData      = 1;
    cmd.Instruction = W25Q64_CMD_READ_STATUS1;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_WRITE_ENABLE;
    }

    pol.Match         = W25Q64_STATUS1_WEL;
    pol.Mask          = W25Q64_STATUS1_WEL;
    pol.MatchMode     = HAL_OSPI_MATCH_MODE_AND;
    pol.Interval      = 0x10;
    pol.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_OSPI_AutoPolling(&hospi1, &pol, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_AUTOPOLL;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
uint32_t w25q64_read_id(void)
{
    OSPI_RegularCmdTypeDef cmd = {0};
    uint8_t id[3] = {0};

    ospi_cmd_defaults(&cmd);
    cmd.DataMode    = HAL_OSPI_DATA_1_LINE;
    cmd.NbData      = 3;
    cmd.Instruction = W25Q64_CMD_JEDEC_ID;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return 0;
    }
    if (HAL_OSPI_Receive(&hospi1, id, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return 0;
    }
    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

/* ------------------------------------------------------------------------ */
static int w25q64_read_sr(uint8_t instr, uint8_t *val)
{
    OSPI_RegularCmdTypeDef cmd = {0};

    ospi_cmd_defaults(&cmd);
    cmd.DataMode    = HAL_OSPI_DATA_1_LINE;
    cmd.NbData      = 1;
    cmd.Instruction = instr;
    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    *val = 0;
    return (HAL_OSPI_Receive(&hospi1, val, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK)
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
/* not connected internally and quad reads/writes silently fail.             */
/*                                                                           */
/* QE lives in non-volatile SR2, and non-volatile status-register writes are */
/* gated by WP# = IO2 (PF7). In 1-line mode the OSPI does not drive IO2, so  */
/* PF7 floats; if it sits low the write is silently rejected. Drive it high  */
/* for the duration of the write, then restore the AF.                       */
static int w25q64_quad_enable(void)
{
    uint8_t sr2;

    /* 1) Read SR2 first (read-modify-write, preserve other bits). */
    OSPI_RegularCmdTypeDef cmd = {0};
    ospi_cmd_defaults(&cmd);
    cmd.DataMode    = HAL_OSPI_DATA_1_LINE;
    cmd.NbData      = 1;
    cmd.Instruction = W25Q64_CMD_READ_STATUS2;
    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    sr2 = 0;
    if (HAL_OSPI_Receive(&hospi1, &sr2, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_RECEIVE;
    }
    if ((sr2 & W25Q64_STATUS2_QE) != 0)
    {
        return W25Q64_OK;                       /* already enabled */
    }

    /* 2) Drive IO2 (PF7, WP#) high so the non-volatile write is accepted. */
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOF_CLK_ENABLE();
    gpio.Pin   = GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOF, &gpio);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_SET);

    /* 3) Write Enable + write SR2 |= QE. Try the 1-byte 0x31 (SR2) form
     * first; some W25Q64JV revisions only honour QE through the 2-byte
     * 0x01 (SR1,SR2) form, so fall back to that. */
    int st = W25Q64_OK;
    uint8_t data[2] = { 0x00u, (uint8_t)(sr2 | W25Q64_STATUS2_QE) };

    st = w25q64_write_enable();
    if (st == W25Q64_OK)
    {
        ospi_cmd_defaults(&cmd);
        cmd.DataMode    = HAL_OSPI_DATA_1_LINE;
        cmd.NbData      = 1;
        cmd.Instruction = W25Q64_CMD_WRITE_STATUS2;   /* 0x31, 1 byte */
        if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        {
            st = W25Q64_ERR_COMMAND;
        }
        else if (HAL_OSPI_Transmit(&hospi1, &data[1], HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
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
    ospi_cmd_defaults(&cmd);
    cmd.DataMode    = HAL_OSPI_DATA_1_LINE;
    cmd.NbData      = 2;
    cmd.Instruction = W25Q64_CMD_WRITE_STATUS1;       /* 0x01, 2 bytes */
    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    if (HAL_OSPI_Transmit(&hospi1, data, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_TRANSMIT;
    }
    st = w25q64_wait_busy();
    if (st != W25Q64_OK)
    {
        return st;
    }

out:
    /* 4) Restore IO2 to OCTOSPI AF10 regardless of the result. */
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF10_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOF, &gpio);

    if (st != W25Q64_OK)
    {
        return st;
    }

    /* 5) Verify QE actually took. */
    return ((w25q64_read_sr2() & W25Q64_STATUS2_QE) != 0) ? W25Q64_OK : W25Q64_ERR_INIT;
}

/* ------------------------------------------------------------------------ */
int w25q64_reset_flash(void)
{
    OSPI_RegularCmdTypeDef cmd = {0};

    ospi_cmd_defaults(&cmd);
    cmd.Instruction = W25Q64_CMD_RESET_EN;   /* 0x66 */
    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    (void)w25q64_wait_busy();
    ospi_cmd_defaults(&cmd);
    cmd.Instruction = W25Q64_CMD_RESET;      /* 0x99 */
    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
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

    /* STM32H7: the I/O compensation cell is required for high-speed OSPI GPIO
     * drive strength. Enable it: CSI clock + SYSCFG clock + SYSCFG_CCCSR bit0. */
    __HAL_RCC_CSI_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_EnableCompensationCell();

    /* Reset the flash so it is in a known (non-QPI, non-memory-mapped) state */
    st = w25q64_reset_flash();
    if (st != W25Q64_OK)
    {
        return st;
    }

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
    OSPI_RegularCmdTypeDef cmd = {0};

    int st = w25q64_write_enable();
    if (st != W25Q64_OK)
    {
        return st;
    }

    ospi_cmd_defaults(&cmd);
    cmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    cmd.Address     = addr;
    cmd.Instruction = instr;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
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
    OSPI_RegularCmdTypeDef cmd = {0};

    int st = w25q64_write_enable();
    if (st != W25Q64_OK)
    {
        return st;
    }

    ospi_cmd_defaults(&cmd);
    cmd.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    cmd.Address     = addr;
    cmd.DataMode    = write_cfg[m].data_lines;
    cmd.NbData      = len;
    cmd.Instruction = write_cfg[m].instr;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    if (HAL_OSPI_Transmit(&hospi1, (uint8_t *)buf, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
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
    OSPI_RegularCmdTypeDef cmd = {0};

    if (m >= W25Q64_READ_MODES)
    {
        return W25Q64_ERR_PARAM;
    }

    ospi_cmd_defaults(&cmd);
    cmd.AddressMode       = read_cfg[m].addr_lines;
    cmd.Address           = addr;
    cmd.AlternateBytesMode = read_cfg[m].alt_mode;
    cmd.AlternateBytes    = read_cfg[m].alt_bytes;
    cmd.DataMode          = read_cfg[m].data_lines;
    cmd.DummyCycles       = read_cfg[m].dummy;
    cmd.NbData            = len;
    cmd.Instruction       = read_cfg[m].instr;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_COMMAND;
    }
    if (HAL_OSPI_Receive(&hospi1, buf, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_RECEIVE;
    }
    return W25Q64_OK;
}

/* ------------------------------------------------------------------------ */
int w25q64_memmap_start(w25q64_read_mode_t m)
{
    OSPI_RegularCmdTypeDef cmd = {0};
    OSPI_MemoryMappedTypeDef mm = {0};

    if (m >= W25Q64_READ_MODES)
    {
        return W25Q64_ERR_PARAM;
    }

    ospi_cmd_defaults(&cmd);
    cmd.AddressMode       = read_cfg[m].addr_lines;
    cmd.AlternateBytesMode = read_cfg[m].alt_mode;
    cmd.AlternateBytes    = read_cfg[m].alt_bytes;
    cmd.DataMode          = read_cfg[m].data_lines;
    cmd.DummyCycles       = read_cfg[m].dummy;
    cmd.Instruction       = read_cfg[m].instr;

    /* Timeout counter DISABLED, matching ST's ExtMem_Boot: with TCEN enabled a
     * gap in memory-mapped reads can leave memmap mode mid-run and the next
     * code fetch bus-errors. */
    mm.TimeOutActivation = HAL_OSPI_TIMEOUT_COUNTER_DISABLE;
    mm.TimeOutPeriod     = 0;

    /* Make sure we are in command mode before switching to memory-mapped. */
    hospi1.State = HAL_OSPI_STATE_READY;

    if (HAL_OSPI_Command(&hospi1, &cmd, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
    {
        return W25Q64_ERR_MEMMAP;
    }
    if (HAL_OSPI_MemoryMapped(&hospi1, &mm) != HAL_OK)
    {
        return W25Q64_ERR_MEMMAP;
    }
    return W25Q64_OK;
}

void w25q64_memmap_stop(void)
{
    /* Leave memory-mapped mode. HAL_OSPI_Abort clears the BUSY flag and
     * returns the handle to READY. FMODE stays memory-mapped until the next
     * HAL_OSPI_Command(), whose OSPI_ConfigCmd() re-initializes
     * OCTOSPI_CR_FMODE back to indirect-write - so no register poking or
     * dangling data command is needed here (a command without a matching
     * Receive would leave the handle stuck in HAL_OSPI_STATE_CMD_CFG and make
     * every later HAL_OSPI_Command fail with INVALID_SEQUENCE). */
    (void)HAL_OSPI_Abort(&hospi1);
    hospi1.State = HAL_OSPI_STATE_READY;
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
