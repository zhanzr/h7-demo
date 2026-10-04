#ifndef __DCMI_OV5640_H
#define __DCMI_OV5640_H

#include "stm32h7xx_hal.h"
#include "sccb.h"  
#include "usart.h"
#include "lcd_spi_200.h"

 // DCMI status flag; set to 1 by the HAL_DCMI_FrameEventCallback() interrupt callback when a frame transfer completes 
extern volatile uint8_t OV5640_FrameState;  // variable declaration for use by other files
extern volatile uint8_t OV5640_FPS ;      // frame rate

// Used to set the output format; referenced by OV5640_Set_Pixformat()
#define 	Pixformat_RGB565   0
#define 	Pixformat_JPEG     1
#define 	Pixformat_GRAY		 2

#define  OV5640_AF_Focusing     	2           // auto focus in progress
#define  OV5640_AF_End				1				// auto focus complete
#define  OV5640_Success   			0           // communication success flag
#define  OV5640_Error     			-1          // communication error

#define  OV5640_Enable    1
#define  OV5640_Disable   0


// OV5640 effect modes, referenced by OV5640_Set_Effect()
#define  OV5640_Effect_Normal       0  // normal mode
#define  OV5640_Effect_Negative     1  // negative mode, colors are inverted
#define  OV5640_Effect_BW           2  // black & white mode
#define  OV5640_Effect_Solarize  	3  // solarize mode

// 1. Define the actual output image size of the OV5640; adjust for the application or display
// 2. These two parameters do not affect the frame rate
// 3. Since the OV5640 ISP window ratio is 4:3 (1280x960), the output size must keep this ratio
// 4. To use another ratio, modify the initialization configuration
#define 	OV5640_Width          440   // image length 
#define 	OV5640_Height         330   // image width

// 1. Define the display frame size; the values must be divisible by 4!
// 2. In RGB565 format, DCMI crops the OV5640 4:3 image to fit the screen ratio
// 3. In JPEG mode, the values must be divisible by 8!
#define 	Display_Width          LCD_Width   	// image length 
#define 	Display_Height         LCD_Height   // image width

#define 	Display_BufferSize     Display_Width * Display_Height*2 /4   // DMA transfer size (32-bit wide)

/*------------------------------------------------------------ Common registers ------------------------------------------------*/

#define 	OV5640_ChipID_H          	0x300A  	// chip ID register, high byte
#define 	OV5640_ChipID_L          	0x300B  	// chip ID register, low byte

#define	OV5640_FORMAT_CONTROL		0x4300	// set data interface output format	
#define 	OV5640_FORMAT_CONTROL_MUX  0x501F	// set ISP format

#define	OV5640_JPEG_MODE_SELECT		0x4713	// JPEG mode selection, modes 1-6; see the datasheet
#define	OV5640_JPEG_VFIFO_CTRL00 	0x4600	// set whether JPEG mode 2 has fixed output width
#define	OV5640_JPEG_VFIFO_HSIZE_H	0x4602	// JPEG output horizontal size, high byte
#define	OV5640_JPEG_VFIFO_HSIZE_L	0x4603	// JPEG output horizontal size, low byte
#define	OV5640_JPEG_VFIFO_VSIZE_H	0x4604	// JPEG output vertical size, high byte
#define	OV5640_JPEG_VFIFO_VSIZE_L	0x4605	// JPEG output vertical size, low byte

#define 	OV5640_GroupAccess			0X3212	// register group access
#define 	OV5640_TIMING_DVPHO_H		0x3808	// output horizontal size, high byte
#define 	OV5640_TIMING_DVPHO_L		0x3809	// output horizontal size, low byte
#define 	OV5640_TIMING_DVPVO_H		0x380A	// output vertical size, high byte
#define 	OV5640_TIMING_DVPVO_L		0x380B   // output vertical size, low byte
#define 	OV5640_TIMING_Flip			0x3820	// Bits[2:1] set vertical flip
#define 	OV5640_TIMING_Mirror			0x3821	// Bits[2:1] set horizontal mirror

#define 	OV5640_AF_CMD_MAIN			0x3022	// AF main command
#define 	OV5640_AF_CMD_ACK				0x3023	// AF command acknowledge
#define 	OV5640_AF_FW_STATUS			0x3029	// focus status register

/*------------------------------------------------------------ Function declarations ------------------------------------------------*/

int8_t   DCMI_OV5640_Init(void);	// Initialize SCCB, DCMI, DMA and configure the OV5640

void     OV5640_DMA_Transmit_Continuous(uint32_t DMA_Buffer,uint32_t DMA_BufferSize);	// Start DMA transfer, continuous mode
void     OV5640_DMA_Transmit_Snapshot(uint32_t DMA_Buffer,uint32_t DMA_BufferSize);		//  Start DMA transfer, snapshot mode; stops after one frame
void     OV5640_DCMI_Suspend(void);		// Suspend DCMI, stop capturing data
void     OV5640_DCMI_Resume(void);		// Resume DCMI, start capturing data
void     OV5640_DCMI_Stop(void);			// Disable DCMI DMA requests, stop DCMI capture, disable the DCMI peripheral
int8_t 	OV5640_DCMI_Crop(uint16_t Displey_XSize,uint16_t Displey_YSize,uint16_t Sensor_XSize,uint16_t Sensor_YSize ); // Crop the image

void     OV5640_Reset(void);				//	Perform a software reset
uint16_t OV5640_ReadID(void);				// Read the device ID
void		OV5640_Config(void);				// Configure OV5640 parameters
	
void		OV5640_Set_Pixformat(uint8_t pixformat);					// Set image output format	
void 		OV5640_Set_JPEG_QuantizationScale(uint8_t scale);		// Set JPEG compression level, range 0x01-0x3F
int8_t 	OV5640_Set_Framesize(uint16_t width,uint16_t height);	// Set actual output image size
int8_t 	OV5640_Set_Horizontal_Mirror( int8_t ConfigState );	// Set whether the output image is horizontally mirrored
int8_t 	OV5640_Set_Vertical_Flip( int8_t ConfigState );			//	Set whether the output image is vertically flipped 
void 		OV5640_Set_Brightness(int8_t Brightness);					// Set brightness
void		OV5640_Set_Contrast(int8_t Contrast);						// Set contrast
void 		OV5640_Set_Effect(uint8_t effect_Mode);					// Set effects: normal, negative, etc.

int8_t 	OV5640_AF_Download_Firmware(void);		//	Download the auto-focus firmware into the OV5640
int8_t 	OV5640_AF_QueryStatus(void);				//	Query focus status
void 		OV5640_AF_Trigger_Constant(void);		// Auto focus, continuous trigger
void 		OV5640_AF_Trigger_Single(void);			// Auto focus, single trigger 
void 		OV5640_AF_Release(void);					// Release the motor; the lens returns to its initial (infinity focus) position

/*-------------------------------------------------------------- Pin configuration macros ---------------------------------------------*/

#define OV5640_PWDN_PIN            			 GPIO_PIN_14        				 	// PWDN pin      
#define OV5640_PWDN_PORT           			 GPIOD                 			 	// PWDN GPIO port     
#define GPIO_OV5640_PWDN_CLK_ENABLE    	__HAL_RCC_GPIOD_CLK_ENABLE() 		// PWDN GPIO port clock

// Low level: power-down mode off, camera works normally
#define	OV5640_PWDN_OFF	HAL_GPIO_WritePin(OV5640_PWDN_PORT, OV5640_PWDN_PIN, GPIO_PIN_RESET)	

// High level: power-down mode on, camera stops, minimal power consumption
#define 	OV5640_PWDN_ON		HAL_GPIO_WritePin(OV5640_PWDN_PORT, OV5640_PWDN_PIN, GPIO_PIN_SET)	
  
 
#endif //__DCMI_OV5640_H




