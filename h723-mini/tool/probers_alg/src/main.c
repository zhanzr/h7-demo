/**
 * ospi_alg_test - minimal: after Init, immediately erase+program via the
 * algorithm's register-level code and read back via the HAL.
 */

#include <stdio.h>
#include "board.h"
#include "uart_printf.h"
#include "stm32h7xx_hal.h"

#include "../../qspi_map/algo/flash_w25q64_ospi.c"

static OSPI_HandleTypeDef hhal;
void HAL_OSPI_MspInit(OSPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef gpio = {0};
    if (hspi->Instance == OCTOSPI1)
    {
        __HAL_RCC_OCTOSPIM_CLK_ENABLE();
        __HAL_RCC_OSPI1_CLK_ENABLE();
        __HAL_RCC_GPIOF_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
        gpio.Mode = GPIO_MODE_AF_PP; gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
        gpio.Alternate = GPIO_AF10_OCTOSPIM_P1; HAL_GPIO_Init(GPIOF, &gpio);
        gpio.Pin = GPIO_PIN_10; gpio.Alternate = GPIO_AF9_OCTOSPIM_P1;
        HAL_GPIO_Init(GPIOF, &gpio);
        gpio.Pin = GPIO_PIN_6; gpio.Alternate = GPIO_AF10_OCTOSPIM_P1;
        HAL_GPIO_Init(GPIOG, &gpio);
    }
}

static void hal_ospi_setup(void)
{
    hhal.Instance = OCTOSPI1;
    hhal.Init.ClockPrescaler = 2; hhal.Init.FifoThreshold = 8;
    hhal.Init.DualQuad = HAL_OSPI_DUALQUAD_DISABLE;
    hhal.Init.MemoryType = HAL_OSPI_MEMTYPE_MICRON;
    hhal.Init.DeviceSize = 23; hhal.Init.ChipSelectHighTime = 1;
    hhal.Init.FreeRunningClock = HAL_OSPI_FREERUNCLK_DISABLE;
    hhal.Init.ClockMode = HAL_OSPI_CLOCK_MODE_3;
    hhal.Init.WrapSize = HAL_OSPI_WRAP_NOT_SUPPORTED;
    hhal.Init.SampleShifting = HAL_OSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hhal.Init.DelayHoldQuarterCycle = HAL_OSPI_DHQC_DISABLE;
    hhal.Init.ChipSelectBoundary = 0;
    hhal.Init.DelayBlockBypass = HAL_OSPI_DELAY_BLOCK_BYPASSED;
    hhal.Init.MaxTran = 0; hhal.Init.Refresh = 0;
    HAL_OSPI_Init(&hhal);
    { OSPIM_CfgTypeDef m = {0}; m.ClkPort = 1; m.NCSPort = 1;
      m.IOLowPort = HAL_OSPIM_IOPORT_1_LOW;
      HAL_OSPIM_Config(&hhal, &m, HAL_OSPI_TIMEOUT_DEFAULT_VALUE); }
}

static void hal_read(uint32_t addr, uint8_t *out, uint32_t n)
{
    OSPI_RegularCmdTypeDef hr = {0};
    hr.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    hr.FlashId = HAL_OSPI_FLASH_ID_1;
    hr.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
    hr.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
    hr.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    hr.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
    hr.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
    hr.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
    hr.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    hr.DataMode = HAL_OSPI_DATA_1_LINE;
    hr.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
    hr.NbData = n;
    hr.DummyCycles = 0;
    hr.DQSMode = HAL_OSPI_DQS_DISABLE;
    hr.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    hr.Instruction = 0x03;
    hr.Address = addr;
    if (HAL_OSPI_Command(&hhal, &hr, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) == HAL_OK)
        HAL_OSPI_Receive(&hhal, out, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);
}

int main(void)
{
    unsigned char wb[16], rb[16];
    unsigned long i;
    int rc;

    HAL_Init();
    __enable_irq();
    UART_Init();
    printf("\r\n=== OSPI algorithm write test ===\r\n");

    rc = Init(0, 0, 0);
    printf("Init rc=%d, JEDEC=0x%06lX\r\n", rc, (unsigned long)w25q_read_id());
    hal_ospi_setup();

    for (i = 0; i < 16; i++) { wb[i] = (unsigned char)(0x50 + i); }

    rc = EraseSector(0x90000000u + 0x400000);
    printf("EraseSector @0x400000: rc=%d\r\n", rc);
    rc = ProgramPage(0x90000000u + 0x400000, 16, wb);
    printf("ProgramPage 16B @0x400000: rc=%d\r\n", rc);

    hal_read(0x400000, rb, 16);
    printf("HAL read @0x400000=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
           rb[0], rb[1], rb[2], rb[3], rb[4], rb[5], rb[6], rb[7]);

    printf("DONE\r\n");
    while (1) { }
}
