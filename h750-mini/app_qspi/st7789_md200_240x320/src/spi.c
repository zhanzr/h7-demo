/**
  * @file    spi.c
  * @brief   SPI4 init for the ST7789 panel (240x320, SPI mode 0, 60 MHz SCK).
  *
  * Mirrors MX_SPI4_Init from the CubeMX project (cubemx_file/):
  *   PE11 NSS / PE12 SCK / PE14 MOSI (AF5), hardware NSS, kernel clock =
  *   D2PCLK1 (120 MHz) divided by 2 -> 60 MHz SCK. TX-only (no MISO).
  */

#include "spi.h"
#include "main.h"
#include "board.h"

SPI_HandleTypeDef hspi4;

/* ------------------------------------------------------------------------ */
void MX_SPI4_Init(void)
{
    hspi4.Instance                     = SPI4;
    hspi4.Init.Mode                    = SPI_MODE_MASTER;
    hspi4.Init.Direction               = SPI_DIRECTION_1LINE;
    hspi4.Init.DataSize                = SPI_DATASIZE_8BIT;
    hspi4.Init.CLKPolarity             = SPI_POLARITY_LOW;
    hspi4.Init.CLKPhase                = SPI_PHASE_1EDGE;
    hspi4.Init.NSS                     = SPI_NSS_HARD_OUTPUT;
    hspi4.Init.BaudRatePrescaler       = SPI_BAUDRATEPRESCALER_2;
    hspi4.Init.FirstBit                = SPI_FIRSTBIT_MSB;
    hspi4.Init.TIMode                  = SPI_TIMODE_DISABLE;
    hspi4.Init.CRCCalculation          = SPI_CRCCALCULATION_DISABLE;
    hspi4.Init.CRCPolynomial           = 0x0;
    hspi4.Init.NSSPMode                = SPI_NSS_PULSE_ENABLE;
    hspi4.Init.NSSPolarity             = SPI_NSS_POLARITY_LOW;
    hspi4.Init.FifoThreshold           = SPI_FIFO_THRESHOLD_02DATA;
    hspi4.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi4.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi4.Init.MasterSSIdleness        = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi4.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi4.Init.MasterReceiverAutoSusp  = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi4.Init.MasterKeepIOState       = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi4.Init.IOSwap                  = SPI_IO_SWAP_DISABLE;

    if (HAL_SPI_Init(&hspi4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------------ */
void HAL_SPI_MspInit(SPI_HandleTypeDef *spiHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (spiHandle->Instance == SPI4)
    {
        /* SPI4 kernel clock: D2PCLK1 (120 MHz) -> /2 -> 60 MHz SCK. */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI4;
        PeriphClkInitStruct.Spi45ClockSelection  = RCC_SPI45CLKSOURCE_D2PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_RCC_SPI4_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();

        GPIO_InitStruct.Pin       = LCD_CS_Pin | LCD_SCL_Pin | LCD_SDA_Pin;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI4;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    }
}
