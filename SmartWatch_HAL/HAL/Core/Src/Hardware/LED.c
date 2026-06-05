/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : LED.c
  * @brief          : LED驱动源文件
  ******************************************************************************
  * @attention
  *
  * LED硬件连接：
  *   LED1 (Flashlight) - PA0
  *   LED2 - PB12
  *   LED3 (Power) - PB13
  *
  * 功能说明：
  *   - GPIO时钟和引脚配置由CubeMX的MX_GPIO_Init()完成
  *   - 提供LED1/LED2/LED3的开关和翻转控制
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Hardware/LED.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PT */

/* USER CODE END PT */

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
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  LED1开启
  * @param  无
  * @retval 无
  */
void LED1_ON(void)
{
  /* USER CODE BEGIN LED1_ON */
  HAL_GPIO_WritePin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin, GPIO_PIN_RESET);
  /* USER CODE END LED1_ON */
}

/**
  * @brief  LED1关闭
  * @param  无
  * @retval 无
  */
void LED1_OFF(void)
{
  /* USER CODE BEGIN LED1_OFF */
  HAL_GPIO_WritePin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin, GPIO_PIN_SET);
  /* USER CODE END LED1_OFF */
}

/**
  * @brief  LED1状态翻转
  * @param  无
  * @retval 无
  */
void LED1_Turn(void)
{
  /* USER CODE BEGIN LED1_Turn */
  if (HAL_GPIO_ReadPin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin) == GPIO_PIN_RESET)
  {
    HAL_GPIO_WritePin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin, GPIO_PIN_SET);
  }
  else
  {
    HAL_GPIO_WritePin(LED1_Flashlight_GPIO_Port, LED1_Flashlight_Pin, GPIO_PIN_RESET);
  }
  /* USER CODE END LED1_Turn */
}

/**
  * @brief  LED2开启
  * @param  无
  * @retval 无
  */
void LED2_ON(void)
{
  /* USER CODE BEGIN LED2_ON */
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
  /* USER CODE END LED2_ON */
}

/**
  * @brief  LED2关闭
  * @param  无
  * @retval 无
  */
void LED2_OFF(void)
{
  /* USER CODE BEGIN LED2_OFF */
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
  /* USER CODE END LED2_OFF */
}

/**
  * @brief  LED2状态翻转
  * @param  无
  * @retval 无
  */
void LED2_Turn(void)
{
  /* USER CODE BEGIN LED2_Turn */
  if (HAL_GPIO_ReadPin(LED2_GPIO_Port, LED2_Pin) == GPIO_PIN_RESET)
  {
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
  }
  else
  {
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
  }
  /* USER CODE END LED2_Turn */
}

/**
  * @brief  LED3开启（PB13高电平）
  * @param  无
  * @retval 无
  */
void LED3_ON(void)
{
  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
}

/**
  * @brief  LED3关闭（PB13低电平）
  * @param  无
  * @retval 无
  */
void LED3_OFF(void)
{
  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  LED3状态翻转
  * @param  无
  * @retval 无
  */
void LED3_Turn(void)
{
  if (HAL_GPIO_ReadPin(LED3_GPIO_Port, LED3_Pin) == GPIO_PIN_RESET)
  {
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
  }
  else
  {
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
