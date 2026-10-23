/*
  interface.h - low-level ST7735S bus primitives (h750-mini port, md100).
  Same bus config as the mcudev md144 project (the bus is identical); one
  wiring (SCL=PE12, SDA=PE14 on the J1B header), two drive methods
  selected at runtime:

    SOFT: PE12/PE14 re-muxed to plain GPIO outputs and bit-banged (the
          proven path; DC selects command vs data, CS asserted per
          transfer).
    HW  : PE12/PE14 re-muxed to SPI4 AF5 (SCK / MOSI), **mode 0**
          (CPOL=0, CPHA=1EDGE - the ST7735S latches on the rising edge),
          MSB first, 8-bit, software NSS. CS=PE11 / DC=PE15 / RES=PE13
          stay GPIO. SPI4 from PCLK2 = 120 MHz, /8 = 15 MHz SCK (the
          ST7735S datasheet write-cycle limit).
          Mode 0 is mandatory: HAL_SPI_Transmit() clears SPE before it
          returns, and with CPOL=1 the re-enable would drive an extra
          rising SCK edge while CS is low, bit-shifting every byte (the
          panel then stays all-white because 0x11 SLPOUT never lands).

  Pin roles:
    SCL = PE12 (J1B 35)  (SPI clock, bit-banged or SPI4_SCK AF5)
    SDA = PE14 (J1B 33)  (SPI MOSI, bit-banged or SPI4_MOSI AF5)
    DC  = PE15 (J1B 32)  (data/command, always GPIO)
    RES = PE13 (J1B 34)  (reset, always GPIO)
    CS  = PE11 (J1B 36)  (chip select, always GPIO)
*/

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include <stdint.h>

void CS_SET(void);
void CS_CLR(void);
void WriteComm(uint16_t data);
void WriteData(uint16_t data);
void SendData(uint32_t color);
void LCD_WriteDataFast(uint8_t data);   /* raw byte, caller manages CS/DC */
void LCD_BeginData(void);                /* DC high, CS low, for raster bursts */
void LCD_EndData(void);                  /* CS high after a burst */

/* ---- bus selection (SOFT bit-bang vs HW SPI4) ---- */
void    LCD_UseSoftBus(void);   /* re-mux PE12/PE14 to GPIO, idle high    */
void    LCD_UseHwBus(void);     /* re-mux PE12/PE14 to SPI4 AF5 + init    */
uint8_t LCD_BusIsHw(void);      /* 1 while the HW SPI4 bus is selected    */
unsigned long LCD_HwSpiKHz(void); /* active SPI4 baud in kHz (info page)  */
void    SPI_HW_SetSpeed(void);  /* (re)apply the configured SPI prescaler */
void    SPI_HW_Flush(void);     /* drain the HW TX buffer (blocking)      */

#endif /* __INTERFACE_H */
