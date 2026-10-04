#include "sccb.h"  


/*****************************************************************************************
*	Function: SCCB_GPIO_Config
*	Input: None
*	Return: None
*	Description: Initialize the IIC GPIO pins, push-pull output
*	Note: Since the IIC communication speed is not high, an IO speed setting of 2M is sufficient here
******************************************************************************************/

void SCCB_GPIO_Config (void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	
	SCCB_SCL_CLK_ENABLE;	// Initialize the IO port clock
	SCCB_SDA_CLK_ENABLE;

	
	GPIO_InitStruct.Pin 			= SCCB_SCL_PIN;
	GPIO_InitStruct.Mode 		= GPIO_MODE_OUTPUT_OD;
//	GPIO_InitStruct.Mode 		= GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull 		= GPIO_NOPULL;
	GPIO_InitStruct.Speed 		= GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SCCB_SCL_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin 			= SCCB_SDA_PIN;				// SDA pin
	HAL_GPIO_Init(SCCB_SDA_PORT, &GPIO_InitStruct);		

//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull  = GPIO_PULLUP;

	HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_SET);		// SCL outputs high
	HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_SET);    // SDA outputs high

}

/*****************************************************************************************
*	Function: SCCB_Delay
*	Input: a - delay time
*	Return: None
*	Description: Simple delay function
*	Note: For portability and since delay precision is not critical, no timer is needed for the delay
******************************************************************************************/

void SCCB_Delay(uint32_t a)
{
	volatile uint16_t i;
	while (a --)				
	{
		for (i = 0; i < 100; i++) {
			__NOP();
		}
	}
}

/*****************************************************************************************
*	Function: SCCB_Start
*	Input: None
*	Return: None
*	Description: IIC start signal
*	Note: While SCL is high, SDA transitions from high to low to generate the start signal
******************************************************************************************/

void SCCB_Start(void)
{
	SCCB_SDA(1);		
	SCCB_SCL(1);
	SCCB_Delay(SCCB_DelayVaule);
	
	SCCB_SDA(0);
	SCCB_Delay(SCCB_DelayVaule);
	SCCB_SCL(0);
	SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
*	Function: SCCB_Stop
*	Input: None
*	Return: None
*	Description: IIC stop signal
*	Note: While SCL is high, SDA transitions from low to high to generate the stop signal
******************************************************************************************/

void SCCB_Stop(void)
{
	SCCB_SCL(0);
	SCCB_Delay(SCCB_DelayVaule);
	SCCB_SDA(0);
	SCCB_Delay(SCCB_DelayVaule);
	
	SCCB_SCL(1);
	SCCB_Delay(SCCB_DelayVaule);
	SCCB_SDA(1);
	SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
*	Function: SCCB_ACK
*	Input: None
*	Return: None
*	Description: IIC ACK signal
*	Note: While SCL is high, the SDA pin outputs low to produce the ACK signal
******************************************************************************************/

void SCCB_ACK(void)
{
	SCCB_SCL(0);
	SCCB_Delay(SCCB_DelayVaule);
	SCCB_SDA(0);
	SCCB_Delay(SCCB_DelayVaule);	
	SCCB_SCL(1);
	SCCB_Delay(SCCB_DelayVaule);
	
	SCCB_SCL(0);		// When SCL goes low, SDA should be pulled high immediately to release the bus
	SCCB_SDA(1);		
	
	SCCB_Delay(SCCB_DelayVaule);

}

/*****************************************************************************************
*	Function: SCCB_NoACK
*	Input: None
*	Return: None
*	Description: IIC NACK signal
*	Note: While SCL is high, if the SDA pin is high, a NACK signal is produced
******************************************************************************************/

void SCCB_NoACK(void)
{
	SCCB_SCL(0);	
	SCCB_Delay(SCCB_DelayVaule);
	SCCB_SDA(1);
	SCCB_Delay(SCCB_DelayVaule);
	SCCB_SCL(1);
	SCCB_Delay(SCCB_DelayVaule);
	
	SCCB_SCL(0);
	SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
*	Function: SCCB_WaitACK
*	Input: None
*	Return: None
*	Description: Wait for the receiving device to issue an ACK signal
*	Note: While SCL is high, if the SDA pin is detected low, the receiving device has responded normally
******************************************************************************************/

uint8_t SCCB_WaitACK(void)
{
	SCCB_SDA(1);
	SCCB_Delay(SCCB_DelayVaule);
	SCCB_SCL(1);
	SCCB_Delay(SCCB_DelayVaule);	
	
	if( HAL_GPIO_ReadPin(SCCB_SDA_PORT,SCCB_SDA_PIN) != 0) // Check whether the device has responded		
	{
		SCCB_SCL(0);	
		SCCB_Delay( SCCB_DelayVaule );		
		return ACK_ERR;	// No ACK
	}
	else
	{
		SCCB_SCL(0);	
		SCCB_Delay( SCCB_DelayVaule );		
		return ACK_OK;	// ACK OK
	}
}

/*****************************************************************************************
*	Function:	SCCB_WriteByte
*	Input:	IIC_Data - the 8-bit data to write
*	Return:	ACK_OK  - device responded normally
*          	   ACK_ERR - device response error
*	Description:	Write one byte of data
*	Note:   MSB first
******************************************************************************************/

uint8_t SCCB_WriteByte(uint8_t IIC_Data)
{
	uint8_t i;

	for (i = 0; i < 8; i++)
	{
		SCCB_SDA(IIC_Data & 0x80);
		
		SCCB_Delay( SCCB_DelayVaule );
		SCCB_SCL(1);
		SCCB_Delay( SCCB_DelayVaule );
		SCCB_SCL(0);		
		if(i == 7)
		{
			SCCB_SDA(1);			
		}
		IIC_Data <<= 1;
	}

	return SCCB_WaitACK(); // Wait for device response
}

/*****************************************************************************************
*	Function:	SCCB_ReadByte
*	Input:	ACK_Mode - ACK mode; 1 sends ACK, 0 sends NACK
*	Return:	ACK_OK  - device responded normally
*          	   ACK_ERR - device response error
*	Description:   Read one byte of data
*	Note:   1. MSB first
*				   2. A NACK signal should be sent when the host receives the last byte
******************************************************************************************/

uint8_t SCCB_ReadByte(uint8_t ACK_Mode)
{
	uint8_t IIC_Data = 0;
	uint8_t i = 0;
	
	for (i = 0; i < 8; i++)
	{
		IIC_Data <<= 1;
		
		SCCB_SCL(1);
		SCCB_Delay( SCCB_DelayVaule );
		IIC_Data |= (HAL_GPIO_ReadPin(SCCB_SDA_PORT,SCCB_SDA_PIN) & 0x01);	
		SCCB_SCL(0);
		SCCB_Delay( SCCB_DelayVaule );
	}
	
	if ( ACK_Mode == 1 )				//	ACK signal
		SCCB_ACK();
	else
		SCCB_NoACK();		 	// NACK signal
	
	return IIC_Data; 
}


/*************************************************************************************************************************************
*	Function:	SCCB_WriteHandle
*
*	Input:	addr - register to operate on (8-bit address)
*
*	Return:	SUCCESS - operation succeeded,ERROR	  - operation failed
*					
*	Description:	Perform write operation on the register (8-bit address), used by OV2640
************************************************************************************************************************************/

uint8_t SCCB_WriteHandle (uint8_t addr)
{
	uint8_t status;		// Status flag

	SCCB_Start();	// Start IIC communication
	if( SCCB_WriteByte(OV2640_DEVICE_ADDRESS) == ACK_OK ) // Write data command
	{
		if( SCCB_WriteByte((uint8_t)(addr)) != ACK_OK )
		{
			status = ERROR;	// operation failed
		}			
	}
	status = SUCCESS;	
//	if (HAL_OK == HAL_I2C_Master_Transmit(&hi2c1, OV2640_DEVICE_ADDRESS, &addr, 1, 100)) {
//		status = SUCCESS;	
//	} else {
//		status = ERROR;	
//	}

	return status;	
}

/*************************************************************************************************************************************
*	Function:	SCCB_WriteReg
*
*	Input:	addr - register to write (8-bit address), value - data to write
*					
*	Return:	SUCCESS - operation succeeded, ERROR	  - operation failed
*					
*	Description:	Write one byte to the register (8-bit address), used by OV2640
************************************************************************************************************************************/

uint8_t SCCB_WriteReg (uint8_t addr,uint8_t value)
{
	uint8_t status;
	
	SCCB_Start(); // Start IIC communication

	if( SCCB_WriteHandle(addr) == SUCCESS)	// Write the register to operate on
	{
		if (SCCB_WriteByte(value) != ACK_OK) // Write data
		{
			status = ERROR;						
		}
	}	
	SCCB_Stop(); // Stop communication
	
	status = SUCCESS;	// Write successful
	return status;
}
/*************************************************************************************************************************************
*	Function:	SCCB_ReadReg
*
*	Input:	addr - register to read (8-bit address)
*					
*	Return:	Data read
*					
*	Description:	Read one byte from the register (8-bit address), used by OV2640
************************************************************************************************************************************/

uint8_t SCCB_ReadReg (uint8_t addr)
{
   uint8_t value = 0;

	SCCB_Start();		// Start IIC communication

	if( SCCB_WriteHandle(addr) == SUCCESS) // Write the register to operate on
	{
      SCCB_Stop();	// Stop IIC communication
		SCCB_Start(); // Restart IIC communication

		if (SCCB_WriteByte(OV2640_DEVICE_ADDRESS|0X01) == ACK_OK)	// Send read command
		{	
			value = SCCB_ReadByte(0);	// Send a NACK signal when reading the last byte
		}					
		SCCB_Stop();	// Stop IIC communication

	}

	return value;	
}

/*************************************************************************************************************************************
*	Function:	SCCB_WriteHandle_16Bit
*
*	Input:	addr - register to operate on (16-bit address)
*
*	Return:	SUCCESS - operation succeeded,ERROR - operation failed
*					
*	Description:	Perform write operation on the register (16-bit address), used by OV5640
************************************************************************************************************************************/

uint8_t SCCB_WriteHandle_16Bit (uint16_t addr)
{
	uint8_t status;		// Status flag

	SCCB_Start();	// Start IIC communication
	if( SCCB_WriteByte(OV5640_DEVICE_ADDRESS) == ACK_OK ) // Write data command
	{
		if( SCCB_WriteByte((uint8_t)(addr >> 8)) == ACK_OK ) // Write 16-bit address
		{
			if( SCCB_WriteByte((uint8_t)(addr)) != ACK_OK )
			{
				status = ERROR;	// operation failed
			}			
		}		
	}
	status = SUCCESS;	// operation succeeded
	return status;	
}

/*************************************************************************************************************************************
*	Function:	SCCB_WriteReg_16Bit
*
*	Input:	addr - register to write (16-bit address)  value - data to write
*					
*	Return:	SUCCESS - operation succeeded,ERROR	  - operation failed
*					
*	Description:	Write one byte to the register (16-bit address), used by OV5640
************************************************************************************************************************************/

uint8_t SCCB_WriteReg_16Bit(uint16_t addr,uint8_t value)
{
	uint8_t status;
	
	SCCB_Start(); // Start IIC communication

	if( SCCB_WriteHandle_16Bit(addr) == SUCCESS)	// Write the register to operate on
	{
		if (SCCB_WriteByte(value) != ACK_OK) // Write data
		{
			status = ERROR;						
		}
	}	
	SCCB_Stop(); // Stop communication
	
	status = SUCCESS;	// Write successful
	return status;
}

/*************************************************************************************************************************************
*	Function:	SCCB_ReadReg_16Bit
*
*	Input:	addr - register to read (16-bit address)
*					
*	Return:	Data read
*					
*	Description:	Read one byte from the register (16-bit address), used by OV5640
************************************************************************************************************************************/

uint8_t SCCB_ReadReg_16Bit (uint16_t addr)
{
   uint8_t value = 0;

	SCCB_Start();		// Start IIC communication

	if( SCCB_WriteHandle_16Bit(addr) == SUCCESS) // Write the register to operate on
	{
      SCCB_Stop();	// Stop IIC communication
		SCCB_Start(); // Restart IIC communication

		if (SCCB_WriteByte(OV5640_DEVICE_ADDRESS|0X01) == ACK_OK)	// Send read command
		{	
			value = SCCB_ReadByte(0);	// Send a NACK signal when reading the last byte
		}					
		SCCB_Stop();	// Stop IIC communication

	}

	return value;	
}

/*************************************************************************************************************************************
*	Function:	SCCB_WriteBuffer_16Bit
*
*	Input:	addr - register to write (16-bit address)  *pData - data buffer   size - size of data to transfer
*					
*	Return:	SUCCESS - operation succeeded,ERROR	  - operation failed
*					
*	Description:	Write data in bulk to the register (16-bit address), used by OV5640 when writing autofocus firmware
************************************************************************************************************************************/

uint8_t SCCB_WriteBuffer_16Bit(uint16_t addr,uint8_t *pData, uint32_t size)
{
	uint8_t status;	
	uint32_t i;
	
	SCCB_Start(); // Start IIC communication

	if( SCCB_WriteHandle_16Bit(addr) == SUCCESS)	// Write the register to operate on
	{
		for(i=0;i<size;i++)
		{
			SCCB_WriteByte(*pData);// Write data			
			pData++;
		}
	}	
	SCCB_Stop(); // Stop communication
	
	status = SUCCESS;	// Write successful
	return status;
}
/********************************************************************************************/
