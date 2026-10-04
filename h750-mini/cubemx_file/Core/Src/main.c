/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dcmi.h"
#include "dma.h"
#include "quadspi.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#if defined(__ARMCC_VERSION) && !defined(__MICROLIB)
#include <rt_sys.h>
#endif
#include "custom_def.h"

#include "lcd_spi_200.h"
#include "dcmi_ov5640.h"  

#define Camera_Buffer	0x24000000
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if defined(__ICCARM__)
/* New definition from EWARM V9, compatible with EWARM8 */
int iar_fputc(int ch);
#define PUTCHAR_PROTOTYPE int iar_fputc(int ch)
#elif defined ( __CC_ARM ) || defined(__ARMCC_VERSION)
/* ARM Compiler 5/6*/
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#elif defined(__GNUC__)
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#endif /* __ICCARM__ */

PUTCHAR_PROTOTYPE {
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
	return (ch);
}

int stdout_putchar (int ch) {
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
	return (ch);
}

#if defined(__ARMCC_VERSION) && !defined(__MICROLIB)
/* ---------------------------------------------------------------------------
 * Full ARM C library (non-microLIB) retarget: printf -> USART1.
 * The full library routes stdout through the _sys_* layer (semihosting by
 * default), which is why _ttywrch hung. These overrides send to USART1.
 * ------------------------------------------------------------------------- */
__asm(".global __use_no_semihosting");

struct __FILE { int handle; }; /* completes stdio's FILE for the retarget */

FILE __stdin;
FILE __stdout;
FILE __stderr;

FILEHANDLE _sys_open(const char *name, int openmode)
{
  (void)openmode;
  if (strcmp(name, __stdin_name) == 0)  { return 0; }
  if (strcmp(name, __stdout_name) == 0) { return 1; }
  if (strcmp(name, __stderr_name) == 0) { return 2; }
  return -1;
}

int _sys_close(FILEHANDLE fh) { (void)fh; return 0; }

int _sys_read(FILEHANDLE fh, unsigned char *buf, unsigned len, int mode)
{
  (void)fh; (void)buf; (void)len; (void)mode;
  return 0;
}

int _sys_write(FILEHANDLE fh, const unsigned char *buf, unsigned len, int mode)
{
  (void)fh;
  (void)mode;
  if (len != 0U)
  {
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, HAL_MAX_DELAY);
  }
  return 0; /* all bytes written */
}

int _sys_istty(FILEHANDLE fh) { (void)fh; return 1; }

int _sys_seek(FILEHANDLE fh, long pos) { (void)fh; (void)pos; return -1; }

int _sys_ensure(FILEHANDLE fh) { (void)fh; return 0; }

long _sys_flen(FILEHANDLE fh) { (void)fh; return 0; }

int _sys_tmpnam2(char *name, int sig, unsigned maxlen)
{
  (void)name; (void)sig; (void)maxlen;
  return -1;
}

void _sys_exit(int returncode) { (void)returncode; while (1) { } }

char *_sys_command_string(char *cmd, int len) { (void)cmd; (void)len; return 0; }

/* Last-resort character output (avoids semihosting hang). */
void _ttywrch(int ch)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
}

int ferror(FILE *f) { (void)f; return 0; }
#endif /* !__MICROLIB */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* Note: stale I/D-cache lines from a debugger soft-reset are dropped in
     SystemInit(), which runs before __main's .data copy. */
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
#ifndef QSPI_APP
  MPU_Config();
#endif

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* Pre-program 4 flash wait states (needed for 480MHz SYSCLK) with a barrier so the
     latency set+readback inside HAL_RCC_ClockConfig can't race on the M7 device bus. */
#ifndef QSPI_APP
  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_4);
  __DSB();
  __ISB();
#endif
  /* USER CODE END Init */

  /* Configure the system clock */
#ifndef QSPI_APP
  SystemClock_Config();
#endif

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC3_Init();
#ifndef QSPI_APP
  MX_QUADSPI_Init();
#endif
  MX_SPI4_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_DCMI_Init();
  /* USER CODE BEGIN 2 */


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	//   MX_SDMMC1_SD_Init(); will blocks
//	  HAL_GPIO_WritePin(DCMI_PWDN_GPIO_Port, DCMI_PWDN_Pin, GPIO_PIN_SET);

	SPI_LCD_Init();
	
	LCD_DisplayString( 84 ,240,"Clk:");
	LCD_DisplayNumber( 132,240, SystemCoreClock/1000000,2);
	
	
//	HAL_GPIO_WritePin(DCMI_PWDN_GPIO_Port, DCMI_PWDN_Pin, GPIO_PIN_RESET);
//	HAL_GPIO_WritePin(DCMI_LEDEN_GPIO_Port, DCMI_LEDEN_Pin, GPIO_PIN_SET);
	
	int8_t ret_dcmi_init_val = DCMI_OV5640_Init();
	if(OV5640_Success != ret_dcmi_init_val) {
		printf("DCMI init failed.\n");
//		HAL_GPIO_WritePin(DCMI_PWDN_GPIO_Port, DCMI_PWDN_Pin, GPIO_PIN_SET);
//		HAL_GPIO_WritePin(DCMI_LEDEN_GPIO_Port, DCMI_LEDEN_Pin, GPIO_PIN_RESET);		
	} else {
		OV5640_AF_Download_Firmware();
		OV5640_AF_Trigger_Constant();	
		//	OV5640_AF_Trigger_Single();

		//	120/160 degree wide-angle lenses default to different
		//	orientations than the auto-focus lens; adjust per real usage.
		//	OV5640_Set_Vertical_Flip( OV5640_Disable );		// cancel vertical flip
		//	OV5640_Set_Horizontal_Mirror( OV5640_Enable );	// enable horizontal mirror

		OV5640_DMA_Transmit_Continuous(Camera_Buffer, Display_BufferSize); 
	}

	
	printf("H750 Test @ %u Hz\n", SystemCoreClock);
	printf("CC: %s\n", COMPILER_NAME);		
	printf("%u Hz, %08X, CM:%d, FPU_USED:%d\n",
			SystemCoreClock, SCB->CPUID,
			__CORTEX_M, __FPU_USED);
	printf("vector: %08X %08X\n", (uint32_t)(&stdout_putchar), (uint32_t)(&main));

  uint32_t tick_print = 0;
  while (1)
  {
		if ( OV5640_FrameState == 1 ) {		
			OV5640_FrameState = 0;
			LCD_CopyBuffer(0,0,Display_Width,Display_Height, (uint16_t *)Camera_Buffer);
			LCD_ShowTransparent(1);
			LCD_DisplayNumber( 84,240, OV5640_FPS,2);
		}

		if ((HAL_GetTick() - tick_print) >= 1000) {
			tick_print = HAL_GetTick();
			HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
			printf("%u Hz, %08X, CM:%d, FPU_USED:%d\n",
					SystemCoreClock, SCB->CPUID,
					__CORTEX_M, __FPU_USED);
		}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

	HAL_StatusTypeDef tmpStatus = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
  if (tmpStatus != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
