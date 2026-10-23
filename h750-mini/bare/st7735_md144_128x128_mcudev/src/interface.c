/*
  interface.c - low-level ST7735S bus primitives (h750-mini port, mcudev).
  MERGE version: one wiring (SCL=PE12, SDA=PE14, both on the J1B header),
  two drive methods selected at runtime:

    SOFT: PE12/PE14 re-muxed to plain GPIO outputs and bit-banged (the
          proven path; DC selects command vs data, CS asserted per
          transfer).
    HW  : PE12/PE14 re-muxed to SPI4 AF5 (SCK/MOSI), master, simplex TX
          (1LINE), 8-bit, mode 0 (CPOL=0, CPHA=1EDGE), MSB first, NSS
          soft. CS=PE11 / DC=PE15 / RES=PE13 stay GPIO.
          SPI4 kernel clock = PCLK2 = 120 MHz (board 480 MHz tree); the
          default prescaler is /8 = 15 MHz SCK - the ST7735S datasheet
          write-cycle limit (66 ns); see the LCD_SPI4_PRESC note below.

          Mode 0 is REQUIRED here, not cosmetic. HAL_SPI_Transmit() ends
          in SPI_CloseTransfer(), which clears SPE, so every single
          transfer re-enables the peripheral. With CPOL=1 the SCK pin
          snaps from its disabled level (low) up to its idle level (high)
          at that moment - with CS already asserted - and the panel
          latches that extra rising edge as a 9th bit. Every byte then
          arrives bit-shifted, so no command (0x11 SLPOUT) ever lands and
          the module stays all-white even though the display is
          re-initialised on every bus switch. With CPOL=0 SCK is low in
          both states, so no spurious edge is produced. (The bit-bang
          path idles SCL high but only raises it with SDA already stable,
          so it was immune.)

  Raster bursts in HW mode stream through a 512-byte TX buffer (one
  HAL_SPI_Transmit per <=512 bytes); in SOFT mode bytes go straight to the
  bit-bang.  CS is asserted per transfer, DC selects command vs data.
*/

#include "interface.h"
#include <stdio.h>
#include "lcd.h"
#include "stm32h7xx_hal.h"

/* Bytes of pixel data accumulated before one HAL_SPI_Transmit call (HW). */
#define SPI_TX_BUF_SIZE 512U

/* SPI4 baud prescaler: PCLK2 = 120 MHz (480 MHz board tree).
 *
 * 15 MHz (/8) is the default: that is the ST7735S datasheet write-cycle
 * limit (tSCYCW = 66 ns), so the panel is comfortably in spec while still
 * being twice the original 7.5 MHz. Ladder:
 *   /2  = 60 MHz   too fast in practice (out of spec, 4x over the limit)
 *   /4  = 30 MHz   still 2x over the datasheet limit - only for testing
 *   /8  = 15 MHz   default, in spec
 *   /16 = 7.5 MHz  very conservative, previous default
 *   /32 = 3.75 MHz only for long / loose wiring
 * The old "all-white at 15 MHz" observation was the CPOL=1 clock-edge bug,
 * not a speed limit. */
#ifndef LCD_SPI4_PRESC
#define LCD_SPI4_PRESC SPI_BAUDRATEPRESCALER_8
#endif

static uint8_t           s_tx_buf[SPI_TX_BUF_SIZE];
static uint16_t          s_tx_len;
static SPI_HandleTypeDef s_hspi;
static uint8_t           s_spi_ready;
static uint8_t           s_bus_hw;       /* 0 = soft (GPIO), 1 = HW SPI4    */

void CS_SET(void)
{
    LCD_CS_SET;
}
void CS_CLR(void)
{
    LCD_CS_CLR;
}

/* Configure + init SPI4 (once). GPIO re-mux happens in LCD_UseHwBus.
 * The Init field set and the explicit SPI45 kernel-clock config mirror the
 * working st7789_md200 project's SPI4 (same pins, same board). */
static void spi_hw_init(void)
{
    RCC_PeriphCLKInitTypeDef periph = {0};

    if (s_spi_ready != 0U)
    {
        return;
    }

    /* SPI4 kernel clock: D2PCLK1 (PCLK2, 120 MHz) - the mux reset default
     * is the same, but the st7789 demo sets it explicitly, so do not rely
     * on the default. */
    periph.PeriphClockSelection = RCC_PERIPHCLK_SPI4;
    periph.Spi45ClockSelection  = RCC_SPI45CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK)
    {
        printf("[LCD] SPI4 kernel clock config FAILED\r\n");
    }

    __HAL_RCC_SPI4_CLK_ENABLE();

    s_hspi.Instance               = SPI4;
    s_hspi.Init.Mode              = SPI_MODE_MASTER;
    s_hspi.Init.Direction         = SPI_DIRECTION_1LINE;      /* TX only */
    s_hspi.Init.DataSize          = SPI_DATASIZE_8BIT;
    /* Mode 0, NOT mode 3: HAL_SPI_Transmit() disables SPE when it returns,
     * so CPOL=1 would add a spurious rising SCK edge at every re-enable
     * (CS already low) and shift every byte by one bit. See the file
     * header. The ST7735S latches on the rising SCK edge in either mode. */
    s_hspi.Init.CLKPolarity       = SPI_POLARITY_LOW;         /* mode 0  */
    s_hspi.Init.CLKPhase          = SPI_PHASE_1EDGE;
    s_hspi.Init.NSS               = SPI_NSS_SOFT;
    s_hspi.Init.BaudRatePrescaler = LCD_SPI4_PRESC;
    s_hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    s_hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
    s_hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    s_hspi.Init.CRCPolynomial     = 7U;
    s_hspi.Init.NSSPMode          = SPI_NSS_PULSE_ENABLE;
    s_hspi.Init.NSSPolarity       = SPI_NSS_POLARITY_LOW;
    s_hspi.Init.FifoThreshold     = SPI_FIFO_THRESHOLD_02DATA;
    s_hspi.Init.TxCRCInitializationPattern =
        SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    s_hspi.Init.RxCRCInitializationPattern =
        SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    s_hspi.Init.MasterSSIdleness       = SPI_MASTER_SS_IDLENESS_00CYCLE;
    s_hspi.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    s_hspi.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    /* Keep the AF pins driven at their idle level even while the peripheral
     * is disabled (AFCNTR=1). HAL_SPI_Transmit() clears SPE before it
     * returns, so with AFCNTR=0 SCK/MOSI are released after every single
     * transfer and re-driven at the next one - while CS is already low.
     * Keeping them driven removes that low/high transition (verified on
     * silicon: CFG2/CR1 showed AFCNTR=0, SPIEN=0 after each transfer). */
    s_hspi.Init.MasterKeepIOState      = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    s_hspi.Init.IOSwap                 = SPI_IO_SWAP_DISABLE;
    if (HAL_SPI_Init(&s_hspi) != HAL_OK)
    {
        printf("[LCD] HAL_SPI_Init FAILED\r\n");
    }
    s_spi_ready = 1U;
}

/* Re-apply the SPI4 config with the project-selected baud prescaler. */
void SPI_HW_SetSpeed(void)
{
    if (s_spi_ready == 0U)
    {
        return;
    }
    (void)HAL_SPI_DeInit(&s_hspi);
    s_hspi.Init.BaudRatePrescaler = LCD_SPI4_PRESC;
    (void)HAL_SPI_Init(&s_hspi);
}

/* Push all buffered bytes through SPI4 (blocking). */
void SPI_HW_Flush(void)
{
    if (s_tx_len != 0U)
    {
        if (HAL_SPI_Transmit(&s_hspi, s_tx_buf, (uint32_t)s_tx_len,
                             HAL_MAX_DELAY) != HAL_OK)
        {
            printf("[LCD] SPI TX FAIL len=%lu\r\n", (unsigned long)s_tx_len);
        }
        s_tx_len = 0U;
    }
}

/* ---------------- bus selection ------------------------------------- */

void LCD_UseSoftBus(void)
{
    GPIO_InitTypeDef g;

    SPI_HW_Flush();
    s_bus_hw = 0U;

    __HAL_RCC_GPIOE_CLK_ENABLE();
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pull  = GPIO_NOPULL;
    g.Pin   = LCD_SCL_Pin | LCD_SDA_Pin;
    HAL_GPIO_Init(LCD_GPIO_PortSCL, &g);
    /* idle SCL/SDA high */
    LCD_SPI_SCL_SET;
    LCD_SPI_SDA_SET;
}

void LCD_UseHwBus(void)
{
    GPIO_InitTypeDef g;

    s_bus_hw = 1U;

    __HAL_RCC_GPIOE_CLK_ENABLE();
    g.Mode      = GPIO_MODE_AF_PP;
    /* VERY_HIGH is harmless at 15 MHz and still supports running the bus
     * faster for experiments: OSPEEDR steps on this part are ~8 MHz (low),
     * ~25 MHz (medium), ~50 MHz (high), ~100 MHz (very high). MEDIUM is
     * enough for 15 MHz but leaves no headroom for LCD_SPI4_PRESC = /4. */
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Pull      = GPIO_NOPULL;
    g.Alternate = GPIO_AF5_SPI4;         /* PE12 = SCK, PE14 = MOSI */
    g.Pin       = LCD_SCL_Pin | LCD_SDA_Pin;
    HAL_GPIO_Init(LCD_GPIO_PortSCL, &g);

    spi_hw_init();
}

uint8_t LCD_BusIsHw(void)
{
    return s_bus_hw;
}

/* Active SPI4 baud in kHz (for the info page), e.g. 15000 = 15 MHz. */
unsigned long LCD_HwSpiKHz(void)
{
    uint32_t div = 2U << ((s_hspi.Init.BaudRatePrescaler & SPI_CFG1_MBR) >>
                          SPI_CFG1_MBR_Pos);
    return (unsigned long)(HAL_RCC_GetPCLK2Freq() / div / 1000U);
}

/* ---------------- SOFT path: bit-bang PE12/PE14 ---------------------- */

static void soft_send_byte(uint8_t dat)
{
    for (int i = 0; i < 8; i++)
    {
        if ((dat & 0x80U) != 0U)
        {
            LCD_SPI_SDA_SET;
        }
        else
        {
            LCD_SPI_SDA_CLR;
        }
        dat <<= 1;
        LCD_SPI_SCL_CLR;
        LCD_SPI_SCL_SET;
    }
}

/* ---------------- byte-level transfers ------------------------------- */

/* Send one byte immediately (commands): bus-dependent, no buffering. */
static void spi_send_now(uint8_t dat)
{
    if (s_bus_hw != 0U)
    {
        uint8_t b = dat;
        /* Report failures: a dropped command byte leaves the panel in its
         * power-on (all-white) state with no other visible symptom. */
        if (HAL_SPI_Transmit(&s_hspi, &b, 1U, HAL_MAX_DELAY) != HAL_OK)
        {
            printf("[LCD] SPI TX FAIL (cmd) err=%lu SR=%08lX\r\n",
                   (unsigned long)HAL_SPI_GetError(&s_hspi),
                   (unsigned long)SPI4->SR);
        }
    }
    else
    {
        soft_send_byte(dat);
    }
}

/* Queue/send one data byte (DC already high). */
static void spi_put(uint8_t dat)
{
    if (s_bus_hw == 0U)
    {
        soft_send_byte(dat);
        return;
    }
    s_tx_buf[s_tx_len++] = dat;
    if (s_tx_len >= SPI_TX_BUF_SIZE)
    {
        SPI_HW_Flush();
    }
}

/* Write a register/command: DC low. */
void WriteComm(uint16_t data)
{
    SPI_HW_Flush();       /* drain pending data bytes (DC high) first */
    LCD_CS_CLR;
    LCD_RS_CLR;
    spi_send_now((uint8_t)data);
    LCD_CS_SET;
}

/* Write a data byte: DC high. */
void WriteData(uint16_t data)
{
    SPI_HW_Flush();
    LCD_CS_CLR;
    LCD_RS_SET;
    spi_send_now((uint8_t)data);
    LCD_CS_SET;
}

/* Stream one RGB565 pixel color as two 8-bit writes (DC high persists). */
void SendData(uint32_t color)
{
    LCD_CS_CLR;
    LCD_RS_SET;
    spi_put((uint8_t)(color >> 8));
    spi_put((uint8_t)color);
    SPI_HW_Flush();
    LCD_CS_SET;
}

/* Fast raw 8-bit data byte: no CS/DC toggling (caller manages both). */
void LCD_WriteDataFast(uint8_t data)
{
    spi_put(data);
}

/* Begin/end a data-gram burst for raster fills: DC high, CS held low. */
void LCD_BeginData(void)
{
    SPI_HW_Flush();       /* never mix a command byte into a data burst */
    LCD_CS_CLR;
    LCD_RS_SET;
}
void LCD_EndData(void)
{
    SPI_HW_Flush();
    LCD_CS_SET;
}
