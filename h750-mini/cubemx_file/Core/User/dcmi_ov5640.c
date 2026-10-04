#include <stdio.h>

#include "dcmi_ov5640.h"  
#include "dcmi_ov5640_cfg.h"  

extern DCMI_HandleTypeDef   hdcmi;            // DCMI handle
DMA_HandleTypeDef    DMA_Handle_dcmi;  // DMA handle

volatile uint8_t OV5640_FrameState = 0;  // DCMI status flag; set to 1 by the HAL_DCMI_FrameEventCallback() interrupt callback when a frame transfer completes     
volatile uint8_t OV5640_FPS ;          // frame rate

/*****************************************************************************************************************************************
*	Function name:	HAL_DCMI_MspInit
*
*	Parameters:	hdcmi - variable of type DCMI_HandleTypeDef, i.e. the DCMI handle
*
*	Function:	Initialize the DCMI pins
*
*****************************************************************************************************************************************/
//void HAL_DCMI_MspInit(DCMI_HandleTypeDef* hdcmi)
//{
//   GPIO_InitTypeDef GPIO_InitStruct = {0};

//   if(hdcmi->Instance==DCMI)
//   {
//		__HAL_RCC_DCMI_CLK_ENABLE();		// Enable the DCMI peripheral clock

//		__HAL_RCC_GPIOE_CLK_ENABLE();// Enable the corresponding GPIO clocks
//		__HAL_RCC_GPIOD_CLK_ENABLE();
//		__HAL_RCC_GPIOB_CLK_ENABLE();
//		__HAL_RCC_GPIOC_CLK_ENABLE();
//		__HAL_RCC_GPIOA_CLK_ENABLE();
//		
//		GPIO_OV5640_PWDN_CLK_ENABLE;    // Enable the GPIO clock for the PWDN pin

///****************************************************************************  
//   Data pins                       Clock and sync pins
//    PC6     ------> DCMI_D0        PB7     ------> DCMI_VSYNC
//    PC7     ------> DCMI_D1	     PA4     ------> DCMI_HSYNC
//    PE0     ------> DCMI_D2        PA6  	 ------> DCMI_PIXCLK
//    PE1     ------> DCMI_D3	
//    PE4     ------> DCMI_D4	    SCCB control pins, initialized in sccb.c
//    PD3     ------> DCMI_D5		  PB8  ------> SCCB_SCL
//    PE5     ------> DCMI_D6	     PB9  ------> SCCB_SDA 
//    PE6     ------> DCMI_D7

//   Power-down control pin
//   PD14   ------> PWDN
//******************************************************************************/

//		GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_0|GPIO_PIN_5|GPIO_PIN_4
//								  |GPIO_PIN_6;
//		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//		GPIO_InitStruct.Pull = GPIO_NOPULL;
//		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//		GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
//		HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

//		GPIO_InitStruct.Pin = GPIO_PIN_3;
//		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//		GPIO_InitStruct.Pull = GPIO_NOPULL;
//		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//		GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
//		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

//		GPIO_InitStruct.Pin = GPIO_PIN_7;
//		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//		GPIO_InitStruct.Pull = GPIO_NOPULL;
//		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//		GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
//		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

//		GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_6;
//		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//		GPIO_InitStruct.Pull = GPIO_NOPULL;
//		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//		GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
//		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

//		GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_4;
//		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//		GPIO_InitStruct.Pull = GPIO_NOPULL;
//		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//		GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
//		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

//// Initialize the PWDN pin  
//		OV5640_PWDN_ON;	// High level: power-down mode on, camera stops, minimal power consumption

//		GPIO_InitStruct.Pin 		= OV5640_PWDN_PIN;				// PWDN pin
//		GPIO_InitStruct.Mode 	= GPIO_MODE_OUTPUT_PP;			// push-pull output mode
//		GPIO_InitStruct.Pull 	= GPIO_PULLUP;						// pull-up
//		GPIO_InitStruct.Speed 	= GPIO_SPEED_FREQ_LOW;			// low speed
//		HAL_GPIO_Init(OV5640_PWDN_PORT, &GPIO_InitStruct);	   // initialize  
//	}
//}

		
/***************************************************************************************************************************************
*	Function name: MX_DCMI_Init
*
*	Function: Configure the DCMI parameters
*
*	Description: 8-bit data mode, full data, full-frame capture, interrupts enabled		 			          
*
*****************************************************************************************************************************************/
void Test_MX_DCMI_Init(void)
{
   hdcmi.Instance                = DCMI;
   hdcmi.Init.SynchroMode        = DCMI_SYNCHRO_HARDWARE;      // Hardware sync mode, using external VS/HS signals for synchronization
   hdcmi.Init.PCKPolarity        = DCMI_PCKPOLARITY_RISING;    // Pixel clock rising edge active
   hdcmi.Init.VSPolarity         = DCMI_VSPOLARITY_LOW;        // VS active low
   hdcmi.Init.HSPolarity         = DCMI_HSPOLARITY_LOW;        // HS active low
   hdcmi.Init.CaptureRate        = DCMI_CR_ALL_FRAME;          // Capture rate, capture every frame
   hdcmi.Init.ExtendedDataMode   = DCMI_EXTEND_DATA_8B;        // 8-bit data mode
   hdcmi.Init.JPEGMode           = DCMI_JPEG_DISABLE;         	// DCMI JPEG mode not used
   hdcmi.Init.ByteSelectMode     = DCMI_BSM_ALL;               // DCMI interface captures all data  
   hdcmi.Init.ByteSelectStart    = DCMI_OEBS_ODD;              // Start byte selection, capture from the first data of a frame/line
   hdcmi.Init.LineSelectMode     = DCMI_LSM_ALL;               // Capture all lines
   hdcmi.Init.LineSelectStart    = DCMI_OELS_ODD;              // Start line selection, capture the first line after frame start
   HAL_DCMI_Init(&hdcmi) ;

   HAL_NVIC_SetPriority(DCMI_IRQn, 0 ,5);    // Set interrupt priority
   HAL_NVIC_EnableIRQ(DCMI_IRQn); 		      // Enable DCMI interrupt
}


/***************************************************************************************************************************************
*	Function name: OV5640_DMA_Init
*
*	Function: Configure the DMA parameters
*
*	Description: Uses DMA2, peripheral-to-memory mode, 32-bit data width, interrupts enabled			          
*
*****************************************************************************************************************************************/
void OV5640_DMA_Init(void)
{
   __HAL_RCC_DMA2_CLK_ENABLE();   // Enable the DMA2 clock

   DMA_Handle_dcmi.Instance                     = DMA2_Stream7;               // DMA2 stream 7      
   DMA_Handle_dcmi.Init.Request                 = DMA_REQUEST_DCMI;           // DMA request from DCMI
   DMA_Handle_dcmi.Init.Direction               = DMA_PERIPH_TO_MEMORY;       // Peripheral-to-memory mode
   DMA_Handle_dcmi.Init.PeriphInc               = DMA_PINC_DISABLE;           // Peripheral address increment disabled
   DMA_Handle_dcmi.Init.MemInc                  = DMA_MINC_ENABLE;			   // Memory address increment enabled
   DMA_Handle_dcmi.Init.PeriphDataAlignment     = DMA_PDATAALIGN_WORD;        // DCMI data width, 32 bits  
   DMA_Handle_dcmi.Init.MemDataAlignment        = DMA_MDATAALIGN_WORD;        // Memory data width, 32 bits
   DMA_Handle_dcmi.Init.Mode                    = DMA_CIRCULAR;               // Circular mode					
   DMA_Handle_dcmi.Init.Priority                = DMA_PRIORITY_LOW;           // Low priority
   DMA_Handle_dcmi.Init.FIFOMode                = DMA_FIFOMODE_ENABLE;        // Enable FIFO
   DMA_Handle_dcmi.Init.FIFOThreshold           = DMA_FIFO_THRESHOLD_FULL;    // Full FIFO mode, 4x32-bit size
   DMA_Handle_dcmi.Init.MemBurst                = DMA_MBURST_SINGLE;          // Single transfer
   DMA_Handle_dcmi.Init.PeriphBurst             = DMA_PBURST_SINGLE;          // Single transfer

   HAL_DMA_Init(&DMA_Handle_dcmi);                        // Configure DMA
   __HAL_LINKDMA(&hdcmi, DMA_Handle, DMA_Handle_dcmi);    // Link to the DCMI handle
	
   HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);         // Set interrupt priority
   HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);                 // Enable interrupt
}

/***************************************************************************************************************************************
*	Function name: OV5640_Delay
*	Parameters: Delay - delay time in ms
*	Function: Simple delay function, not very precise
*	Description: For portability, a software delay is used here; in real projects it can be replaced with an RTOS delay or the HAL delay
*****************************************************************************************************************************************/
void OV5640_Delay(uint32_t Delay)
{
	volatile uint16_t i;

	while (Delay --)				
	{
		for (i = 0; i < 40000; i++);
	}	
//	HAL_Delay(Delay);	  // HAL library delay can be used
}

/***************************************************************************************************************************************
*	Function name: DCMI_OV5640_Init
*
*	Function: Initialize SCCB, DCMI, DMA and configure the OV5640
*
*****************************************************************************************************************************************/
int8_t DCMI_OV5640_Init(void)
{
	uint16_t	Device_ID;
	
   SCCB_GPIO_Config();
	Test_MX_DCMI_Init(); 
   OV5640_DMA_Init();
	OV5640_Reset();	
	Device_ID =  OV5640_ReadID();	

	if( Device_ID == 0x5640 )	
	{
		printf ("OV5640 OK,ID:0x%X\r\n",Device_ID);

		OV5640_Config();
		OV5640_Set_Framesize(OV5640_Width,OV5640_Height);	
		// Crop the output image to fit the screen; not needed in JPEG mode
		OV5640_DCMI_Crop( Display_Width, Display_Height, OV5640_Width, OV5640_Height );
				
		return OV5640_Success;
	}
	else
	{
		printf ("OV5640 ERROR!!!!!  ID:%X\r\n",Device_ID);
		return  OV5640_Error;
	}	
}

/***************************************************************************************************************************************
*	Function name: OV5640_DMA_Transmit_Continuous
*
*	Parameters:  DMA_Buffer - DMA target address, i.e. the memory area used to store camera data
*            DMA_BufferSize - transfer data size, 32-bit wide
*
*	Function: Start a DMA transfer, continuous mode
*
*	Description: 1. In continuous mode, transfer runs until DCMI is suspended or stopped
*            2. In RGB565 mode, one pixel needs 2 bytes of storage
*				 3. Since the DMA transfer is 32-bit wide, DMA_BufferSize must be divided by 4, e.g.:
*               To capture a 240x240 image, 240*240*2 = 115200 bytes are needed,
*               so DMA_BufferSize = 115200 / 4 = 28800.
*ke
*****************************************************************************************************************************************/
void OV5640_DMA_Transmit_Continuous(uint32_t DMA_Buffer,uint32_t DMA_BufferSize)
{
   DMA_Handle_dcmi.Init.Mode  = DMA_CIRCULAR;  // Circular mode					

   HAL_DMA_Init(&DMA_Handle_dcmi);    // Configure DMA

  // Enable DCMI data capture, continuous capture mode
   HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)DMA_Buffer,DMA_BufferSize);
}

/***************************************************************************************************************************************
*	Function name: OV5640_DMA_Transmit_Snapshot
*
*	Parameters:  DMA_Buffer - DMA target address, i.e. the memory area used to store camera data
*            DMA_BufferSize - transfer data size, 32-bit wide
*
*	Function: Start a DMA transfer, snapshot mode, stops after one frame
*
*	Description: 1. Snapshot mode, only one frame is transferred
*            2. In RGB565 mode, one pixel needs 2 bytes of storage
*				 3. Since the DMA transfer is 32-bit wide, DMA_BufferSize must be divided by 4, e.g.:
*               To capture a 240x240 image, 240*240*2 = 115200 bytes are needed,
*               so DMA_BufferSize = 115200 / 4 = 28800.
*            4. After this mode completes, DCMI is suspended; call OV5640_DCMI_Resume() before starting another transfer
*
*****************************************************************************************************************************************/
void OV5640_DMA_Transmit_Snapshot(uint32_t DMA_Buffer,uint32_t DMA_BufferSize)
{
   DMA_Handle_dcmi.Init.Mode  = DMA_NORMAL;  // Normal mode					

   HAL_DMA_Init(&DMA_Handle_dcmi);    // Configure DMA

   HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)DMA_Buffer,DMA_BufferSize);
}

/***************************************************************************************************************************************
*	Function name: OV5640_DCMI_Suspend
*
*	Function: Suspend DCMI, stop capturing data
*
*	Description: 1. In continuous mode, calling this function stops DCMI data capture
*            2. Call OV5640_DCMI_Resume() to resume DCMI
*				 3. Note that while DCMI is suspended, the DMA keeps running
*
*****************************************************************************************************************************************/
void OV5640_DCMI_Suspend(void) 
{
   HAL_DCMI_Suspend(&hdcmi);    // Suspend DCMI
}

/***************************************************************************************************************************************
*	Function name: OV5640_DCMI_Resume
*
*	Function: Resume DCMI, start capturing data
*
*	Description: 1. When DCMI is suspended, call this function to resume
*            2. In snapshot mode via OV5640_DMA_Transmit_Snapshot(), DCMI is suspended after the transfer; call this function to resume capture before starting again
*
*****************************************************************************************************************************************/
void  OV5640_DCMI_Resume(void) 
{
   (&hdcmi)->State = HAL_DCMI_STATE_BUSY;       // Update the DCMI state flag
   (&hdcmi)->Instance->CR |= DCMI_CR_CAPTURE;   // Enable DCMI capture
}

/***************************************************************************************************************************************
*	Function name: OV5640_DCMI_Stop
*
*	Function: Disable DCMI DMA requests, stop DCMI capture, disable the DCMI peripheral
*
*****************************************************************************************************************************************/
void  OV5640_DCMI_Stop(void) 
{
   HAL_DCMI_Stop(&hdcmi);
}


/***************************************************************************************************************************************
*	Function name: OV5640_DCMI_Crop
*
*	Parameters:  Displey_XSize, Displey_YSize - display dimensions
*				  Sensor_XSize, Sensor_YSize - camera sensor output image dimensions
*
*	Function: Use the DCMI crop feature to crop the sensor output image to fit the screen
*
*	Description: 1. The camera's output aspect ratio may not match the display, so cropping is needed
*				 2. The output aspect ratio is set by the OV5640_Config() parameters; the final size is set by OV5640_Set_Framesize()
*            3. The DCMI horizontal active pixel count must also be divisible by 4!
*				 4. The function computes horizontal and vertical offsets to center the crop as much as possible
*****************************************************************************************************************************************/
int8_t OV5640_DCMI_Crop(uint16_t Displey_XSize,uint16_t Displey_YSize,uint16_t Sensor_XSize,uint16_t Sensor_YSize )
{
	uint16_t DCMI_X_Offset,DCMI_Y_Offset;	// horizontal and vertical offsets; vertical = number of lines, horizontal = number of pixel clocks (PCLK periods)
	uint16_t DCMI_CAPCNT;		// horizontal active pixels, i.e. pixel clock count (PCLK periods)
	uint16_t DCMI_VLINE;			// vertical active line count

	if( (Displey_XSize>=Sensor_XSize)|| (Displey_YSize>=Sensor_YSize) )
	{
//		printf("Actual display size is greater than or equal to the camera output size, exiting DCMI cropping\r\n");
		return OV5640_Error;  // If the actual display size is >= the camera output size, exit without cropping
	}
// In RGB565 format, the horizontal offset must be odd, otherwise colors are wrong:
// one valid pixel is 2 bytes and takes 2 PCLK periods, so capture must start at an odd position or data will be misaligned.
// Note that register values start from 0!
	DCMI_X_Offset = Sensor_XSize - Displey_XSize; // actual calculation is (Sensor_XSize - LCD_XSize)/2*2

// Compute the vertical offset to center the crop; the value represents the number of lines,	
	DCMI_Y_Offset = (Sensor_YSize - Displey_YSize)/2-1; // register values start from 0, so subtract 1

// Since one valid pixel is 2 bytes and takes 2 PCLK periods, multiply by 2
// The final register value must be divisible by 4!
	DCMI_CAPCNT = Displey_XSize*2-1;	// register values start from 0, so subtract 1
	
	DCMI_VLINE = Displey_YSize-1;		// vertical active line count
	
//	printf("%d  %d  %d  %d\r\n",DCMI_X_Offset,DCMI_Y_Offset,DCMI_CAPCNT,DCMI_VLINE);
	
	HAL_DCMI_ConfigCrop (&hdcmi,DCMI_X_Offset,DCMI_Y_Offset,DCMI_CAPCNT,DCMI_VLINE);// set crop window
	HAL_DCMI_EnableCrop(&hdcmi);		// enable cropping
	
	return OV5640_Success;
}

/***************************************************************************************************************************************
*	Function name: OV5640_Reset
*
*	Function: Perform a software reset
*
*	Description: Includes several delay operations          
*
*****************************************************************************************************************************************/
void OV5640_Reset(void)
{
	OV5640_Delay(30);  // Wait for the module to stabilize after power-up, at least 5 ms, then pull PWDN low  	
	
	OV5640_PWDN_OFF;  // PWDN pin outputs low level, power-down mode off, camera operates normally; the module's white LED turns on
  
// Per the OV5640 power-up sequence, after PWDN is pulled low, wait 1 ms before raising RESET. The module uses hardware RC reset lasting about 6-10 ms,
// so a delay is added to let the hardware reset finish and stabilize
	OV5640_Delay(5);    
	
// After reset completes, wait >= 20 ms before SCCB configuration
	OV5640_Delay(20);    
	
	SCCB_WriteReg_16Bit(0x3103, 0x11);	// Per the datasheet, before reset, use the clock input pin clock directly as the main clock
	SCCB_WriteReg_16Bit(0x3008, 0x82);	// Perform a software reset
	OV5640_Delay(5);  // delay 5 ms
	
}

/***************************************************************************************************************************************
*	Function name: OV5640_ReadID
*
*	Function: Read the OV5640 device ID
*
*****************************************************************************************************************************************/
uint16_t OV5640_ReadID(void)
{
   uint8_t PID_H,PID_L;     // ID variables
	
   PID_H = SCCB_ReadReg_16Bit(OV5640_ChipID_H); // read ID high byte
   PID_L = SCCB_ReadReg_16Bit(OV5640_ChipID_L); // read ID low byte
	
	return(PID_H<<8)|PID_L; // return the complete device ID
}

/***************************************************************************************************************************************
*	Function name: OV5640_Config
*
*	Function: Configure the OV5640 registers
*
*	Description: The parameters are defined in dcmi_ov5640_cfg.h
*            
*****************************************************************************************************************************************/

void OV5640_Config(void)
{
	uint32_t i;
	
	uint8_t	read_reg; 

	for(i=0; i<(sizeof(OV5640_INIT_Config)/4); i++)
	{
		SCCB_WriteReg_16Bit(OV5640_INIT_Config[i][0], OV5640_INIT_Config[i][1]); 
		
		read_reg = SCCB_ReadReg_16Bit(OV5640_INIT_Config[i][0]);

		if(OV5640_INIT_Config[i][1] != read_reg )
		{
			printf("read_reg:%u\n", i);
			printf("0x%x-0x%x-0x%x\r\n",OV5640_INIT_Config[i][0],OV5640_INIT_Config[i][1],read_reg);
		}
	}
}

/***************************************************************************************************************************************
*	Function name: OV5640_Set_Pixformat
*
*	Parameters:  pixformat - pixel format, select Pixformat_RGB565, Pixformat_GRAY or Pixformat_JPEG
*
*	Function: Set the output pixel format
*
*****************************************************************************************************************************************/

void OV5640_Set_Pixformat(uint8_t pixformat)
{
   uint8_t OV5640_Reg;  // register value

	if( pixformat == Pixformat_JPEG )
	{
		SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL, 		0x30);	//	set data interface output format	
		SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX, 	0x00);	// set ISP format	
 
		SCCB_WriteReg_16Bit(OV5640_JPEG_MODE_SELECT, 0x02);	 	// JPEG mode 2

		SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_CTRL00, 0xA0); 		// JPEG fixed line count
		
		SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_HSIZE_H, OV5640_Width>>8);			// JPEG output horizontal size, high byte
		SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_HSIZE_L, (uint8_t)OV5640_Width);	// JPEG output horizontal size, low byte
		SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_VSIZE_H, OV5640_Height>>8);			// JPEG output vertical size, high byte
		SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_VSIZE_L, (uint8_t)OV5640_Height);	// JPEG output vertical size, low byte	
		
	}
	else if( pixformat == Pixformat_GRAY )
	{
		SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL, 		0x10);	//	set data interface output format
		SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX, 	0x00);	// set ISP format		
	}
	else	// RGB565
	{
		SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL, 		0x6F);	// set RGB565 format here, order G[2:0]B[4:0], R[4:0]G[5:3]	
		SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX, 	0x01);	// set ISP format	
	}
	
   OV5640_Reg = SCCB_ReadReg_16Bit(0x3821);   // read register value; Bit[5] enables JPEG mode
	SCCB_WriteReg_16Bit(0x3821,(OV5640_Reg & 0xDF) | ((pixformat == Pixformat_JPEG) ? 0x20 : 0x00));
	 
   OV5640_Reg = SCCB_ReadReg_16Bit(0x3002);   // read register value; Bits[7], [4] and [2] enable VFIFO, JFIFO, JPG
	SCCB_WriteReg_16Bit(0x3002,(OV5640_Reg & 0xE3) | ((pixformat == Pixformat_JPEG) ? 0x00 : 0x1C));
	 
   OV5640_Reg = SCCB_ReadReg_16Bit(0x3006);   // read register value; Bits[5] and [3] enable the JPG clock
	SCCB_WriteReg_16Bit(0x3006,(OV5640_Reg & 0xD7) | ((pixformat == Pixformat_JPEG) ? 0x28 : 0x00));

}

/***************************************************************************************************************************************
*	Function name: OV5640_Set_JPEG_QuantizationScale
*
*	Parameters: scale - compression level, range 0x01-0x3F
*
*	Function: Higher values compress harder, producing smaller images but lower quality; adjust as needed
*
*****************************************************************************************************************************************/

void OV5640_Set_JPEG_QuantizationScale(uint8_t scale)
{
	SCCB_WriteReg_16Bit(0x4407, scale); 	// JPEG compression level
}


/***************************************************************************************************************************************
*	Function name: OV5640_Set_Framesize
*
*	Parameters:  width - actual output image length, height - actual output image width
*
*	Function: Set the actual output image size (after scaling)
*
*	Description: 1. The requested image width/height must keep the ISP window ratio from the init config, otherwise the image is distorted
*            2. A smaller output resolution does not necessarily mean a higher frame rate; the frame rate depends only on the init config (PLL, HTS and VTS)
*
*****************************************************************************************************************************************/

int8_t OV5640_Set_Framesize(uint16_t width,uint16_t height)
{
// Many OV5640 operations require this corresponding group configuration	
    SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X03);  	// start configuring group 3
	
    SCCB_WriteReg_16Bit(OV5640_TIMING_DVPHO_H,width>>8);			// DVPHO, set output horizontal size
    SCCB_WriteReg_16Bit(OV5640_TIMING_DVPHO_L,width&0xff);
    SCCB_WriteReg_16Bit(OV5640_TIMING_DVPVO_H,height>>8);		// DVPVO, set output vertical size
    SCCB_WriteReg_16Bit(OV5640_TIMING_DVPVO_L,height&0xff);

    SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X13);		// end configuration
    SCCB_WriteReg_16Bit(OV5640_GroupAccess,0Xa3);		// enable settings
	
	return OV5640_Success; 
}

/***************************************************************************************************************************************
*	Function name: OV5640_Set_Horizontal_Mirror
*
*	Parameters:  ConfigState - 1 mirrors the image horizontally, 0 restores normal
*
*	Function: Set whether the output image is horizontally mirrored
*
*****************************************************************************************************************************************/
int8_t OV5640_Set_Horizontal_Mirror( int8_t ConfigState )
{
   uint8_t OV5640_Reg;  // register value

   OV5640_Reg = SCCB_ReadReg_16Bit(OV5640_TIMING_Mirror);   // read register value

// Bits[2:1] set horizontal mirror
   if ( ConfigState == OV5640_Enable )    // if mirroring is enabled
   { 
      OV5640_Reg |= 0X06;  
   } 
   else                    // disable mirroring
   {
      OV5640_Reg &= 0xF9; 	
   }
   return  SCCB_WriteReg_16Bit(OV5640_TIMING_Mirror,OV5640_Reg);   // write register
}

/***************************************************************************************************************************************
*	Function name: OV5640_Set_Vertical_Flip
*
*	Parameters:  ConfigState - 1 flips the image vertically, 0 restores normal
*
*	Function: Set whether the output image is vertically flipped
*
*****************************************************************************************************************************************/
int8_t OV5640_Set_Vertical_Flip( int8_t ConfigState )
{
   uint8_t OV5640_Reg;  // register value

   OV5640_Reg = SCCB_ReadReg_16Bit(OV5640_TIMING_Flip);          // read register value

// Bits[2:1] set vertical flip
   if ( ConfigState == OV5640_Enable )   
   { 
		OV5640_Reg |= 0X06;       
   } 
   else   // disable flip
   {
      OV5640_Reg &= 0xF9; 	
   }
   return  SCCB_WriteReg_16Bit(OV5640_TIMING_Flip,OV5640_Reg);   // write register
}


/***************************************************************************************************************************************
*	Function name: OV5640_Set_Brightness
*
*	Parameters:  Brightness - brightness, 9 levels: 4, 3, 2, 1, 0, -1, -2, -3, -4; higher value = brighter                
*
*	Description: 1. Uses the code from the OV5640 datasheet directly
*            2. Higher brightness makes the image brighter but slightly blurrier
*				 3. Too-low brightness increases noise
*
*****************************************************************************************************************************************/
void OV5640_Set_Brightness(int8_t Brightness)
{
	Brightness = Brightness+4;
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X03);  	// start configuring group 3

	SCCB_WriteReg_16Bit( 0x5587, OV5640_Brightness_Config[Brightness][0]);	
	SCCB_WriteReg_16Bit( 0x5588, OV5640_Brightness_Config[Brightness][1]);
	
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X13);		// end configuration
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0Xa3);		// enable settings	
}

/***************************************************************************************************************************************
*	Function name: OV5640_Set_Contrast
*
*	Parameters: Contrast - contrast, 7 levels: 3, 2, 1, 0, -1, -2, -3                  
*
*	Description: 1. Uses the code from the OV5640 datasheet directly
*            2. Higher contrast makes the image sharper with more distinct blacks and whites
*
*****************************************************************************************************************************************/
void OV5640_Set_Contrast(int8_t Contrast)
{
	Contrast = Contrast+3;
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X03);  	// start configuring group 3

	SCCB_WriteReg_16Bit( 0x5586, OV5640_Contrast_Config[Contrast][0]);	
	SCCB_WriteReg_16Bit( 0x5585, OV5640_Contrast_Config[Contrast][1]);
	
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X13);		// end configuration
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0Xa3);		// enable settings	
}
/***************************************************************************************************************************************
*	Function name: OV5640_Set_Effect
*
*	Parameters:  effect_Mode - effect mode, select OV5640_Effect_Normal, OV5640_Effect_Negative,
*                          OV5640_Effect_BW or OV5640_Effect_Solarize
*
*	Function: Set OV5640 effects: normal, negative, black & white, solarize
*
*	Description: Only 4 modes are listed here; see the datasheet for more effect modes
*
*****************************************************************************************************************************************/
void OV5640_Set_Effect(uint8_t effect_Mode)
{
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X03);  	// start configuring group 3

	SCCB_WriteReg_16Bit( 0x5580, OV5640_Effect_Config[effect_Mode][0]);	
	SCCB_WriteReg_16Bit( 0x5583, OV5640_Effect_Config[effect_Mode][1]);
	SCCB_WriteReg_16Bit( 0x5584, OV5640_Effect_Config[effect_Mode][2]);	
	SCCB_WriteReg_16Bit( 0x5003, OV5640_Effect_Config[effect_Mode][3]);	
	
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0X13);		// end configuration
	SCCB_WriteReg_16Bit(OV5640_GroupAccess,0Xa3);		// enable settings		
	
}
/***************************************************************************************************************************************
*	Function name: OV5640_Download_AF_Firmware
*
*	Function: Download the auto-focus firmware into the OV5640
*
*	Description: The OV5640 has no internal flash, so the firmware cannot be saved and must be written on every power-up
*
*****************************************************************************************************************************************/

int8_t OV5640_AF_Download_Firmware(void)
{ 
	uint8_t  AF_Status = 0;		// focus status
	uint16_t i = 0; 				// counter variable
	uint16_t OV5640_MCU_Addr = 0x8000;	// OV5640 MCU memory start address 0x8000, size 4 KB
	
	SCCB_WriteReg_16Bit(0x3000, 0x20);	// Bit[5]: reset the MCU; required before writing the firmware
// Start writing the firmware, in bulk for speed
	SCCB_WriteBuffer_16Bit( OV5640_MCU_Addr,(uint8_t *)OV5640_AF_Firmware,sizeof(OV5640_AF_Firmware) );
	SCCB_WriteReg_16Bit(0x3000,0x00);  // Bit[5]: writing done, write 0 to enable the MCU
	
// After writing the firmware there is an initialization phase, so read the status up to 100 times and judge by it
	for(i=0;i<100;i++)	
	{
		AF_Status = SCCB_ReadReg_16Bit(OV5640_AF_FW_STATUS);	// read status register
		if( AF_Status == 0x7E)
		{
			printf("AF firmware initialzing......>>>\r\n");	
		}			
		if( AF_Status == 0x70)	// motor released, lens returns to initial (infinity) position, meaning firmware written successfully
		{
			printf("AF firmware write OK!\r\n");
			return OV5640_Success;  
		}			
	}
// After 100 reads, status 0x70 was not seen, so the firmware write failed	
	printf("Auto Focus firmware write error!!!\r\n");	
	return OV5640_Error;	
}  

/***************************************************************************************************************************************
*	Function name: OV5640_AF_QueryStatus
*
*	Return value: OV5640_AF_End - focus complete, OV5640_AF_Focusing - focusing
*
*	Function: Query the focus status
*
*	Description: 1. The focus process takes roughly 500+ ms
*				 2. Until focusing completes, the captured image is out of focus and very blurry
*
*****************************************************************************************************************************************/

int8_t OV5640_AF_QueryStatus(void)
{
	uint8_t  AF_Status = 0;		// focus status	
	
	AF_Status = SCCB_ReadReg_16Bit(OV5640_AF_FW_STATUS);	// read status register
	printf("AF_Status:0x%x\r\n",AF_Status);

// Single-focus mode returns 0x10, continuous-focus mode returns 0x20
	if( (AF_Status == 0x10)||(AF_Status == 0x20) )		
	{
		return OV5640_AF_End;	// return focus-complete flag
	}
	else
	{
		return OV5640_AF_Focusing;	// return focusing flag
	}
}

/***************************************************************************************************************************************
*	Function name: OV5640_AF_Trigger_Constant
*
*	Function: Trigger focusing continuously; when the OV5640 detects the image is out of focus it keeps focusing without user intervention
*
*	Description: 1. Call OV5640_AF_QueryStatus() to query the focus status
*				 2. Call OV5640_AF_Release() to exit continuous focus mode
*				 3. The focus process takes roughly 500+ ms
*				 4. In dim lighting the OV5640 may focus repeatedly; switch to single-focus mode as needed
*				
*****************************************************************************************************************************************/

void OV5640_AF_Trigger_Constant(void)
{
	SCCB_WriteReg_16Bit(0x3022,0x04);	//	continuous focusing
}

/***************************************************************************************************************************************
*	Function name: OV5640_AF_Trigger_Single
*
*	Function: Trigger a single auto-focus operation
*
*	Description: The focus process takes roughly 500+ ms; call OV5640_AF_QueryStatus() to query the status
*
*****************************************************************************************************************************************/

void OV5640_AF_Trigger_Single(void)
{
	SCCB_WriteReg_16Bit(OV5640_AF_CMD_MAIN,0x03);	// trigger a single auto-focus 
}

/***************************************************************************************************************************************
*	Function name: OV5640_AF_Release
*
*	Function: Release the motor; the lens returns to its initial (infinity focus) position
*
*****************************************************************************************************************************************/

void OV5640_AF_Release(void)
{
	SCCB_WriteReg_16Bit(OV5640_AF_CMD_MAIN,0x08);	// focus release command		
}

/***************************************************************************************************************************************
*	Function name: HAL_DCMI_FrameEventCallback
*
*	Function: Frame callback; entered after each frame is transferred
*
*	Description: After each frame, updates the flags and computes the frame rate
*****************************************************************************************************************************************/

void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
	static uint32_t DCMI_Tick = 0;         	// stores the current tick count
   static uint8_t  DCMI_Frame_Count = 0;   	// frame counter   

 	if(HAL_GetTick() - DCMI_Tick >= 1000)    // compute the frame rate every 1 s
	{
		DCMI_Tick = HAL_GetTick();        // refresh the current tick count
		
		OV5640_FPS = DCMI_Frame_Count;   // get the fps 

		DCMI_Frame_Count = 0;            // clear the counter
	}
	DCMI_Frame_Count ++;    // increment on each interrupt (each completed frame transfer)

   OV5640_FrameState = 1;  // set the transfer-complete flag
}

/***************************************************************************************************************************************
*	Function name: HAL_DCMI_ErrorCallback
*
*	Function: Error callback
*
*	Description: Entered on a DMA transfer error or FIFO overflow error
*****************************************************************************************************************************************/

void  HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi)
{
   // if( HAL_DCMI_GetError(hdcmi) == HAL_DCMI_ERROR_OVR)
   // {
   //    printf("FIFO overflow error!!!\r\n");
   // }
//   printf("error:0x%x!!!!\r\n",HAL_DCMI_GetError(hdcmi));
}

/*********************************************************************************************************************************************************************************************************************************************ke*************/
// 
