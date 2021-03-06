/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "assert.h"
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "stm32l476g_discovery_gyroscope.h"
char buffer[200];
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct{
	int line;
	int col;
} position_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
int buff_size = 16;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	position_t current_pos;
	current_pos.line = 0;
	current_pos.col = 0;
	#define THRESHOLD 10000
	float velocity[3]; // storage for angular velocity measurements (x,y,z) from L3GD20
	float angle[3]; // storage for integrated angles
	assert(BSP_GYRO_Init() == HAL_OK);
	int col = 0;
	int last_col = 0;
	int last_line = 0;
	int backwards = 0;
	int forward = 0;
	HAL_UART_Transmit(&huart2, (uint8_t *)"\e[H", 10, 10000);
//	while(1){
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[2J", 10, 10000);
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1C", 10, 10000);
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[2C", 10, 10000);
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1D", 10, 10000);
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1D", 10, 10000);
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[3C", 10, 10000);
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1D", 10, 10000);
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1D", 10, 10000);
//		HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1C", 10, 10000);
//	}
	while (1){
		// get a new reading
		BSP_GYRO_GetXYZ(velocity);

		// if motion on any axis exceeds a minimum threshold
		if(fabs(velocity[0])>THRESHOLD || fabs(velocity[1])>THRESHOLD || fabs(velocity[2])>THRESHOLD){
			HAL_UART_Transmit(&huart2, (uint8_t *)"\e[J", 5, 10000);
			HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1J", 6, 10000);

			// integrate motion if it exceeds threshold on THIS axis
			for(int ii=0; ii<3; ii++) { // for x, y, z...
				if(fabs(velocity[ii])>THRESHOLD)
				angle[ii] += velocity[ii]; // ... perform integration and print
			}

//			// print new result
//			sprintf(buffer, "%8d %8d %8d %8d %8d %8d \r\n",
//			(int)velocity[0], (int)velocity[1], (int)velocity[2],
//			(int)angle[0], (int)angle[1], (int)(angle[2]));

			if ((int)angle[1] > last_line){
				current_pos.line += 1;
//				char up_code[buff_size];
//				sprintf(up_code, "\e[%dA", (int)current_pos.line);
				HAL_UART_Transmit(&huart2, (uint8_t *)".\e[1A", 5, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)"o\e[1A", 5, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)"O\e[1A", 5, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1A", 5, 10000);
				last_line = (int)angle[1];
			}
			if ((int)angle[1] < last_line){
				current_pos.line = current_pos.line - 1;
//				char down_code[buff_size];
//				sprintf(down_code, "\e[%dB", (int)current_pos.line);
				HAL_UART_Transmit(&huart2, (uint8_t *)".\e[1B", 5, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)"o\e[1B", 5, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)"O\e[1B", 5, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)"\e[1B", 5, 10000);
				last_line = (int)angle[1];
			}
			if ((int)angle[0] < last_col){
				current_pos.col = current_pos.col - 1;
//				char left_code[buff_size];
//				sprintf(left_code, "\e[%dD", (int)current_pos.col);
				backwards = 1;
				last_col = (int)angle[0];
			}
			if ((int)angle[0] > last_col){
				current_pos.col += 1;
//				char right_code[buff_size];
//				sprintf(right_code, "\e[%dC", (int)current_pos.col);
				forward = 1;
				last_col = (int)angle[0];
			}
			if (backwards){
				HAL_UART_Transmit(&huart2, (uint8_t *)"\e[8D", 5, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)"Oo.", 3, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)"\e[4D", 5, 10000);
				backwards = 0;
			}
			if (forward){
				HAL_UART_Transmit(&huart2, (uint8_t *)"\e[5C", 5, 10000);
				HAL_UART_Transmit(&huart2, (uint8_t *)".oO", 3, 10000);
				forward = 0;
			}
			last_col = (int)angle[0];
			last_line = (int)angle[1];
			HAL_Delay(50);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_4BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GYRO_CS_GPIO_Port, GYRO_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : GYRO_CS_Pin */
  GPIO_InitStruct.Pin = GYRO_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GYRO_CS_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
