#ifndef __spi_lcd
#define __spi_lcd

#include "stm32h7xx_hal.h"
#include "usart.h"

#include "lcd_fonts.h"	// Image and font files are optional; users may remove them as needed

/*----------------------------------------------- Parameter macros -------------------------------------------*/

#define LCD_Width     240		// LCD pixel width
#define LCD_Height    320		// LCD pixel height

// Display direction parameters
// Usage: LCD_SetDirection(Direction_H) for landscape mode
#define	Direction_H				0					//Landscape
#define	Direction_H_Flip	   1					//Landscape, flipped vertically
#define	Direction_V				2					//Portrait 
#define	Direction_V_Flip	   3					//Portrait, flipped vertically 

// Set whether extra digits are padded with 0 or spaces when displaying variables
// Used only by LCD_DisplayNumber() (integers) and LCD_DisplayDecimals() (decimals)
// Usage: LCD_ShowNumMode(Fill_Zero) pads with 0, e.g. 123 becomes 000123
#define  Fill_Zero  0		//Fill with 0
#define  Fill_Space 1		//Fill with space


/*---------------------------------------- Common colors ------------------------------------------------------

 1. For convenience, colors are defined as 24-bit RGB888 and automatically converted to 16-bit RGB565 in code
 2. In the 24-bit value, high-to-low bits correspond to the R, G, B color channels
 3. Get a 24-bit RGB color from a PC color picker, then pass it to LCD_SetColor() or LCD_SetBackColor() to display it
 */                                                  						
#define 	LCD_WHITE       0xFFFFFF	 // Pure white
#define 	LCD_BLACK       0x000000    // Pure black
                        
#define 	LCD_BLUE        0x0000FF	 //	Pure blue
#define 	LCD_GREEN       0x00FF00    //	Pure green
#define 	LCD_RED         0xFF0000    //	Pure red
#define 	LCD_CYAN        0x00FFFF    //	Cyan
#define 	LCD_MAGENTA     0xFF00FF    //	Magenta
#define 	LCD_YELLOW      0xFFFF00    //	Yellow
#define 	LCD_GREY        0x2C2C2C    //	Grey
												
#define 	LIGHT_BLUE      0x8080FF    //	Light blue
#define 	LIGHT_GREEN     0x80FF80    //	Light green
#define 	LIGHT_RED       0xFF8080    //	Light red
#define 	LIGHT_CYAN      0x80FFFF    //	Light cyan
#define 	LIGHT_MAGENTA   0xFF80FF    //	Light magenta
#define 	LIGHT_YELLOW    0xFFFF80    //	Light yellow
#define 	LIGHT_GREY      0xA3A3A3    //	Light grey
												
#define 	DARK_BLUE       0x000080    //	Dark blue
#define 	DARK_GREEN      0x008000    //	Dark green
#define 	DARK_RED        0x800000    //	Dark red
#define 	DARK_CYAN       0x008080    //	Dark cyan
#define 	DARK_MAGENTA    0x800080    //	Dark magenta
#define 	DARK_YELLOW     0x808000    //	Dark yellow
#define 	DARK_GREY       0x404040    //	Dark grey


/*------------------------------------------------ Function declarations ----------------------------------------------*/

void  SPI_LCD_Init(void);      // LCD and SPI initialization   
void  LCD_Clear(void);			 // Clear screen
void  LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);	// Clear a screen region

void  LCD_SetAddress(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);	// Set coordinates		
void  LCD_SetColor(uint32_t Color); 				   //	Set pen color
void  LCD_SetBackColor(uint32_t Color);  				//	Set background color
void  LCD_SetDirection(uint8_t direction);  	      //	Set display direction

//>>>>>	Display ASCII characters
void  LCD_SetAsciiFont(pFONT *fonts);										//	Set ASCII font
void 	LCD_DisplayChar(uint16_t x, uint16_t y,uint8_t c);				//	Display a single ASCII character
void 	LCD_DisplayString( uint16_t x, uint16_t y, char *p);	 		//	Display ASCII string

//>>>>>	Display Chinese characters, including ASCII
void 	LCD_SetTextFont(pFONT *fonts);										// Set text font, including Chinese and ASCII
void 	LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText);		// Display a single Chinese character
void 	LCD_DisplayText(uint16_t x, uint16_t y, char *pText) ;		// Display string, including Chinese and ASCII characters

//>>>>>	Display integers or decimals
void  LCD_ShowNumMode(uint8_t mode);		// Set variable display mode, pad with spaces or 0
void  LCD_ShowTransparent(uint8_t mode);	// Transparent text mode: only draw foreground pixels, background shows through
void  LCD_DisplayNumber( uint16_t x, uint16_t y, int32_t number,uint8_t len) ;					// Display integer
void  LCD_DisplayDecimals( uint16_t x, uint16_t y, double number,uint8_t len,uint8_t decs);	// Display decimal

//>>>>>	2D graphics functions
void  LCD_DrawPoint(uint16_t x,uint16_t y,uint32_t color);   	//Draw a point

void  LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height);          // Draw a vertical line
void  LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width);           // Draw a horizontal line
void  LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);	// Draw a line between two points

void  LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);			//Draw a rectangle
void  LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r);									//Draw a circle
void  LCD_DrawEllipse(int x, int y, int r1, int r2);											//Draw an ellipse

//>>>>>	Area fill functions
void  LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);			//Fill a rectangle
void  LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r);									//Fill a circle

//>>>>>	Draw monochrome image
void 	LCD_DrawImage(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint8_t *pImage)  ;

//>>>>>	Bulk copy function, copies data directly to the LCD framebuffer
void	LCD_CopyBuffer(uint16_t x, uint16_t y,uint16_t width,uint16_t height,uint16_t *DataBuff);

 /*--------------------------------------------- Other LCD pins -----------------------------------------------*/
#define  LCD_DC_PIN						GPIO_PIN_15				         // Data/command select pin				
#define	LCD_DC_PORT						GPIOE									// Data/command select GPIO port
#define 	GPIO_LDC_DC_CLK_ENABLE     __HAL_RCC_GPIOE_CLK_ENABLE()	// Data/command select GPIO clock	

#define	LCD_DC_Command		   HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET);	   // Low level, command transfer 
#define 	LCD_DC_Data		      HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);		// High level, data transfer

void lcd_bl_bright_set(uint16_t duty);

#endif //__spi_lcd




