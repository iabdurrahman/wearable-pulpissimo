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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ads1118_stm32.h"
#include <stdio.h>
#include <string.h>

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
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
ads1118_config_t ads_cfg;

int16_t ads_raw;
float ads_voltage;

uint32_t lastRead = 0;

char uartBuf[128];

float battery_percentage = 0.0f;

/* Status baterai dari differential B+/B-/IN+ measurement */
float battery_voltage_diff = 0.0f;
float v_bminus = 0.0f;
float v_inplus = 0.0f;
int is_charging = 0;
int low_battery_alert = 0;
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
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2,
                      (uint8_t *)ptr,
                      len,
                      HAL_MAX_DELAY);

    return len;
}

float battery_percentage_calc(float voltage)
{
    if (voltage >= 4.20f)
        return 100.0f;

    if (voltage <= 3.00f)
        return 0.0f;

    return ((voltage - 3.00f) / (4.20f - 3.00f)) * 100.0f;
}
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

    ads1118_default_config(&ads_cfg);
    ads_cfg.hspi = &hspi2;
    ads_cfg.cs_port = GPIOC;
    ads_cfg.cs_pin  = GPIO_PIN_1;

    ads_cfg.vdivider_ratio          = 47.0f / 141.0f; /* divider B+ */
    ads_cfg.vdivider_ratio_bminus   = 47.0f / 141.0f; /* ISI SESUAI DIVIDER B- KAMU YANG SEBENARNYA */
    ads_cfg.vdivider_ratio_inplus   = 47.0f / 141.0f; /* ISI SESUAI DIVIDER IN+ KAMU YANG SEBENARNYA - JANGAN copy-paste 47/141 kalau itu bukan rasio fisik IN+ kamu! */

    ads_cfg.low_batt_threshold_v      = 3.3f;
    ads_cfg.charge_detect_threshold_v = 3.0f;

    if (ads1118_init(&ads_cfg) != ADS1118_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN 2 */
    /* USER CODE END 2 */
//  ads1118_debug_print_state();   /* <-- baris debug tambahan */
  /* USER CODE BEGIN 2 */
//  {
//      uint16_t cfg = 0xC383; /* OS_START=1, AIN0 vs GND, +-4.096V, single-shot, 128SPS */
//      uint16_t poll_cfg = cfg & (uint16_t)~0x8000; /* sama, tapi OS_START=0 (query only) */
//      uint8_t tx[4], rx[4];
//      uint8_t ptx[4], prx[4];
//      int i;
//
//      tx[0] = (uint8_t)(cfg >> 8); tx[1] = (uint8_t)(cfg & 0xFF);
//      tx[2] = tx[0]; tx[3] = tx[1];
//
//      ptx[0] = (uint8_t)(poll_cfg >> 8); ptx[1] = (uint8_t)(poll_cfg & 0xFF);
//      ptx[2] = ptx[0]; ptx[3] = ptx[1];
//
//      /* Trigger */
//      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
//      HAL_SPI_TransmitReceive(&hspi2, tx, rx, 4, HAL_MAX_DELAY);
//      HAL_StatusTypeDef hs = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 4, HAL_MAX_DELAY);
//      printf("HAL status=%d, SPI error code=0x%08lX\r\n", hs, HAL_SPI_GetError(&hspi2));
//      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
//      printf("Trigger : rx=%02X %02X %02X %02X\r\n", rx[0], rx[1], rx[2], rx[3]);
//
//      /* Poll 20x, tiap 5ms, print tiap iterasi */
//      for (i = 0; i < 20; i++)
//      {
//          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
//          HAL_SPI_TransmitReceive(&hspi2, ptx, prx, 4, HAL_MAX_DELAY);
//          HAL_StatusTypeDef hs = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 4, HAL_MAX_DELAY);
//          printf("HAL status=%d, SPI error code=0x%08lX\r\n", hs, HAL_SPI_GetError(&hspi2));
//          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
//
//          printf("Poll %2d : rx=%02X %02X %02X %02X  OS=%d\r\n",
//                 i, prx[0], prx[1], prx[2], prx[3], (prx[2] & 0x80) ? 1 : 0);
//
//          HAL_Delay(5);
//      }
//  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if ((HAL_GetTick() - lastRead) >= 15000)
      {
          lastRead = HAL_GetTick();
#if 0
          ads1118_status_t st = ads1118_read_raw_filtered(&ads_raw);
          if (st == ADS1118_OK)
          {
              ads_voltage = ads1118_raw_to_voltage(ads_raw, ads_cfg.pga) / ads_cfg.vdivider_ratio;
              battery_percentage = battery_percentage_calc(ads_voltage);

              printf("=====================================\r\n");
              printf("Time                : %lu s\r\n", HAL_GetTick() / 1000);
              printf("Raw ADC             : %d\r\n", ads_raw);
              printf("Battery Voltage     : %.3f V\r\n", ads_voltage);
              printf("Battery Percentage  : %.1f%%\r\n", battery_percentage);
              printf("=====================================\r\n\r\n");
          }
          else
          {
              printf("ADS1118 Read Failed! status=%d\r\n", (int)st);
          }
#endif
          ads1118_status_t st2 = ads1118_read_battery_status(&battery_voltage_diff, &v_bminus, &v_inplus,
                                                               &is_charging, &low_battery_alert);
          if (st2 == ADS1118_OK)
          {
        	  battery_percentage = battery_percentage_calc(battery_voltage_diff);

              printf("------------ Battery Status -----------\r\n");
              printf("Battery Voltage (B+-B-) 	: %.3f V\r\n", battery_voltage_diff);
              printf("V(B-)                   	: %.3f V\r\n", v_bminus);
              printf("V(IN+)                  	: %.3f V\r\n", v_inplus);
              printf("Battery Percentage  		: %.1f%%\r\n", battery_percentage);
              printf("Status Charge                : %s\r\n", is_charging ? "YA" : "TIDAK");
              printf("Low Battery Alert       	: %s\r\n", low_battery_alert ? "AKTIF" : "tidak");
              if (low_battery_alert) {
                  printf(">>> Charge Baterai!!! <<<\r\n");
              }
              printf("---------------------------------------\r\n\r\n");
          }
          else
          {
              printf("Battery Status Read Failed! status=%d\r\n", (int)st2);
          }

	      /* USER CODE END WHILE */

	      /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
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
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ADS1118_CS_GPIO_Port, ADS1118_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ADS1118_CS_Pin */
  GPIO_InitStruct.Pin = ADS1118_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ADS1118_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
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
