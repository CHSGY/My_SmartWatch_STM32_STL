/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED1_Flashlight_Pin GPIO_PIN_0
#define LED1_Flashlight_GPIO_Port GPIOA
#define BATTERY_ADC_Pin GPIO_PIN_2
#define BATTERY_ADC_GPIO_Port GPIOA
#define KEY3_Pin GPIO_PIN_4
#define KEY3_GPIO_Port GPIOA
#define KEY2_Pin GPIO_PIN_6
#define KEY2_GPIO_Port GPIOA
#define KEY1_Pin GPIO_PIN_1
#define KEY1_GPIO_Port GPIOB
#define MPU_SCL_Pin GPIO_PIN_10
#define MPU_SCL_GPIO_Port GPIOB
#define MPU_SDA_Pin GPIO_PIN_11
#define MPU_SDA_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_12
#define LED2_GPIO_Port GPIOB
#define LED3_Pin GPIO_PIN_13
#define LED3_GPIO_Port GPIOB
#define OLED_SCL_Pin GPIO_PIN_8
#define OLED_SCL_GPIO_Port GPIOB
#define OLED_SDA_Pin GPIO_PIN_9
#define OLED_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
/* 电源控制引脚别名（对应CubeMX中的LED2/LED3引脚命名） */
#define POWER_CONTROL_Pin       	LED3_Pin			/* PB13: MCU电源PMOS控制 */
#define POWER_CONTROL_GPIO_Port 	LED3_GPIO_Port
#define ADC_CONTROL_Pin         	LED2_Pin			/* PB12: ADC/高压PMOS控制 */
#define ADC_CONTROL_GPIO_Port   	LED2_GPIO_Port
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
