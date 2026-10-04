#ifndef __CAMERA_SCCB_H
#define __CAMERA_SCCB_H

#include "stm32h7xx_hal.h"

#define OV2640_DEVICE_ADDRESS     0x60    // OV2640 address
#define OV5640_DEVICE_ADDRESS     0X78		// OV5640 address

/*----------------------------------------- IIIC pin configuration macros -----------------------------------------------*/

#define SCCB_SCL_CLK_ENABLE       __HAL_RCC_GPIOB_CLK_ENABLE()		// SCL pin clock
#define SCCB_SCL_PORT   		   GPIOB                 				// SCL pin port
#define SCCB_SCL_PIN     		   GPIO_PIN_8 								// SCL pin
        
#define SCCB_SDA_CLK_ENABLE       __HAL_RCC_GPIOB_CLK_ENABLE() 	// SDA pin clock
#define SCCB_SDA_PORT   			 GPIOB                   			// SDA pin port
#define SCCB_SDA_PIN    			 GPIO_PIN_9              			// SDA pin

/*------------------------------------------ IIC related definitions -------------------------------------------------*/

#define ACK_OK  	1  			// Response OK
#define ACK_ERR 	0				// Response error

// SCCB communication delay, used by SCCB_Delay(),
//	communication speed around 300KHz
#define SCCB_DelayVaule  8  	

/*-------------------------------------------- IO port operations ---------------------------------------------------*/   

#define SCCB_SCL(a)	if (a)	\
										HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_SET); \
									else		\
										HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_RESET)	

#define SCCB_SDA(a)	if (a)	\
										HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_SET); \
									else		\
										HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_RESET)		

/*--------------------------------------------- function declarations --------------------------------------------------*/  		
					
void 		SCCB_GPIO_Config (void);				// IIC pin initialization
void 		SCCB_Delay(uint32_t a);					// IIC delay function						
void 		SCCB_Start(void);							// Start IIC communication
void 		SCCB_Stop(void);							// IIC stop signal
void 		SCCB_ACK(void);							//	Send ACK signal
void 		SCCB_NoACK(void);							// Send NACK signal
uint8_t 	SCCB_WaitACK(void);						//	Wait for ACK signal
uint8_t	SCCB_WriteByte(uint8_t IIC_Data); 	// Write byte function
uint8_t 	SCCB_ReadByte(uint8_t ACK_Mode);		// Read byte function
		
uint8_t  SCCB_WriteReg (uint8_t addr,uint8_t value);     	// Write one byte to the register (8-bit address), used by OV2640
uint8_t  SCCB_ReadReg (uint8_t addr);                    	// Read one byte from the register (8-bit address), used by OV2640
									
uint8_t 	SCCB_WriteReg_16Bit(uint16_t addr,uint8_t value);	// Write one byte to the register (16-bit address), used by OV5640									
uint8_t 	SCCB_ReadReg_16Bit (uint16_t addr);						// Read one byte from the register (16-bit address), used by OV5640
uint8_t 	SCCB_WriteBuffer_16Bit(uint16_t addr,uint8_t *pData, uint32_t size);	// Write data in bulk to the register (16-bit address), used by OV5640 when writing autofocus firmware		
									
#endif //__CAMERA_SCCB_H
