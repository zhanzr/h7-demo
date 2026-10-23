/*
  lcd.h - ST7735S 1.0" 128x96 LCD driver (h750-mini port, md100 module).
  Same wiring and driver family as the st7735_md144_128x128_mcudev project
  (identical J1B connections; the mcudev firmware largely works on this
  module directly).

  Panel geometry: ST7735S 128x96, MADCTL 0xC8. The visible window starts
  at GRAM column 2 / row 1 - the authoritative values from the vendor
  example (module_db/st7735s-md010-sqqvga3-st-01-v01, USER/main.h:
  COL_Pre = 2, ROW_Pre = 1). Note: raising ROW_Pre does not "skip dead
  rows" - it pushes the content down, and the noise band at the top is
  just the unwritten rows above the content, so it grows with the offset
  (observed 2 -> 3 -> 4 lines) while the picture bottom drops out of the
  window.
*/

#ifndef __LCD_H
#define __LCD_H

#include <stdint.h>
#include "stm32h7xx_hal.h"
#include "lcd/lcd_fonts.h"

/* ---- Panel geometry / demo timing ---- */
#define LCD_Width    128
#define LCD_Height   96
#define COL       128
#define ROW       96
/* Visible window starts at GRAM (2, 1) - vendor example values. */
#define COL_Pre   2
#define ROW_Pre   1
#define Delay_Time 500

/* ---- 4-wire SPI control pins (same J1B connections as the mcudev
 *      st7735_md144_128x128 project) ---- */
#define LCD_GPIO_PortSCL    GPIOE
#define LCD_SCL_Pin         GPIO_PIN_12  /* J1B 35, SPI4_SCK AF5 */
#define LCD_GPIO_PortSDA    GPIOE
#define LCD_SDA_Pin         GPIO_PIN_14  /* J1B 33, SPI4_MOSI AF5 */
#define LCD_GPIO_PortRS     GPIOE
#define LCD_RS_Pin          GPIO_PIN_15  /* J1B 32, DC  */
#define LCD_GPIO_PortRST    GPIOE
#define LCD_RST_Pin         GPIO_PIN_13  /* J1B 34 */
#define LCD_GPIO_PortCS     GPIOE
#define LCD_CS_Pin          GPIO_PIN_11  /* J1B 36 */
#define LCD_GPIO_PortBL     GPIOD
#define LCD_BL_Pin          GPIO_PIN_15  /* J1B 18, TIM4_CH4 backlight PWM */

/* ---- pin accessors (bit-banged SPI) ---- */
#define LCD_SPI_SCL_SET  HAL_GPIO_WritePin(LCD_GPIO_PortSCL, LCD_SCL_Pin, GPIO_PIN_SET)
#define LCD_SPI_SCL_CLR  HAL_GPIO_WritePin(LCD_GPIO_PortSCL, LCD_SCL_Pin, GPIO_PIN_RESET)
#define LCD_SPI_SDA_SET  HAL_GPIO_WritePin(LCD_GPIO_PortSDA, LCD_SDA_Pin, GPIO_PIN_SET)
#define LCD_SPI_SDA_CLR  HAL_GPIO_WritePin(LCD_GPIO_PortSDA, LCD_SDA_Pin, GPIO_PIN_RESET)
#define LCD_RS_SET       HAL_GPIO_WritePin(LCD_GPIO_PortRS,  LCD_RS_Pin,  GPIO_PIN_SET)
#define LCD_RS_CLR       HAL_GPIO_WritePin(LCD_GPIO_PortRS,  LCD_RS_Pin,  GPIO_PIN_RESET)
#define LCD_RST_SET      HAL_GPIO_WritePin(LCD_GPIO_PortRST, LCD_RST_Pin, GPIO_PIN_SET)
#define LCD_RST_CLR      HAL_GPIO_WritePin(LCD_GPIO_PortRST, LCD_RST_Pin, GPIO_PIN_RESET)
#define LCD_CS_SET       HAL_GPIO_WritePin(LCD_GPIO_PortCS,  LCD_CS_Pin,  GPIO_PIN_SET)
#define LCD_CS_CLR       HAL_GPIO_WritePin(LCD_GPIO_PortCS,  LCD_CS_Pin,  GPIO_PIN_RESET)

/* ---- 24-bit colors (RGB888, converted to RGB565 by LCD_SetColor) ---- */
#define LCD_WHITE       0xFFFFFF
#define LCD_BLACK       0x000000
#define LCD_BLUE        0x0000FF
#define LCD_GREEN       0x00FF00
#define LCD_RED         0xFF0000
#define LCD_CYAN        0x00FFFF
#define LCD_MAGENTA     0xFF00FF
#define LCD_YELLOW      0xFFFF00

/* ---- raw RGB565 colors (vendor screens use these directly) ---- */
#define WHITE   0xFFFF
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define MAGENTA 0xF81F
#define GREEN   0x07E0
#define CYAN    0x7FFF
#define YELLOW  0xFFE0

#define ABS(X)  ((X) > 0 ? (X) : -(X))

/* ---- API: init / window / colors ---- */
void LCD_GPIOInit(void);
void LCD_RESET(void);
void LCD_IC_Init(void);
void LCD_Init(void);
void LCD_Reinit(void);   /* reset + re-init after a bus switch */
void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_SetColor(uint32_t rbg888);
void LCD_SetBackColor(uint32_t rbg888);
void LCD_Clear(void);
void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/* ---- API: vendor screen-window helper (raw, absolute coords) ---- */
void BlockWrite(uint16_t Xstart, uint16_t Xend, uint16_t Ystart, uint16_t Yend);

/* ---- API: vendor demo screens ---- */
void DispColor(uint32_t color);
void DispFrame(void);
void DispGrayHor16(void);
void DispBand(void);
void StopDelay(uint16_t ms);

/* ---- API: ASCII text ---- */
void LCD_SetAsciiFont(pFONT *font);
void LCD_ShowTransparent(uint8_t mode);
void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t c);
void LCD_DisplayString(uint16_t x, uint16_t y, char *p);

/* ---- API: 2D drawing ---- */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color);
void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height);
void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r);

/* ---- API: raw buffer blit (RGB565 words) ---- */
void LCD_CopyBuffer(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    uint16_t *data);

#endif /* __LCD_H */
