#include <stdio.h>
#include "tim.h"

#include "lcd_spi_200.h"

extern SPI_HandleTypeDef hspi4;	      // SPI_HandleTypeDef structure variable

#define  LCD_SPI hspi4           // SPI local macro, convenient for modification and porting

static pFONT *LCD_AsciiFonts;		// ASCII font set
static pFONT *LCD_CHFonts;		   // Chinese font (also includes ASCII fonts)

// Because updating this SPI LCD requires setting the address region first then writing to the framebuffer,
// writing coordinates and framebuffer pixel by pixel would be very slow.
// Therefore a buffer is allocated: data is written to it first, then flushed to the framebuffer in bulk.
// Users can adjust the buffer size according to their needs.
// For example, displaying a 32*32 Chinese character needs 32*32*2 = 2048 bytes (each pixel is 2 bytes)
uint16_t  LCD_Buff[1024];        // LCD buffer, 16-bit wide (each pixel is 2 bytes)

struct	//LCD parameter structure
{
	uint32_t Color;  				//	LCD current pen color
	uint32_t BackColor;			//	background color
   uint8_t  ShowNum_Mode;		// Number display mode
	uint8_t  Transparent;			// Transparent text mode: only draw foreground pixels
	uint8_t  Direction;			//	Display direction
   uint16_t Width;            // Screen width in pixels
   uint16_t Height;           // Screen height in pixels	
   uint8_t  X_Offset;         // X offset for setting the controller framebuffer write mode
   uint8_t  Y_Offset;         // Y offset for setting the controller framebuffer write mode
}LCD;

// This function is modified from the HAL SPI library function, specifically for LCD_Clear().
// It is intended to allow SPI transfers of unlimited data length
HAL_StatusTypeDef LCD_SPI_Transmit(SPI_HandleTypeDef *hspi, uint16_t pData, uint32_t Size);
HAL_StatusTypeDef LCD_SPI_TransmitBuffer (SPI_HandleTypeDef *hspi, uint16_t *pData, uint32_t Size);

/****************************************************************************************************************************************
*	Function name:	HAL_SPI_MspInit
*	Input:	hspi - SPI_HandleTypeDef variable, i.e. the SPI handle
*	Function:	Initialize SPI pins
****************************************************************************************************************************************/

//void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
//{
//   GPIO_InitTypeDef GPIO_InitStruct = {0};
//   if(hspi->Instance==SPI4)
//   {
//		__HAL_RCC_SPI4_CLK_ENABLE();			// Enable SPI4 clock

//		__HAL_RCC_GPIOE_CLK_ENABLE();		// Enable SPI4 GPIO
//		
//      GPIO_LDC_Backlight_CLK_ENABLE;   // Enable backlight        pin clock
//      GPIO_LDC_DC_CLK_ENABLE;          // Enable data/command select pin clock

///******************************************************  
//		PE11     ------> SPI4_NSS, using hardware chip select
//		PE12     ------> SPI4_SCK
//		PE14     ------> SPI4_MOSI
//		
//      PD15     ------> backlight pin
//      PE15     ------> data/command select pin
//*******************************************************/

//// Initialize SCK, MOSI and chip select pins using hardware SPI chip select
//      GPIO_InitStruct.Pin 		   = GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_14; // SCK, MOSI and chip select pins
//      GPIO_InitStruct.Mode 		= GPIO_MODE_AF_PP;            			// Alternate function push-pull output
//      GPIO_InitStruct.Pull 		= GPIO_NOPULL;                			// No pull-up or pull-down
//      GPIO_InitStruct.Speed 		= GPIO_SPEED_FREQ_VERY_HIGH;  			// Highest speed grade
//      GPIO_InitStruct.Alternate  = GPIO_AF5_SPI4;              			// Remapped to SPI4, AF line 1
//      HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
//  
//// Initialize backlight pin  
//		GPIO_InitStruct.Pin 		= LCD_Backlight_PIN;				// backlight pin
//		GPIO_InitStruct.Mode 	= GPIO_MODE_OUTPUT_PP;			// push-pull output mode
//		GPIO_InitStruct.Pull 	= GPIO_NOPULL;						// No pull-up or pull-down
//		GPIO_InitStruct.Speed 	= GPIO_SPEED_FREQ_LOW;			// low speed grade
//		HAL_GPIO_Init(LCD_Backlight_PORT, &GPIO_InitStruct);	// Initialize  

//// Initialize data/command select pin  
//		GPIO_InitStruct.Pin 		= LCD_DC_PIN;				      // data/command select pin
//		GPIO_InitStruct.Mode 	= GPIO_MODE_OUTPUT_PP;			// push-pull output mode
//		GPIO_InitStruct.Pull 	= GPIO_NOPULL;						// No pull-up or pull-down
//		GPIO_InitStruct.Speed 	= GPIO_SPEED_FREQ_LOW;			// low speed grade
//		HAL_GPIO_Init(LCD_DC_PORT, &GPIO_InitStruct);	      // Initialize  
//   }
//}

/****************************************************************************************************************************************
*	Function name:	MX_SPI4_Init
*	Function:	Initialize SPI configuration
*	Note:Use hardware chip select	 
****************************************************************************************************************************************/


//void MX_SPI4_Init(void)
//{
//	LCD_SPI.Instance 									= SPI4;							   					//	Use SPI4
//	LCD_SPI.Init.Mode 								= SPI_MODE_MASTER;            					//	Master mode
//	LCD_SPI.Init.Direction 							= SPI_DIRECTION_1LINE;       					   //	Single line
//	LCD_SPI.Init.DataSize 							= SPI_DATASIZE_8BIT;          					//	8-bit data width
//	LCD_SPI.Init.CLKPolarity 						= SPI_POLARITY_LOW;           					//	CLK stays low when idle
//	LCD_SPI.Init.CLKPhase 							= SPI_PHASE_1EDGE;            					//	Data valid on the first CLK edge
//	LCD_SPI.Init.NSS 									= SPI_NSS_HARD_OUTPUT;        					//	Use hardware chip select   
//	
//// The SPI kernel clock is set to 120M, divided by 2 to get a 60M SCK clock
//	LCD_SPI.Init.BaudRatePrescaler 				= SPI_BAUDRATEPRESCALER_2;
//	
//	LCD_SPI.Init.FirstBit	 						= SPI_FIRSTBIT_MSB;									//	MSB first
//	LCD_SPI.Init.TIMode 								= SPI_TIMODE_DISABLE;         					//	TI mode disabled
//	LCD_SPI.Init.CRCCalculation					= SPI_CRCCALCULATION_DISABLE; 					//	CRC disabled
//	LCD_SPI.Init.CRCPolynomial 					= 0x0;                        					// CRC polynomial, not used here				
//	LCD_SPI.Init.NSSPMode 							= SPI_NSS_PULSE_ENABLE;      						//	Use chip select pulse mode
//	LCD_SPI.Init.NSSPolarity 						= SPI_NSS_POLARITY_LOW;      						//	Chip select active low
//	LCD_SPI.Init.FifoThreshold 					= SPI_FIFO_THRESHOLD_02DATA;  					//	FIFO threshold
//	LCD_SPI.Init.TxCRCInitializationPattern 	= SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;   // TX CRC initialization pattern, not used here
//	LCD_SPI.Init.RxCRCInitializationPattern 	= SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;   // RX CRC initialization pattern, not used here
//	LCD_SPI.Init.MasterSSIdleness 				= SPI_MASTER_SS_IDLENESS_00CYCLE;            // Extra delay cycles = 0
//	LCD_SPI.Init.MasterInterDataIdleness 		= SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;     // Delay cycles between two data frames in master mode
//	LCD_SPI.Init.MasterReceiverAutoSusp 		= SPI_MASTER_RX_AUTOSUSP_DISABLE;            // Disable automatic receive management
//	LCD_SPI.Init.MasterKeepIOState 				= SPI_MASTER_KEEP_IO_STATE_DISABLE; 	 		//	Master mode: SPI is prevented from holding the current pin state
//	LCD_SPI.Init.IOSwap 								= SPI_IO_SWAP_DISABLE;				            // Do not swap MOSI and MISO
//	
//   HAL_SPI_Init(&LCD_SPI);
//}


/****************************************************************************************************************************************
*	Function name: LCD_WriteCommand
*
*	Input: lcd_command - control command to write
*
*	Function: Writes a command to the LCD controller
*
****************************************************************************************************************************************/

void  LCD_WriteCommand(uint8_t lcd_command)
{
   LCD_DC_Command;     // DC pin outputs low level, indicating this transfer is a command

   HAL_SPI_Transmit(&LCD_SPI, &lcd_command, 1, 1000); // Start SPI transfer
}

/****************************************************************************************************************************************
*	Function name: LCD_WriteData_8bit
*
*	Input: lcd_data - 8-bit data to write
*
*	Function: Write 8-bit data
*	
****************************************************************************************************************************************/

void  LCD_WriteData_8bit(uint8_t lcd_data)
{
   LCD_DC_Data;     // DC pin outputs high level, indicating this transfer is data

   HAL_SPI_Transmit(&LCD_SPI, &lcd_data, 1, 1000) ; // Start SPI transfer
}

/****************************************************************************************************************************************
*	Function name: LCD_WriteData_16bit
*
*	Input: lcd_data - 16-bit data to write
*
*	Function: Write 16-bit data
*	
****************************************************************************************************************************************/

void  LCD_WriteData_16bit(uint16_t lcd_data)
{
   uint8_t lcd_data_buff[2];    // data send buffer
   LCD_DC_Data;      // DC pin outputs high level, indicating this transfer is data
 
   lcd_data_buff[0] = lcd_data>>8;  // Split the data
   lcd_data_buff[1] = lcd_data;
		
	HAL_SPI_Transmit(&LCD_SPI, lcd_data_buff, 2, 1000) ;   // Start SPI transfer
}

/****************************************************************************************************************************************
*	Function name: LCD_WriteBuff
*
*	Input: DataBuff - data buffer, DataSize - data length
*
*	Function: Write data to the screen in bulk
*	
****************************************************************************************************************************************/

void  LCD_WriteBuff(uint16_t *DataBuff, uint16_t DataSize)
{
	LCD_DC_Data;     // DC pin outputs high level, indicating this transfer is data	

// Switch to 16-bit data width for more efficient writes, no splitting needed	
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16-bit data width
   HAL_SPI_Init(&LCD_SPI);		
	
	HAL_SPI_Transmit(&LCD_SPI, (uint8_t *)DataBuff, DataSize, 1000) ; // Start SPI transfer
	
// Switch back to 8-bit data width, because commands and some data are transmitted as 8-bit
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8-bit data width
   HAL_SPI_Init(&LCD_SPI);	
}

/****************************************************************************************************************************************
*	Function name: SPI_LCD_Init
*
*	Function: Initialize SPI and various LCD controller parameters
*	
****************************************************************************************************************************************/

void SPI_LCD_Init(void)
{
//   MX_SPI4_Init();               // Initialize SPI and control pins
   
   HAL_Delay(10);               	// After the screen completes reset (including power-on reset), wait at least 5ms before sending commands

 	LCD_WriteCommand(0x36);       // Memory access control command, sets the framebuffer access mode
	LCD_WriteData_8bit(0x00);     // Configure top-to-bottom, left-to-right, RGB pixel format

	LCD_WriteCommand(0x3A);			// Interface pixel format command, sets whether 12/16/18-bit color is used
	LCD_WriteData_8bit(0x05);     // Configure 16-bit pixel format here

// Most of the following are voltage setting commands; use the manufacturer values directly
 	LCD_WriteCommand(0xB2);			
	LCD_WriteData_8bit(0x0C);
	LCD_WriteData_8bit(0x0C); 
	LCD_WriteData_8bit(0x00); 
	LCD_WriteData_8bit(0x33); 
	LCD_WriteData_8bit(0x33); 			

	LCD_WriteCommand(0xB7);		   // Gate voltage setting command	
	LCD_WriteData_8bit(0x35);     // VGH = 13.26V, VGL = -10.43V

	LCD_WriteCommand(0xBB);			// Common voltage setting command
	LCD_WriteData_8bit(0x19);     // VCOM = 1.35V

	LCD_WriteCommand(0xC0);
	LCD_WriteData_8bit(0x2C);

	LCD_WriteCommand(0xC2);       // VDV and VRH source setting
	LCD_WriteData_8bit(0x01);     // VDV and VRH are freely configured by the user

	LCD_WriteCommand(0xC3);			// VRH voltage setting command  
	LCD_WriteData_8bit(0x12);     // VRH voltage = 4.6+(vcom+vcom offset+vdv)
				
	LCD_WriteCommand(0xC4);		   // VDV voltage setting command	
	LCD_WriteData_8bit(0x20);     // VDV voltage = 0v

	LCD_WriteCommand(0xC6); 		// Normal mode frame rate control command
	LCD_WriteData_8bit(0x0F);   	// Set the LCD controller refresh rate to 60 fps    

	LCD_WriteCommand(0xD0);			// Power control command
	LCD_WriteData_8bit(0xA4);     // Invalid data, fixed value 0xA4
	LCD_WriteData_8bit(0xA1);     // AVDD = 6.8V, AVDD = -4.8V, VDS = 2.3V

	LCD_WriteCommand(0xE0);       // Positive polarity gamma setting
	LCD_WriteData_8bit(0xD0);
	LCD_WriteData_8bit(0x04);
	LCD_WriteData_8bit(0x0D);
	LCD_WriteData_8bit(0x11);
	LCD_WriteData_8bit(0x13);
	LCD_WriteData_8bit(0x2B);
	LCD_WriteData_8bit(0x3F);
	LCD_WriteData_8bit(0x54);
	LCD_WriteData_8bit(0x4C);
	LCD_WriteData_8bit(0x18);
	LCD_WriteData_8bit(0x0D);
	LCD_WriteData_8bit(0x0B);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x23);

	LCD_WriteCommand(0xE1);      // Negative polarity gamma setting
	LCD_WriteData_8bit(0xD0);
	LCD_WriteData_8bit(0x04);
	LCD_WriteData_8bit(0x0C);
	LCD_WriteData_8bit(0x11);
	LCD_WriteData_8bit(0x13);
	LCD_WriteData_8bit(0x2C);
	LCD_WriteData_8bit(0x3F);
	LCD_WriteData_8bit(0x44);
	LCD_WriteData_8bit(0x51);
	LCD_WriteData_8bit(0x2F);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x1F);
	LCD_WriteData_8bit(0x20);
	LCD_WriteData_8bit(0x23);

	LCD_WriteCommand(0x21);       // Enable display inversion; the panel is normally black, so operations are reversed

 // Exit sleep command. The LCD controller enters sleep mode after power-on/reset, so exit sleep before operating the screen  
	LCD_WriteCommand(0x11);       // Exit sleep command
   HAL_Delay(120);               // Wait 120ms for the power voltage and clock circuits to stabilize

 // Turn-on display command. The LCD controller automatically turns off the display after power-on/reset 
	LCD_WriteCommand(0x29);       // Turn on display   	

// The following sets some driver defaults
   LCD_SetDirection(Direction_V);  	      //	Set display direction
	LCD_SetBackColor(LCD_BLACK);           // Set background color
 	LCD_SetColor(LCD_WHITE);               // Set pen color  
	LCD_Clear();                           // Clear screen

   LCD_SetAsciiFont(&ASCII_Font24);       // Set default font
   LCD_ShowNumMode(Fill_Zero);	      	// Set the variable display mode: pad extra digits with spaces or 0

	lcd_bl_bright_set(11000);
}

/****************************************************************************************************************************************
*	Function name:	 LCD_SetAddress
*
*	Input:	 x1 - start X coordinate   y1 - start Y coordinate  
*              x2 - end X coordinate   y2 - end Y coordinate	   
*	
*	Function:   Set the display coordinate region		 			 
*****************************************************************************************************************************************/

void LCD_SetAddress(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)		
{
	LCD_WriteCommand(0x2a);			//	Column address setting, i.e. X coordinate
	LCD_WriteData_16bit(x1+LCD.X_Offset);
	LCD_WriteData_16bit(x2+LCD.X_Offset);

	LCD_WriteCommand(0x2b);			//	Row address setting, i.e. Y coordinate
	LCD_WriteData_16bit(y1+LCD.Y_Offset);
	LCD_WriteData_16bit(y2+LCD.Y_Offset);

	LCD_WriteCommand(0x2c);			//	Start writing to the framebuffer, i.e. the color data to display
}

/****************************************************************************************************************************************
*	Function name:	LCD_SetColor
*
*	Input:	Color - color to display, e.g. 0x0000FF is blue
*
*	Function:	This function sets the pen color, e.g. for characters, points, lines, and drawing
*
*	Note:	1. For convenient custom colors, the Color parameter uses 24-bit RGB888 format; no need to worry about color conversion
*					2. In the 24-bit value, high-to-low bits correspond to the R, G, B color channels
*
*****************************************************************************************************************************************/

void LCD_SetColor(uint32_t Color)
{
	uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; //value of each color channel

	Red_Value   = (uint16_t)((Color&0x00F80000)>>8);   // Convert to 16-bit RGB565
	Green_Value = (uint16_t)((Color&0x0000FC00)>>5);
	Blue_Value  = (uint16_t)((Color&0x000000F8)>>3);

	LCD.Color = (uint16_t)(Red_Value | Green_Value | Blue_Value);  // Write the color to the global LCD parameters		
}

/****************************************************************************************************************************************
*	Function name:	LCD_SetBackColor
*
*	Input:	Color - color to display, e.g. 0x0000FF is blue
*
*	Function:	Set the background color; used for clearing the screen and for character backgrounds
*
*	Note:	1. For convenient custom colors, the Color parameter uses 24-bit RGB888 format; no need to worry about color conversion
*					2. In the 24-bit value, high-to-low bits correspond to the R, G, B color channels
*
*****************************************************************************************************************************************/

void LCD_SetBackColor(uint32_t Color)
{
	uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; //value of each color channel

	Red_Value   = (uint16_t)((Color&0x00F80000)>>8);   // Convert to 16-bit RGB565
	Green_Value = (uint16_t)((Color&0x0000FC00)>>5);
	Blue_Value  = (uint16_t)((Color&0x000000F8)>>3);

	LCD.BackColor = (uint16_t)(Red_Value | Green_Value | Blue_Value);	// Write the color to the global LCD parameters			   	
}

/****************************************************************************************************************************************
*	Function name:	LCD_SetDirection
*
*	Input:	direction - display direction
*
*	Function:	Set the display direction
*
*	Note:   1. Acceptable parameters: Direction_H, Direction_V, Direction_H_Flip, Direction_V_Flip        
*              2. Example: LCD_DisplayDirection(Direction_H) sets landscape mode
*
*****************************************************************************************************************************************/

void LCD_SetDirection(uint8_t direction)
{
	LCD.Direction = direction;    // Write to the global LCD parameters

   if( direction == Direction_H )   // Landscape mode
   {
      LCD_WriteCommand(0x36);    		// Memory access control command, sets the framebuffer access mode
      LCD_WriteData_8bit(0x70);        // Landscape mode
      LCD.X_Offset   = 0;             // Set the controller coordinate offset
      LCD.Y_Offset   = 0;   
      LCD.Width      = LCD_Height;		// Reassign width and height
      LCD.Height     = LCD_Width;		
   }
   else if( direction == Direction_V )
   {
      LCD_WriteCommand(0x36);    		// Memory access control command, sets the framebuffer access mode
      LCD_WriteData_8bit(0x00);        // Portrait mode
      LCD.X_Offset   = 0;              // Set the controller coordinate offset
      LCD.Y_Offset   = 0;     
      LCD.Width      = LCD_Width;		// Reassign width and height
      LCD.Height     = LCD_Height;						
   }
   else if( direction == Direction_H_Flip )
   {
      LCD_WriteCommand(0x36);   			 // Memory access control command, sets the framebuffer access mode
      LCD_WriteData_8bit(0xA0);         // Landscape, flipped vertically, RGB pixel format
      LCD.X_Offset   = 0;              // Set the controller coordinate offset
      LCD.Y_Offset   = 0;      
      LCD.Width      = LCD_Height;		 // Reassign width and height
      LCD.Height     = LCD_Width;				
   }
   else if( direction == Direction_V_Flip )
   {
      LCD_WriteCommand(0x36);    		// Memory access control command, sets the framebuffer access mode
      LCD_WriteData_8bit(0xC0);        // Portrait, flipped vertically, RGB pixel format
      LCD.X_Offset   = 0;              // Set the controller coordinate offset
      LCD.Y_Offset   = 0;     
      LCD.Width      = LCD_Width;		// Reassign width and height
      LCD.Height     = LCD_Height;				
   }   
}

/****************************************************************************************************************************************
*	Function name:	LCD_SetAsciiFont
*
*	Input:	*fonts - ASCII font to set
*
*	Function:	Set the ASCII font; available sizes are 3216/2412/2010/1608/1206
*
*	Note:	1. Example: LCD_SetAsciiFont(&ASCII_Font24) sets the 2412 ASCII font
*					2. The font bitmaps are stored in lcd_fonts.c 			
*
*****************************************************************************************************************************************/

void LCD_SetAsciiFont(pFONT *Asciifonts)
{
  LCD_AsciiFonts = Asciifonts;
}

/****************************************************************************************************************************************
*	Function name:	LCD_Clear
*
*	Function:	Clear the LCD to the LCD.BackColor color
*
*	Note:	Call LCD_SetBackColor() first to set the clear color, then call this function to clear the screen
*
*****************************************************************************************************************************************/

void LCD_Clear(void)
{
   LCD_SetAddress(0,0,LCD.Width-1,LCD.Height-1);	// Set coordinates
	
	LCD_DC_Data;     // DC pin outputs high level, indicating this transfer is data	

// Switch to 16-bit data width for more efficient writes, no splitting needed	
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16-bit data width
   HAL_SPI_Init(&LCD_SPI);		
	
   LCD_SPI_Transmit(&LCD_SPI, LCD.BackColor, LCD.Width * LCD.Height) ;   // Start transfer

// Switch back to 8-bit data width, because commands and some data are transmitted as 8-bit
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8-bit data width
   HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
*	Function name:	LCD_ClearRect
*
*	Input:	x - start X coordinate
*					y - start Y coordinate
*					width  - width of the region to clear
*					height - height of the region to clear
*
*	Function:	Clear the region at the given position to the LCD.BackColor color
*
*	Note:	1. Call LCD_SetBackColor() first to set the clear color, then call this function to clear the screen
*				   2. Example: LCD_ClearRect(10, 10, 100, 50) clears a 100x50 region starting at (10,10)
*
*****************************************************************************************************************************************/

void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
   LCD_SetAddress( x, y, x+width-1, y+height-1);	// Set coordinates	
	
	LCD_DC_Data;     // DC pin outputs high level, indicating this transfer is data	

// Switch to 16-bit data width for more efficient writes, no splitting needed	
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16-bit data width
   HAL_SPI_Init(&LCD_SPI);		
	
   LCD_SPI_Transmit(&LCD_SPI, LCD.BackColor, width*height) ;  // Start transfer

// Switch back to 8-bit data width, because commands and some data are transmitted as 8-bit
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8-bit data width
   HAL_SPI_Init(&LCD_SPI);

}

/****************************************************************************************************************************************
*	Function name:	LCD_DrawPoint
*
*	Input:	x - start X coordinate
*					y - start Y coordinate
*					color  - color to draw, in 24-bit RGB888 format; no need to worry about color conversion
*
*	Function:	Draw a point of the given color at the given coordinates
*
*	Note:	Example: LCD_DrawPoint(10, 10, 0x0000FF) draws a blue point at (10,10)
*
*****************************************************************************************************************************************/

void LCD_DrawPoint(uint16_t x,uint16_t y,uint32_t color)
{
	LCD_SetAddress(x,y,x,y);	//	Set coordinates 

	LCD_WriteData_16bit(color)	;
}

/****************************************************************************************************************************************
*	Function name:	LCD_DisplayChar
*
*	Input:	x - start X coordinate
*					y - start Y coordinate
*					c  - ASCII character
*
*	Function:	Display the given character at the given coordinates
*
*	Note:	1. Set the font, e.g. LCD_SetAsciiFont(&ASCII_Font24) for the 2412 ASCII font
*					2.	Set the color, e.g. LCD_SetColor(0xFF0000FF) for blue
*					3. Set the background color, e.g. LCD_SetBackColor(0x000000) for black
*					4. Example: LCD_DisplayChar(10, 10, 'a') displays 'a' at (10,10)
*
*****************************************************************************************************************************************/

void LCD_DisplayChar(uint16_t x, uint16_t y,uint8_t c)
{
	uint16_t  index = 0, counter = 0 ,i = 0, w = 0;		// counter variables
   uint8_t   disChar;		//stores the character font byte

	if ( LCD.Transparent )
	{
		uint16_t row = 0, col = 0;
		uint8_t  bytesPerRow = LCD_AsciiFonts->Sizes / LCD_AsciiFonts->Height;
		c = c - 32;
		for(row = 0; row < LCD_AsciiFonts->Height; row++)
		{
			for(col = 0; col < LCD_AsciiFonts->Width; col++)
			{
				disChar = LCD_AsciiFonts->pTable[c*LCD_AsciiFonts->Sizes + row*bytesPerRow + col/8];
				if( disChar & (0x01 << (col%8)) )
					LCD_DrawPoint(x+col, y+row, LCD.Color);
			}
		}
		return;
	}

	c = c - 32; 	// Calculate the ASCII character offset

	for(index = 0; index < LCD_AsciiFonts->Sizes; index++)	
	{
		disChar = LCD_AsciiFonts->pTable[c*LCD_AsciiFonts->Sizes + index]; //Get the character bitmap value
		for(counter = 0; counter < 8; counter++)
		{ 
			if(disChar & 0x01)	
			{		
            LCD_Buff[i] =  LCD.Color;			// If the current bit is 1, draw with the pen color
			}
			else		
			{		
            LCD_Buff[i] = LCD.BackColor;		//Otherwise draw with the background color
			}
			disChar >>= 1;
			i++;
         w++;
 			if( w == LCD_AsciiFonts->Width ) // If the written data reaches the character width, exit the current loop
			{								   // proceed to draw the next character
				w = 0;
				break;
			}        
		}	
	}		
   LCD_SetAddress( x, y, x+LCD_AsciiFonts->Width-1, y+LCD_AsciiFonts->Height-1);	   // Set coordinates	
   LCD_WriteBuff(LCD_Buff,LCD_AsciiFonts->Width*LCD_AsciiFonts->Height);          // Write to the framebuffer
}

/****************************************************************************************************************************************
*	Function name:	LCD_DisplayString
*
*	Input:	x - start X coordinate
*					y - start Y coordinate
*					p - start address of the ASCII string
*
*	Function:	Display the given string at the given coordinates
*
*	Note:	1. Set the font, e.g. LCD_SetAsciiFont(&ASCII_Font24) for the 2412 ASCII font
*					2.	Set the color, e.g. LCD_SetColor(0x0000FF) for blue
*					3. Set the background color, e.g. LCD_SetBackColor(0x000000) for black
*					4. Example: LCD_DisplayString(10, 10, "KE") displays "KE" at start coordinate (10,10)
*
*****************************************************************************************************************************************/

void LCD_DisplayString( uint16_t x, uint16_t y, char *p) 
{  
	while ((x < LCD.Width) && (*p != 0))	//Check whether the display coordinate exceeds the display area and whether the character is null
	{
		 LCD_DisplayChar( x,y,*p);
		 x += LCD_AsciiFonts->Width; //Display the next character
		 p++;	//Get the next character address
	}
}

/****************************************************************************************************************************************
*	Function name:	LCD_SetTextFont
*
*	Input:	*fonts - text font to set
*
*	Function:	Set the text font, including Chinese and ASCII characters.
*
*	Note:	1. Available Chinese fonts: 3232/2424/2020/1616/1212,
*						with corresponding ASCII fonts 3216/2412/2010/1608/1206
*					2. The font bitmaps are stored in lcd_fonts.c 
*					3. The Chinese library uses a small font set: bitmaps are generated only for the characters actually used
*					4. Example: LCD_SetTextFont(&CH_Font24) sets the 2424 Chinese font and 2412 ASCII font
*
*****************************************************************************************************************************************/

void LCD_SetTextFont(pFONT *fonts)
{
	LCD_CHFonts = fonts;		// Set the Chinese font
	switch(fonts->Width )
	{
		case 12:	LCD_AsciiFonts = &ASCII_Font12;	break;	// Set the ASCII font to 1206
		case 16:	LCD_AsciiFonts = &ASCII_Font16;	break;	// Set the ASCII font to 1608
		case 20:	LCD_AsciiFonts = &ASCII_Font20;	break;	// Set the ASCII font to 2010	
		case 24:	LCD_AsciiFonts = &ASCII_Font24;	break;	// Set the ASCII font to 2412
		case 32:	LCD_AsciiFonts = &ASCII_Font32;	break;	// Set the ASCII font to 3216		
		default: break;
	}
}
/******************************************************************************************************************************************
*	Function name:	LCD_DisplayChinese
*
*	Input:	x - start X coordinate
*					y - start Y coordinate
*					pText - Chinese character
*
*	Function:	Display the given single Chinese character at the given coordinates
*
*	Note:	1. Set the font, e.g. LCD_SetTextFont(&CH_Font24) for the 2424 Chinese font and 2412 ASCII font
*					2.	Set the color, e.g. LCD_SetColor(0xFF0000FF) for blue
*					3. Set the background color, e.g. LCD_SetBackColor(0xFF000000) for black
*					4. Example: LCD_DisplayChinese(10, 10, "A") displays a Chinese character at (10,10)
*
*****************************************************************************************************************************************/

void LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText) 
{
	uint16_t  i=0,index = 0, counter = 0;	// counter variables
	uint16_t  addr;	// bitmap address
   uint8_t   disChar;	//bitmap value
	uint16_t  Xaddress = 0; //X coordinate

	while(1)
	{		
		// Compare the Chinese character encoding in the array to locate its bitmap address		
		if ( *(LCD_CHFonts->pTable + (i+1)*LCD_CHFonts->Sizes + 0)==*pText && *(LCD_CHFonts->pTable + (i+1)*LCD_CHFonts->Sizes + 1)==*(pText+1) )	
		{   
			addr=i;	// bitmap address offset
			break;
		}				
		i+=2;	// Each Chinese character encoding is 2 bytes

		if(i >= LCD_CHFonts->Table_Rows)	break;	// No matching character in the bitmap list	
	}	
	i=0;
	for(index = 0; index <LCD_CHFonts->Sizes; index++)
	{	
		disChar = *(LCD_CHFonts->pTable + (addr)*LCD_CHFonts->Sizes + index);	// Get the corresponding bitmap address
		
		for(counter = 0; counter < 8; counter++)
		{ 
			if(disChar & 0x01)	
			{		
            LCD_Buff[i] =  LCD.Color;			// If the current bit is 1, draw with the pen color
			}
			else		
			{		
            LCD_Buff[i] = LCD.BackColor;		// Otherwise draw with the background color
			}
         i++;
			disChar >>= 1;
			Xaddress++;  //X coordinate increments
			
			if( Xaddress == LCD_CHFonts->Width ) 	//	If the X coordinate reaches the character width, exit the current loop
			{														//	proceed to the next row drawing
				Xaddress = 0;
				break;
			}
		}	
	}	
   LCD_SetAddress( x, y, x+LCD_CHFonts->Width-1, y+LCD_CHFonts->Height-1);	   // Set coordinates	
   LCD_WriteBuff(LCD_Buff,LCD_CHFonts->Width*LCD_CHFonts->Height);            // Write to the framebuffer
}

/*****************************************************************************************************************************************
*	Function name:	LCD_DisplayText
*
*	Input:	x - start X coordinate
*					y - start Y coordinate
*					pText - string; can display Chinese or ASCII characters
*
*	Function:	Display the given string at the given coordinates
*
*	Note:	1. Set the font, e.g. LCD_SetTextFont(&CH_Font24) for the 2424 Chinese font and 2412 ASCII font
*					2.	Set the color, e.g. LCD_SetColor(0xFF0000FF) for blue
*					3. Set the background color, e.g. LCD_SetBackColor(0xFF000000) for black
*					4. Example: LCD_DisplayChinese(10, 10, "ABCSTM32") displays the string "ABCSTM32" at (10,10)
*
**********************************************************************************************************************************ke*******/

void LCD_DisplayText(uint16_t x, uint16_t y, char *pText) 
{  
 	
	while(*pText != 0)	// Check whether it is a null character
	{
		if(*pText<=0x7F)	// Check whether it is ASCII
		{
			LCD_DisplayChar(x,y,*pText);	// Display ASCII
			x+=LCD_AsciiFonts->Width;				// Advance the X coordinate to the next character
			pText++;								// string address + 1
		}
		else					// If the character is Chinese
		{			
			LCD_DisplayChinese(x,y,pText);	// Display Chinese character
			x+=LCD_CHFonts->Width;				// Advance the X coordinate to the next character
			pText+=2;								// string address + 2, Chinese characters are 2 bytes
		}
	}	
}

/*****************************************************************************************************************************************
*	Function name:	LCD_ShowNumMode
*
*	Input:	mode - variable display mode
*
*	Function:	Set whether extra digits of a variable are padded with 0 or spaces. Accepts Fill_Space to pad spaces and Fill_Zero to pad zeros
*
*	Note:   1. Used only by LCD_DisplayNumber() (integers) and LCD_DisplayDecimals() (decimals)
*					2. Example: LCD_ShowNumMode(Fill_Zero) pads extra digits with 0, e.g. 123 becomes 000123
*
*****************************************************************************************************************************************/

void LCD_ShowNumMode(uint8_t mode)
{
	LCD.ShowNum_Mode = mode;
}

void LCD_ShowTransparent(uint8_t mode)
{
	LCD.Transparent = mode;
}

/*****************************************************************************************************************************************
*	Function name:	LCD_DisplayNumber
*
*	Input:	x - start X coordinate
*					y - start Y coordinate
*					number - number to display, range -2147483648~2147483647
*					len - number of digits; if more than len, output at its actual length; for negative numbers, reserve one digit for the sign
*
*	Function:	Display the given integer variable at the given coordinates
*
*	Note:	1. Set the font, e.g. LCD_SetAsciiFont(&ASCII_Font24) for the ASCII font
*					2.	Set the color, e.g. LCD_SetColor(0x0000FF) for blue
*					3. Set the background color, e.g. LCD_SetBackColor(0x000000) for black
*					4. Example: LCD_DisplayNumber(10, 10, a, 5) displays variable a at (10,10), 5 digits total, extra digits padded with 0 or spaces,
*						e.g. a=123 displays as 123 (with two leading spaces) or 00123 depending on LCD_ShowNumMode()
*						
*****************************************************************************************************************************************/

void  LCD_DisplayNumber( uint16_t x, uint16_t y, int32_t number, uint8_t len) 
{  
	char   Number_Buffer[15];				// Stores the converted string

	if( LCD.ShowNum_Mode == Fill_Zero)	// Pad extra digits with 0
	{
		sprintf( Number_Buffer , "%0.*d",len, number );	// Convert number to a string for display		
	}
	else			// Pad extra digits with spaces
	{	
		sprintf( Number_Buffer , "%*d",len, number );	// Convert number to a string for display		
	}
	
	LCD_DisplayString( x, y,(char *)Number_Buffer) ;  // Display the converted string
	
}

/***************************************************************************************************************************************
*	Function name:	LCD_DisplayDecimals
*
*	Input:	x - start X coordinate
*					y - start Y coordinate
*					decimals - number to display, double type range 1.7 x 10^(-308) ~ 1.7 x 10^(+308), with 15-16 accurate significant digits
*
*       			len - Total digits of the variable (including decimal point and sign); if the actual digits exceed the specified total, output at the actual length,
*							Example 1: decimal -123.123, if len <=8, outputs -123.123 as-is
*							Example 2: decimal -123.123, if len =10, outputs -123.123 (two leading spaces before the sign)
*							Example 3: decimal -123.123, if len =10 and LCD_ShowNumMode() is set to zero-fill mode, outputs -00123.123 
*
*					decs - decimal places to keep; if the actual decimals exceed the specified digits, round to the specified width
*							 Example: 1.12345 with decs =4 outputs 1.1235
*
*	Function:	Display the given variable, including decimals, at the given coordinates
*
*	Note:	1. Set the font, e.g. LCD_SetAsciiFont(&ASCII_Font24) for the ASCII font
*					2.	Set the color, e.g. LCD_SetColor(0x0000FF) for blue
*					3. Set the background color, e.g. LCD_SetBackColor(0x000000) for black
*					4. Example: LCD_DisplayDecimals(10, 10, a, 5, 3) displays variable a at (10,10), 5 digits total with 3 decimals
*						
*****************************************************************************************************************************************/

void  LCD_DisplayDecimals( uint16_t x, uint16_t y, double decimals, uint8_t len, uint8_t decs) 
{  
	char  Number_Buffer[20];				// Stores the converted string
	
	if( LCD.ShowNum_Mode == Fill_Zero)	// Zero-fill mode
	{
		sprintf( Number_Buffer , "%0*.*lf",len,decs, decimals );	// Convert number to a string for display		
	}
	else		// Pad extra digits with spaces
	{
		sprintf( Number_Buffer , "%*.*lf",len,decs, decimals );	// Convert number to a string for display		
	}
	
	LCD_DisplayString( x, y,(char *)Number_Buffer) ;	// Display the converted string
}


/***************************************************************************************************************************************
*	Function name: LCD_DrawLine
*
*	Input: x1 - start X coordinate
*			 	 y1 - start Y coordinate
*
*				 x2 - end X coordinate
*            y2 - end Y coordinate
*
*	Function: Draw a line between two points
*
*	Note: This function is ported from the ST official evaluation board example
*						 
*****************************************************************************************************************************************/

#define ABS(X)  ((X) > 0 ? (X) : -(X))    

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0, 
	yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0, 
	curpixel = 0;

	deltax = ABS(x2 - x1);        /* The difference between the x's */
	deltay = ABS(y2 - y1);        /* The difference between the y's */
	x = x1;                       /* Start x off at the first pixel */
	y = y1;                       /* Start y off at the first pixel */

	if (x2 >= x1)                 /* The x-values are increasing */
	{
	 xinc1 = 1;
	 xinc2 = 1;
	}
	else                          /* The x-values are decreasing */
	{
	 xinc1 = -1;
	 xinc2 = -1;
	}

	if (y2 >= y1)                 /* The y-values are increasing */
	{
	 yinc1 = 1;
	 yinc2 = 1;
	}
	else                          /* The y-values are decreasing */
	{
	 yinc1 = -1;
	 yinc2 = -1;
	}

	if (deltax >= deltay)         /* There is at least one x-value for every y-value */
	{
	 xinc1 = 0;                  /* Don't change the x when numerator >= denominator */
	 yinc2 = 0;                  /* Don't change the y for every iteration */
	 den = deltax;
	 num = deltax / 2;
	 numadd = deltay;
	 numpixels = deltax;         /* There are more x-values than y-values */
	}
	else                          /* There is at least one y-value for every x-value */
	{
	 xinc2 = 0;                  /* Don't change the x for every iteration */
	 yinc1 = 0;                  /* Don't change the y when numerator >= denominator */
	 den = deltay;
	 num = deltay / 2;
	 numadd = deltax;
	 numpixels = deltay;         /* There are more y-values than x-values */
	}
	for (curpixel = 0; curpixel <= numpixels; curpixel++)
	{
	 LCD_DrawPoint(x,y,LCD.Color);             /* Draw the current pixel */
	 num += numadd;              /* Increase the numerator by the top of the fraction */
	 if (num >= den)             /* Check if numerator >= denominator */
	 {
		num -= den;               /* Calculate the new numerator value */
		x += xinc1;               /* Change the x as appropriate */
		y += yinc1;               /* Change the y as appropriate */
	 }
	 x += xinc2;                 /* Change the x as appropriate */
	 y += yinc2;                 /* Change the y as appropriate */
	}  
}

/***************************************************************************************************************************************
*	Function name: LCD_DrawLine_V
*
*	Input: x - X coordinate
*			 	 y - Y coordinate
*				 height - vertical length
*
*	Function: Draw a vertical line of the given length at the given position
*
*	Note: 1. This function is ported from the ST official evaluation board example
*				 2. The region to draw must not exceed the screen display area		
*            3. For vertical lines only, prefer this function; it is much faster than LCD_DrawLine
*  Performance test:
*****************************************************************************************************************************************/

void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height)
{
   uint16_t i ; // counter variables

	for (i = 0; i < height; i++)
	{
       LCD_Buff[i] =  LCD.Color;  // Write to the buffer
   }   
   LCD_SetAddress( x, y, x, y+height-1);	     // Set coordinates	

   LCD_WriteBuff(LCD_Buff,height);          // Write to the framebuffer
}

/***************************************************************************************************************************************
*	Function name: LCD_DrawLine_H
*
*	Input: x - X coordinate
*			 	 y - Y coordinate
*				 width  - horizontal length
*
*	Function: Draw a horizontal line of the given length at the given position
*
*	Note: 1. This function is ported from the ST official evaluation board example
*				 2. The region to draw must not exceed the screen display area		
*            3. For horizontal lines only, prefer this function; it is much faster than LCD_DrawLine
*  Performance test:
**********************************************************************************************************************************ke*******/

void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width)
{
   uint16_t i ; // counter variables

	for (i = 0; i < width; i++)
	{
       LCD_Buff[i] =  LCD.Color;  // Write to the buffer
   }   
   LCD_SetAddress( x, y, x+width-1, y);	     // Set coordinates	

   LCD_WriteBuff(LCD_Buff,width);          // Write to the framebuffer
}
/***************************************************************************************************************************************
*	Function name: LCD_DrawRect
*
*	Input: x - X coordinate
*			 	 y - Y coordinate
*			 	 width  - horizontal length
*				 height - vertical length
*
*	Function: Draw a rectangle outline of the given size at the given position
*
*	Note: 1. This function is ported from the ST official evaluation board example
*				 2. The region to draw must not exceed the screen display area
*						 
*****************************************************************************************************************************************/

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
   // Draw horizontal lines
   LCD_DrawLine_H( x,  y,  width);           
   LCD_DrawLine_H( x,  y+height-1,  width);

   // Draw vertical lines
   LCD_DrawLine_V( x,  y,  height);
   LCD_DrawLine_V( x+width-1,  y,  height);
}


/***************************************************************************************************************************************
*	Function name: LCD_DrawCircle
*
*	Input: x - center X coordinate
*			 	 y - center Y coordinate
*			 	 r  - radius
*
*	Function: Draw a circle outline of radius r at (x,y)
*
*	Note: 1. This function is ported from the ST official evaluation board example
*				 2. The region to draw must not exceed the screen display area
*
*****************************************************************************************************************************************/

void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r)
{
	int Xadd = -r, Yadd = 0, err = 2-2*r, e2;
	do {   

		LCD_DrawPoint(x-Xadd,y+Yadd,LCD.Color);
		LCD_DrawPoint(x+Xadd,y+Yadd,LCD.Color);
		LCD_DrawPoint(x+Xadd,y-Yadd,LCD.Color);
		LCD_DrawPoint(x-Xadd,y-Yadd,LCD.Color);
		
		e2 = err;
		if (e2 <= Yadd) {
			err += ++Yadd*2+1;
			if (-Xadd == Yadd && e2 <= Xadd) e2 = 0;
		}
		if (e2 > Xadd) err += ++Xadd*2+1;
    }
    while (Xadd <= 0);   
}


/***************************************************************************************************************************************
*	Function name: LCD_DrawEllipse
*
*	Input: x - center X coordinate
*			 	 y - center Y coordinate
*			 	 r1  - horizontal semi-axis length
*				 r2  - vertical semi-axis length
*
*	Function: Draw an ellipse outline at (x,y) with horizontal semi-axis r1 and vertical semi-axis r2
*
*	Note: 1. This function is ported from the ST official evaluation board example
*				 2. The region to draw must not exceed the screen display area
*
*****************************************************************************************************************************************/

void LCD_DrawEllipse(int x, int y, int r1, int r2)
{
  int Xadd = -r1, Yadd = 0, err = 2-2*r1, e2;
  float K = 0, rad1 = 0, rad2 = 0;
   
  rad1 = r1;
  rad2 = r2;
  
  if (r1 > r2)
  { 
    do {
      K = (float)(rad1/rad2);
		 
		LCD_DrawPoint(x-Xadd,y+(uint16_t)(Yadd/K),LCD.Color);
		LCD_DrawPoint(x+Xadd,y+(uint16_t)(Yadd/K),LCD.Color);
		LCD_DrawPoint(x+Xadd,y-(uint16_t)(Yadd/K),LCD.Color);
		LCD_DrawPoint(x-Xadd,y-(uint16_t)(Yadd/K),LCD.Color);     
		 
      e2 = err;
      if (e2 <= Yadd) {
        err += ++Yadd*2+1;
        if (-Xadd == Yadd && e2 <= Xadd) e2 = 0;
      }
      if (e2 > Xadd) err += ++Xadd*2+1;
    }
    while (Xadd <= 0);
  }
  else
  {
    Yadd = -r2; 
    Xadd = 0;
    do { 
      K = (float)(rad2/rad1);

		LCD_DrawPoint(x-(uint16_t)(Xadd/K),y+Yadd,LCD.Color);
		LCD_DrawPoint(x+(uint16_t)(Xadd/K),y+Yadd,LCD.Color);
		LCD_DrawPoint(x+(uint16_t)(Xadd/K),y-Yadd,LCD.Color);
		LCD_DrawPoint(x-(uint16_t)(Xadd/K),y-Yadd,LCD.Color);  
		 
      e2 = err;
      if (e2 <= Xadd) {
        err += ++Xadd*3+1;
        if (-Yadd == Xadd && e2 <= Yadd) e2 = 0;
      }
      if (e2 > Yadd) err += ++Yadd*3+1;     
    }
    while (Yadd <= 0);
  }
}

/***************************************************************************************************************************************
*	Function name: LCD_FillCircle
*
*	Input: x - center X coordinate
*			 	 y - center Y coordinate
*			 	 r  - radius
*
*	Function: Fill a circular region of radius r at (x,y)
*
*	Note: 1. This function is ported from the ST official evaluation board example
*				 2. The region to draw must not exceed the screen display area
*
*****************************************************************************************************************************************/

void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r)
{
  int32_t  D;    /* Decision Variable */ 
  uint32_t  CurX;/* Current X Value */
  uint32_t  CurY;/* Current Y Value */ 
  
  D = 3 - (r << 1);
  
  CurX = 0;
  CurY = r;
  
  while (CurX <= CurY)
  {
    if(CurY > 0) 
    { 
      LCD_DrawLine_V(x - CurX, y - CurY,2*CurY);
      LCD_DrawLine_V(x + CurX, y - CurY,2*CurY);
    }
    
    if(CurX > 0) 
    {
		// LCD_DrawLine(x - CurY, y - CurX,x - CurY,y - CurX + 2*CurX);
		// LCD_DrawLine(x + CurY, y - CurX,x + CurY,y - CurX + 2*CurX); 	

      LCD_DrawLine_V(x - CurY, y - CurX,2*CurX);
      LCD_DrawLine_V(x + CurY, y - CurX,2*CurX);
    }
    if (D < 0)
    { 
      D += (CurX << 2) + 6;
    }
    else
    {
      D += ((CurX - CurY) << 2) + 10;
      CurY--;
    }
    CurX++;
  }
  LCD_DrawCircle(x, y, r);  
}

/***************************************************************************************************************************************
*	Function name: LCD_FillRect
*
*	Input: x - X coordinate
*			 	 y - Y coordinate
*			 	 width  - horizontal length
*				 height -vertical length
*
*	Function: Fill a solid rectangle of the given size at (x,y)
*
*	Note: The region to draw must not exceed the screen display area
*						 
*****************************************************************************************************************************************/

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
   LCD_SetAddress( x, y, x+width-1, y+height-1);	// Set coordinates	
	
	LCD_DC_Data;     // DC pin outputs high level, indicating this transfer is data	

// Switch to 16-bit data width for more efficient writes, no splitting needed	
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16-bit data width
   HAL_SPI_Init(&LCD_SPI);		
	
   LCD_SPI_Transmit(&LCD_SPI, LCD.Color, width*height) ;

// Switch back to 8-bit data width, because commands and some data are transmitted as 8-bit
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8-bit data width
   HAL_SPI_Init(&LCD_SPI);
}


/***************************************************************************************************************************************
*	Function name: LCD_DrawImage
*
*	Input: x - start X coordinate
*				 y - start Y coordinate
*			 	 width  - image width in pixels
*				 height - image height in pixels
*				*pImage - start address of the image data buffer
*
*	Function: Display the image at the given coordinates
*
*	Note: 1.The image must be pre-converted to bitmaps; know its length and width in advance
*            2. Use LCD_SetColor() to set the pen color and LCD_SetBackColor() to set the background color
*						 
*****************************************************************************************************************************************/

void 	LCD_DrawImage(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint8_t *pImage) 
{  
   uint8_t   disChar;	         // bitmap value
	uint16_t  Xaddress = x;       // X coordinate
 	uint16_t  Yaddress = y;       // Y coordinate  
	uint16_t  i=0,j=0,m=0;        // counter variables
	uint16_t  BuffCount = 0;      // buffer counter
   uint16_t  Buff_Height = 0;    // number of rows in the buffer

// Because the buffer is limited in size, write in multiple passes
   Buff_Height = (sizeof(LCD_Buff)/2) / height;    // Calculate how many rows of the image the buffer can hold

	for(i = 0; i <height; i++)             // Loop writing row by row
	{
		for(j = 0; j <(float)width/8; j++)  
		{
			disChar = *pImage;

			for(m = 0; m < 8; m++)
			{ 
				if(disChar & 0x01)	
				{		
               LCD_Buff[BuffCount] =  LCD.Color;			// If the current bit is 1, draw with the pen color
				}
				else		
				{		
				   LCD_Buff[BuffCount] = LCD.BackColor;		//Otherwise draw with the background color
				}
				disChar >>= 1;     // Shift the bitmap value
				Xaddress++;        // X coordinate increments
				BuffCount++;       // buffer counter       
				if( (Xaddress - x)==width ) // If the X coordinate reaches the character width, exit the current loop and start the next row		
				{											 
					Xaddress = x;				                 
					break;
				}
			}	
			pImage++;			
		}
      if( BuffCount == Buff_Height*width  )  // When the maximum rows the buffer can hold is reached
      {
         BuffCount = 0; // Clear the buffer counter

         LCD_SetAddress( x, Yaddress , x+width-1, Yaddress+Buff_Height-1);	// Set coordinates	
         LCD_WriteBuff(LCD_Buff,width*Buff_Height);          // Write to the framebuffer     

         Yaddress = Yaddress+Buff_Height;    // Calculate the row offset and write the next part of data
      }     
      if( (i+1)== height ) // When the last row is reached
      {
         LCD_SetAddress( x, Yaddress , x+width-1,i+y);	   // Set coordinates	
         LCD_WriteBuff(LCD_Buff,width*(i+1+y-Yaddress));    // Write to the framebuffer     
      }
	}	
}


/***************************************************************************************************************************************
*	Function name: LCD_CopyBuffer
*
*	Input: x - start X coordinate
*				 y - start Y coordinate
*			 	 width  - horizontal length of the target region
*				 height - vertical length of the target region
*				*pImage - start address of the data buffer
*
*	Function: At the given coordinates, copy data directly to the screen framebuffer
*
*	Note: Bulk copy function, can be used for porting LVGL or displaying images captured by a camera
*						 
*****************************************************************************************************************************************/

void	LCD_CopyBuffer(uint16_t x, uint16_t y,uint16_t width,uint16_t height,uint16_t *DataBuff)
{
	
	LCD_SetAddress(x,y,x+width-1,y+height-1);

	LCD_DC_Data;     // DC pin outputs high level, indicating this transfer is data	

// Switch to 16-bit data width for more efficient writes, no splitting needed	
   LCD_SPI.Init.DataSize 	= SPI_DATASIZE_16BIT;   //	16-bit data width
   HAL_SPI_Init(&LCD_SPI);		
	
	LCD_SPI_TransmitBuffer(&LCD_SPI, DataBuff,width * height) ;
	
//	HAL_SPI_Transmit(&hspi5, (uint8_t *)DataBuff, (x2-x1+1) * (y2-y1+1), 1000) ;
	
// Switch back to 8-bit data width, because commands and some data are transmitted as 8-bit
	LCD_SPI.Init.DataSize 	= SPI_DATASIZE_8BIT;    //	8-bit data width
   HAL_SPI_Init(&LCD_SPI);		
	
}

/**********************************************************************************************************************************
*
* The following functions are modified from the HAL library functions to allow SPI transfers of unlimited data length and to improve screen clearing speed
*
*****************************************************************************************************************KE************/


/**
  * @brief Handle SPI Communication Timeout.
  * @param hspi: pointer to a SPI_HandleTypeDef structure that contains
  *              the configuration information for SPI module.
  * @param Flag: SPI flag to check
  * @param Status: flag state to check
  * @param Timeout: Timeout duration
  * @param Tickstart: Tick start value
  * @retval HAL status
  */
HAL_StatusTypeDef LCD_SPI_WaitOnFlagUntilTimeout(SPI_HandleTypeDef *hspi, uint32_t Flag, FlagStatus Status,
                                                    uint32_t Tickstart, uint32_t Timeout)
{
   /* Wait until flag is set */
   while ((__HAL_SPI_GET_FLAG(hspi, Flag) ? SET : RESET) == Status)
   {
      /* Check for the Timeout */
      if ((((HAL_GetTick() - Tickstart) >=  Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
      {
         return HAL_TIMEOUT;
      }
   }
   return HAL_OK;
}


/**
 * @brief  Close Transfer and clear flags.
 * @param  hspi: pointer to a SPI_HandleTypeDef structure that contains
 *               the configuration information for SPI module.
 * @retval HAL_ERROR: if any error detected
 *         HAL_OK: if nothing detected
 */
 void LCD_SPI_CloseTransfer(SPI_HandleTypeDef *hspi)
{
  uint32_t itflag = hspi->Instance->SR;

  __HAL_SPI_CLEAR_EOTFLAG(hspi);
  __HAL_SPI_CLEAR_TXTFFLAG(hspi);

  /* Disable SPI peripheral */
  __HAL_SPI_DISABLE(hspi);

  /* Disable ITs */
  __HAL_SPI_DISABLE_IT(hspi, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP | SPI_IT_UDR | SPI_IT_OVR | SPI_IT_FRE | SPI_IT_MODF));

  /* Disable Tx DMA Request */
  CLEAR_BIT(hspi->Instance->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);

  /* Report UnderRun error for non RX Only communication */
  if (hspi->State != HAL_SPI_STATE_BUSY_RX)
  {
    if ((itflag & SPI_FLAG_UDR) != 0UL)
    {
      SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_UDR);
      __HAL_SPI_CLEAR_UDRFLAG(hspi);
    }
  }

  /* Report OverRun error for non TX Only communication */
  if (hspi->State != HAL_SPI_STATE_BUSY_TX)
  {
    if ((itflag & SPI_FLAG_OVR) != 0UL)
    {
      SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_OVR);
      __HAL_SPI_CLEAR_OVRFLAG(hspi);
    }
  }

  /* SPI Mode Fault error interrupt occurred -------------------------------*/
  if ((itflag & SPI_FLAG_MODF) != 0UL)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_MODF);
    __HAL_SPI_CLEAR_MODFFLAG(hspi);
  }

  /* SPI Frame error interrupt occurred ------------------------------------*/
  if ((itflag & SPI_FLAG_FRE) != 0UL)
  {
    SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FRE);
    __HAL_SPI_CLEAR_FREFLAG(hspi);
  }

  hspi->TxXferCount = (uint16_t)0UL;
  hspi->RxXferCount = (uint16_t)0UL;
}


/**
  * @brief  Modified specifically for screen clearing, transfers the clear color in bulk
  * @param  hspi   : SPI handle
  * @param  pData  : data to write
  * @param  Size   : data size
  * @retval HAL status
  */

HAL_StatusTypeDef LCD_SPI_Transmit(SPI_HandleTypeDef *hspi,uint16_t pData, uint32_t Size)
{
   uint32_t    tickstart;  
   uint32_t    Timeout = 1000;      // Timeout check
   uint32_t    LCD_pData_32bit;     // data when transmitting in 32-bit
   uint32_t    LCD_TxDataCount;     // transmit counter
   HAL_StatusTypeDef errorcode = HAL_OK;

	/* Check Direction parameter */
	assert_param(IS_SPI_DIRECTION_2LINES_OR_1LINE_2LINES_TXONLY(hspi->Init.Direction));

	/* Process Locked */
	__HAL_LOCK(hspi);

	/* Init tickstart for timeout management*/
	tickstart = HAL_GetTick();

	if (hspi->State != HAL_SPI_STATE_READY)
	{
		errorcode = HAL_BUSY;
		__HAL_UNLOCK(hspi);
		return errorcode;
	}

	if ( Size == 0UL)
	{
		errorcode = HAL_ERROR;
		__HAL_UNLOCK(hspi);
		return errorcode;
	}

	/* Set the transaction information */
	hspi->State       = HAL_SPI_STATE_BUSY_TX;
	hspi->ErrorCode   = HAL_SPI_ERROR_NONE;

	LCD_TxDataCount   = Size;                // transferred data length
	LCD_pData_32bit   = (pData<<16)|pData ;  // When transmitting in 32-bit, combine the colors of 2 pixels  

	/*Init field not used in handle to zero */
	hspi->pRxBuffPtr  = NULL;
	hspi->RxXferSize  = (uint16_t) 0UL;
	hspi->RxXferCount = (uint16_t) 0UL;
	hspi->TxISR       = NULL;
	hspi->RxISR       = NULL;

	/* Configure communication direction : 1Line */
	if (hspi->Init.Direction == SPI_DIRECTION_1LINE)
	{
		SPI_1LINE_TX(hspi);
	}

// Hardware TSIZE control is not used; set to 0 here, so transfer data length is unlimited
	MODIFY_REG(hspi->Instance->CR2, SPI_CR2_TSIZE, 0);

	/* Enable SPI peripheral */
	__HAL_SPI_ENABLE(hspi);

	if (hspi->Init.Mode == SPI_MODE_MASTER)
	{
		 /* Master transfer start */
		 SET_BIT(hspi->Instance->CR1, SPI_CR1_CSTART);
	}

	/* Transmit data in 16 Bit mode */
	while (LCD_TxDataCount > 0UL)
	{
		/* Wait until TXP flag is set to send data */
		if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXP))
		{
			if ((hspi->TxXferCount > 1UL) && (hspi->Init.FifoThreshold > SPI_FIFO_THRESHOLD_01DATA))
			{
				*((__IO uint32_t *)&hspi->Instance->TXDR) = (uint32_t )LCD_pData_32bit;
				LCD_TxDataCount -= (uint16_t)2UL;
			}
			else
			{
				*((__IO uint16_t *)&hspi->Instance->TXDR) =  (uint16_t )pData;
				LCD_TxDataCount--;
			}
		}
		else
		{
			/* Timeout management */
			if ((((HAL_GetTick() - tickstart) >=  Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
			{
				/* Call standard close procedure with error check */
				LCD_SPI_CloseTransfer(hspi);

				/* Process Unlocked */
				__HAL_UNLOCK(hspi);

				SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_TIMEOUT);
				hspi->State = HAL_SPI_STATE_READY;
				return HAL_ERROR;
			}
		}
	}

	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_SR_TXC, RESET, tickstart, Timeout) != HAL_OK)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
	}

	SET_BIT((hspi)->Instance->CR1 , SPI_CR1_CSUSP); // Request SPI transfer suspend
	/* Wait for SPI suspend */
	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_FLAG_SUSP, RESET, tickstart, Timeout) != HAL_OK)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
	}
	LCD_SPI_CloseTransfer(hspi);   /* Call standard close procedure with error check */

	SET_BIT((hspi)->Instance->IFCR , SPI_IFCR_SUSPC);  // Clear the suspend flag


	/* Process Unlocked */
	__HAL_UNLOCK(hspi);

	hspi->State = HAL_SPI_STATE_READY;

	if (hspi->ErrorCode != HAL_SPI_ERROR_NONE)
	{
		return HAL_ERROR;
	}
	return errorcode;
}

/**
  * @brief  Modified for bulk data writes, allowing unlimited transfer length
  * @param  hspi   : SPI handle
  * @param  pData  : data to write
  * @param  Size   : data size
  * @retval HAL status
  */
HAL_StatusTypeDef LCD_SPI_TransmitBuffer (SPI_HandleTypeDef *hspi, uint16_t *pData, uint32_t Size)
{
   uint32_t    tickstart;  
   uint32_t    Timeout = 1000;      // Timeout check
   uint32_t    LCD_TxDataCount;     // transmit counter
   HAL_StatusTypeDef errorcode = HAL_OK;

	/* Check Direction parameter */
	assert_param(IS_SPI_DIRECTION_2LINES_OR_1LINE_2LINES_TXONLY(hspi->Init.Direction));

	/* Process Locked */
	__HAL_LOCK(hspi);

	/* Init tickstart for timeout management*/
	tickstart = HAL_GetTick();

	if (hspi->State != HAL_SPI_STATE_READY)
	{
		errorcode = HAL_BUSY;
		__HAL_UNLOCK(hspi);
		return errorcode;
	}

	if ( Size == 0UL)
	{
		errorcode = HAL_ERROR;
		__HAL_UNLOCK(hspi);
		return errorcode;
	}

	/* Set the transaction information */
	hspi->State       = HAL_SPI_STATE_BUSY_TX;
	hspi->ErrorCode   = HAL_SPI_ERROR_NONE;

	LCD_TxDataCount   = Size;                // transferred data length

	/*Init field not used in handle to zero */
	hspi->pRxBuffPtr  = NULL;
	hspi->RxXferSize  = (uint16_t) 0UL;
	hspi->RxXferCount = (uint16_t) 0UL;
	hspi->TxISR       = NULL;
	hspi->RxISR       = NULL;

	/* Configure communication direction : 1Line */
	if (hspi->Init.Direction == SPI_DIRECTION_1LINE)
	{
		SPI_1LINE_TX(hspi);
	}

// Hardware TSIZE control is not used; set to 0 here, so transfer data length is unlimited
	MODIFY_REG(hspi->Instance->CR2, SPI_CR2_TSIZE, 0);

	/* Enable SPI peripheral */
	__HAL_SPI_ENABLE(hspi);

	if (hspi->Init.Mode == SPI_MODE_MASTER)
	{
		 /* Master transfer start */
		 SET_BIT(hspi->Instance->CR1, SPI_CR1_CSTART);
	}

	/* Transmit data in 16 Bit mode */
	while (LCD_TxDataCount > 0UL)
	{
		/* Wait until TXP flag is set to send data */
		if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXP))
		{
			if ((LCD_TxDataCount > 1UL) && (hspi->Init.FifoThreshold > SPI_FIFO_THRESHOLD_01DATA))
			{
				*((__IO uint32_t *)&hspi->Instance->TXDR) = *((uint32_t *)pData);
				pData += 2;
				LCD_TxDataCount -= 2;
			}
			else
			{
				*((__IO uint16_t *)&hspi->Instance->TXDR) = *((uint16_t *)pData);
				pData += 1;
				LCD_TxDataCount--;
			}
		}
		else
		{
			/* Timeout management */
			if ((((HAL_GetTick() - tickstart) >=  Timeout) && (Timeout != HAL_MAX_DELAY)) || (Timeout == 0U))
			{
				/* Call standard close procedure with error check */
				LCD_SPI_CloseTransfer(hspi);

				/* Process Unlocked */
				__HAL_UNLOCK(hspi);

				SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_TIMEOUT);
				hspi->State = HAL_SPI_STATE_READY;
				return HAL_ERROR;
			}
		}
	}

	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_SR_TXC, RESET, tickstart, Timeout) != HAL_OK)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
	}

	SET_BIT((hspi)->Instance->CR1 , SPI_CR1_CSUSP); // Request SPI transfer suspend
	/* Wait for SPI suspend */
	if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_FLAG_SUSP, RESET, tickstart, Timeout) != HAL_OK)
	{
		SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);
	}
	LCD_SPI_CloseTransfer(hspi);   /* Call standard close procedure with error check */

	SET_BIT((hspi)->Instance->IFCR , SPI_IFCR_SUSPC);  // Clear the suspend flag


	/* Process Unlocked */
	__HAL_UNLOCK(hspi);

	hspi->State = HAL_SPI_STATE_READY;

	if (hspi->ErrorCode != HAL_SPI_ERROR_NONE)
	{
		return HAL_ERROR;
	}
	return errorcode;
}

// 11000 is a comfortable value
void lcd_bl_bright_set(uint16_t duty) {
	HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, duty % 0x10000);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
}
